// WebSocket connection and message handling

import * as store from './store.js';
import { showNotification, debounce } from './utils.js';

let ws = null;
let reconnectAttempts = 0;
let maxReconnectAttempts = 10;
let reconnectDelay = 1000; // Start with 1 second
let maxReconnectDelay = 30000; // Max 30 seconds
let reconnectTimer = null;
let isManualDisconnect = false;

// Message handlers registry
const messageHandlers = new Map();

/**
 * Initialize WebSocket connection
 */
export function init() {
  connect();
}

/**
 * Connect to WebSocket server
 */
export function connect() {
  if (ws && (ws.readyState === WebSocket.CONNECTING || ws.readyState === WebSocket.OPEN)) {
    console.log('WebSocket already connected or connecting');
    return;
  }

  const wsProto = location.protocol === 'https:' ? 'wss' : 'ws';
  const wsUrl = `${wsProto}://${location.host}/ws/live`;
  
  console.log('Connecting to WebSocket:', wsUrl);
  ws = new WebSocket(wsUrl);
  
  ws.onopen = handleOpen;
  ws.onclose = handleClose;
  ws.onerror = handleError;
  ws.onmessage = handleMessage;
}

/**
 * Disconnect from WebSocket server
 */
export function disconnect() {
  isManualDisconnect = true;
  if (reconnectTimer) {
    clearTimeout(reconnectTimer);
    reconnectTimer = null;
  }
  
  if (ws) {
    ws.close();
  }
}

/**
 * Send a message through WebSocket
 * @param {Object} message - The message object to send
 * @returns {boolean} True if message was sent successfully
 */
export function send(message) {
  if (!ws || ws.readyState !== WebSocket.OPEN) {
    console.warn('WebSocket not connected, cannot send message:', message);
    return false;
  }
  
  try {
    const jsonMessage = typeof message === 'string' ? message : JSON.stringify(message);
    ws.send(jsonMessage);
    return true;
  } catch (error) {
    console.error('Failed to send WebSocket message:', error);
    return false;
  }
}

/**
 * Register a message handler for a specific message type
 * @param {string} type - The message type to handle
 * @param {Function} handler - The handler function
 */
export function registerHandler(type, handler) {
  if (!messageHandlers.has(type)) {
    messageHandlers.set(type, new Set());
  }
  messageHandlers.get(type).add(handler);
}

/**
 * Unregister a message handler
 * @param {string} type - The message type
 * @param {Function} handler - The handler function to remove
 */
export function unregisterHandler(type, handler) {
  if (messageHandlers.has(type)) {
    messageHandlers.get(type).delete(handler);
    if (messageHandlers.get(type).size === 0) {
      messageHandlers.delete(type);
    }
  }
}

/**
 * Get WebSocket connection state
 * @returns {string} Connection state
 */
export function getConnectionState() {
  if (!ws) return 'disconnected';
  
  switch (ws.readyState) {
    case WebSocket.CONNECTING: return 'connecting';
    case WebSocket.OPEN: return 'connected';
    case WebSocket.CLOSING: return 'closing';
    case WebSocket.CLOSED: return 'disconnected';
    default: return 'unknown';
  }
}

/**
 * Get WebSocket instance (for direct access if needed)
 * @returns {WebSocket|null} The WebSocket instance
 */
export function getWebSocket() {
  return ws;
}

// Event handlers
function handleOpen() {
  console.log('WebSocket connected');
  reconnectAttempts = 0;
  reconnectDelay = 1000;
  isManualDisconnect = false;
  
  const state = store.getState();
  store.update({
    connectionCount: state.connectionCount + 1,
    connectionStatus: 'connected'
  });
  
  // Request initial data
  requestSnapshot();
  requestFilterConfig();
  requestDbscanConfig();
}

function handleClose(event) {
  console.log('WebSocket closed:', event.code, event.reason);
  
  const state = store.getState();
  store.update({
    disconnectionCount: state.disconnectionCount + 1,
    connectionStatus: 'disconnected'
  });
  
  // Attempt reconnection if not manually disconnected
  if (!isManualDisconnect && reconnectAttempts < maxReconnectAttempts) {
    scheduleReconnect();
  } else if (reconnectAttempts >= maxReconnectAttempts) {
    showNotification('WebSocket connection failed after multiple attempts', 'error');
  }
}

function handleError(error) {
  console.error('WebSocket error:', error);
  
  const state = store.getState();
  store.update({
    errorCount: state.errorCount + 1,
    connectionStatus: 'error'
  });
}

function handleMessage(event) {
  try {
    const message = JSON.parse(event.data);
    store.set('lastReceiveTime', Date.now());
    
    // Dispatch to registered handlers first
    if (messageHandlers.has(message.type)) {
      const handlers = messageHandlers.get(message.type);
      handlers.forEach(handler => {
        try {
          handler(message);
        } catch (error) {
          console.error(`Error in message handler for ${message.type}:`, error);
        }
      });
    }
    
    // Handle core message types
    handleCoreMessage(message);
    
  } catch (error) {
    console.error('Failed to parse WebSocket message:', error);
  }
}

function handleCoreMessage(message) {
  const state = store.getState();
  
  switch (message.type) {
    case 'clusters-lite':
      store.update({
        lastSeq: message.seq,
        lastFrameTime: message.t || 0,
        clusterItems: message.items || []
      });
      store.updateFPS();
      break;
      
    case 'raw-lite':
      store.update({
        lastSeq: message.seq,
        lastFrameTime: message.t || 0,
        rawPoints: {
          xy: message.xy || [],
          sid: message.sid || []
        }
      });
      store.updateFPS();
      break;
      
    case 'filtered-lite':
      store.set('filteredPoints', {
        xy: message.xy || [],
        sid: message.sid || []
      });
      break;
      
    case 'sensor.snapshot':
      if (Array.isArray(message.sensors)) {
        store.clearSensors();
        for (const sensor of message.sensors) {
          store.setSensor(sensor.id, sensor);
        }
      }
      
      // Handle publishers/sinks in snapshot
      if (message.publishers && message.publishers.sinks) {
        store.set('sinks', message.publishers.sinks);
      }
      
      // Handle filter config in snapshot
      if (message.filter_config) {
        store.set('currentFilterConfig', message.filter_config);
      }
      
      // Handle world mask in snapshot
      if (message.world_mask) {
        store.set('worldMask', {
          include: message.world_mask.includes || [],
          exclude: message.world_mask.excludes || []
        });
      }
      break;
      
    case 'sensor.updated':
      if (message.sensor) {
        // Check if this update is from another client (not our own)
        const clientId = store.get('clientId');
        if (message.clientId && message.clientId !== clientId) {
          // Remote update: update sensor state
          store.setSensor(message.sensor.id, message.sensor);
        } else if (!message.clientId) {
          // Legacy update without clientId: treat as remote to be safe
          store.setSensor(message.sensor.id, message.sensor);
        }
        // If it's our own update (message.clientId === clientId), ignore it since we already applied optimistically
      }
      break;
      
    case 'filter.config':
      if (message.config) {
        store.set('currentFilterConfig', message.config);
      }
      break;
      
    case 'filter.updated':
      if (message.config) {
        store.set('currentFilterConfig', message.config);
      }
      break;
      
    case 'dbscan.config':
      if (message.config) {
        store.set('currentDbscanConfig', message.config);
      }
      break;
      
    case 'dbscan.updated':
      if (message.config) {
        store.set('currentDbscanConfig', message.config);
      }
      break;
      
    case 'world.updated':
      if (message.world_mask) {
        store.set('worldMask', {
          include: message.world_mask.includes || [],
          exclude: message.world_mask.excludes || []
        });
      }
      break;
      
    case 'ok':
      // Generic OK response
      console.log('Server OK:', message.ref || 'unknown');
      break;
      
    case 'error':
      console.error('Server error:', message.message || 'Unknown error');
      showNotification(message.message || 'Server error', 'error');
      break;
      
    default:
      console.log('Unhandled message type:', message.type, message);
  }
}

function scheduleReconnect() {
  if (reconnectTimer) {
    clearTimeout(reconnectTimer);
  }
  
  reconnectAttempts++;
  const delay = Math.min(reconnectDelay * Math.pow(2, reconnectAttempts - 1), maxReconnectDelay);
  
  console.log(`Scheduling reconnect attempt ${reconnectAttempts}/${maxReconnectAttempts} in ${delay}ms`);
  
  reconnectTimer = setTimeout(() => {
    console.log(`Reconnect attempt ${reconnectAttempts}/${maxReconnectAttempts}`);
    connect();
  }, delay);
}

// Request functions
function requestSnapshot() {
  send({ type: 'sensor.requestSnapshot' });
}

function requestFilterConfig() {
  send({ type: 'filter.requestConfig' });
}

function requestDbscanConfig() {
  send({ type: 'dbscan.requestConfig' });
}

// Utility functions for common operations
export function updateSensor(id, patch, clientId = null) {
  const message = {
    type: 'sensor.update',
    id: id,
    patch: patch
  };
  
  if (clientId) {
    message.clientId = clientId;
  }
  
  return send(message);
}

export function enableSensor(id, enabled) {
  return send({
    type: 'sensor.enable',
    id: id,
    enabled: enabled
  });
}

export function updateFilter(config) {
  return send({
    type: 'filter.update',
    config: config
  });
}

export function updateDbscan(config) {
  return send({
    type: 'dbscan.update',
    config: config
  });
}

export function updateWorldMask(worldMask) {
  return send({
    type: 'world.update',
    patch: {
      world_mask: {
        includes: worldMask.include,
        excludes: worldMask.exclude
      }
    }
  });
}

// Manual reconnection function
export function reconnect() {
  isManualDisconnect = false;
  reconnectAttempts = 0;
  reconnectDelay = 1000;
  
  if (ws) {
    ws.close();
  }
  
  setTimeout(() => {
    connect();
  }, 100);
}