// Centralized state management for the WebUI

import { showNotification } from './utils.js';

// Global state
let state = {
  // Connection state
  connectionStatus: 'connecting',
  connectionCount: 0,
  disconnectionCount: 0,
  errorCount: 0,
  
  // Client identification
  clientId: 'client_' + Math.random().toString(36).substr(2, 9) + '_' + Date.now(),
  
  // Frame data
  lastSeq: 0,
  lastFrameTime: 0,
  lastReceiveTime: 0,
  rawPoints: { xy: [], sid: [] },
  filteredPoints: { xy: [], sid: [] },
  clusterItems: [],
  
  // FPS tracking
  frameTimestamps: [],
  lastFPS: 0,
  
  // Performance settings
  performanceMode: false,
  maxPointsToRender: 10000,
  lastRenderTime: 0,
  targetFPS: 60,
  frameInterval: 1000 / 60,
  
  // Display state
  showRaw: true,
  showFiltered: true,
  perSensorColor: false,
  
  // Sensors
  sensors: new Map(),
  sensorColors: new Map(),
  
  // Sinks/Publishers
  sinks: [],
  
  // Viewport state
  viewport: {
    x: 0,      // pan offset x
    y: 0,      // pan offset y
    scale: 120  // pixels per meter (zoom)
  },
  
  // Interaction state
  isDragging: false,
  dragStart: { x: 0, y: 0 },
  dragMode: 'pan', // 'pan', 'sensor', 'roi'
  selectedSensor: null,
  selectedROI: null,
  selectedROIType: null, // 'include' or 'exclude'
  selectedVertex: null,
  roiEditMode: 'none', // 'none', 'create_include', 'create_exclude', 'edit'
  roiPoints: [], // temporary points during creation
  
  // ROI state
  worldMask: {
    include: [],
    exclude: []
  },
  
  // UI state preservation
  accordionStates: new Map(),
  focusState: null,
  
  // Configuration states
  currentFilterConfig: {
    prefilter: {
      enabled: true,
      neighborhood: { enabled: true, k: 5, r_base: 0.05, r_scale: 1.0 },
      spike_removal: { enabled: true, dr_threshold: 0.3, window_size: 3 },
      outlier_removal: { enabled: true, median_window: 5, outlier_threshold: 2.0 },
      intensity_filter: { enabled: false, min_intensity: 0, min_reliability: 0 },
      isolation_removal: { enabled: true, min_cluster_size: 3, isolation_radius: 0.1 }
    },
    postfilter: {
      enabled: true,
      isolation_removal: { enabled: true, min_points_size: 3, isolation_radius: 0.2, required_neighbors: 2 }
    }
  },
  
  currentDbscanConfig: {
    eps_norm: 2.5,
    minPts: 5,
    k_scale: 1.0,
    h_min: 0.01,
    h_max: 0.20,
    R_max: 5,
    M_max: 600
  }
};

// Subscribers for state changes
const subscribers = new Map();

/**
 * Initialize the store
 */
export function init() {
  console.log('Store initialized with client ID:', state.clientId);
}

/**
 * Get the current state
 * @returns {Object} The current state
 */
export function getState() {
  return state;
}

/**
 * Get a specific part of the state
 * @param {string} key - The state key to retrieve
 * @returns {any} The state value
 */
export function get(key) {
  return state[key];
}

/**
 * Set a specific part of the state
 * @param {string} key - The state key to set
 * @param {any} value - The value to set
 * @param {boolean} notify - Whether to notify subscribers (default: true)
 */
export function set(key, value, notify = true) {
  const oldValue = state[key];
  state[key] = value;
  
  if (notify && subscribers.has(key)) {
    const keySubscribers = subscribers.get(key);
    keySubscribers.forEach(callback => {
      try {
        callback(value, oldValue, key);
      } catch (error) {
        console.error('Error in state subscriber:', error);
      }
    });
  }
}

/**
 * Update multiple state values at once
 * @param {Object} updates - Object with key-value pairs to update
 * @param {boolean} notify - Whether to notify subscribers (default: true)
 */
export function update(updates, notify = true) {
  const oldValues = {};
  
  // Store old values and update state
  for (const [key, value] of Object.entries(updates)) {
    oldValues[key] = state[key];
    state[key] = value;
  }
  
  // Notify subscribers if requested
  if (notify) {
    for (const [key, value] of Object.entries(updates)) {
      if (subscribers.has(key)) {
        const keySubscribers = subscribers.get(key);
        keySubscribers.forEach(callback => {
          try {
            callback(value, oldValues[key], key);
          } catch (error) {
            console.error('Error in state subscriber:', error);
          }
        });
      }
    }
  }
}

/**
 * Subscribe to state changes for a specific key
 * @param {string} key - The state key to watch
 * @param {Function} callback - Function to call when the key changes
 * @returns {Function} Unsubscribe function
 */
export function subscribe(key, callback) {
  if (!subscribers.has(key)) {
    subscribers.set(key, new Set());
  }
  
  const keySubscribers = subscribers.get(key);
  keySubscribers.add(callback);
  
  // Return unsubscribe function
  return () => {
    keySubscribers.delete(callback);
    if (keySubscribers.size === 0) {
      subscribers.delete(key);
    }
  };
}

/**
 * Subscribe to multiple state keys
 * @param {Array<string>} keys - Array of state keys to watch
 * @param {Function} callback - Function to call when any key changes
 * @returns {Function} Unsubscribe function
 */
export function subscribeMultiple(keys, callback) {
  const unsubscribeFunctions = keys.map(key => subscribe(key, callback));
  
  // Return function that unsubscribes from all keys
  return () => {
    unsubscribeFunctions.forEach(unsubscribe => unsubscribe());
  };
}

/**
 * Get sensor by ID
 * @param {number} id - Sensor ID
 * @returns {Object|undefined} The sensor object
 */
export function getSensor(id) {
  return state.sensors.get(id);
}

/**
 * Set sensor data
 * @param {number} id - Sensor ID
 * @param {Object} sensorData - Sensor data
 */
export function setSensor(id, sensorData) {
  state.sensors.set(id, sensorData);
  // Notify subscribers of sensors change
  if (subscribers.has('sensors')) {
    const keySubscribers = subscribers.get('sensors');
    keySubscribers.forEach(callback => {
      try {
        callback(state.sensors, state.sensors, 'sensors');
      } catch (error) {
        console.error('Error in sensors subscriber:', error);
      }
    });
  }
}

/**
 * Remove sensor
 * @param {number} id - Sensor ID
 */
export function removeSensor(id) {
  const existed = state.sensors.delete(id);
  if (existed && subscribers.has('sensors')) {
    const keySubscribers = subscribers.get('sensors');
    keySubscribers.forEach(callback => {
      try {
        callback(state.sensors, state.sensors, 'sensors');
      } catch (error) {
        console.error('Error in sensors subscriber:', error);
      }
    });
  }
}

/**
 * Clear all sensors
 */
export function clearSensors() {
  state.sensors.clear();
  if (subscribers.has('sensors')) {
    const keySubscribers = subscribers.get('sensors');
    keySubscribers.forEach(callback => {
      try {
        callback(state.sensors, state.sensors, 'sensors');
      } catch (error) {
        console.error('Error in sensors subscriber:', error);
      }
    });
  }
}

/**
 * Get sensor color
 * @param {number} sid - Sensor ID
 * @returns {string} The color for the sensor
 */
export function getSensorColor(sid) {
  if (!state.sensorColors.has(sid)) {
    const colorPalette = [
      '#e74c3c', '#3498db', '#2ecc71', '#f39c12', '#9b59b6',
      '#1abc9c', '#e67e22', '#34495e', '#f1c40f', '#95a5a6',
      '#c0392b', '#2980b9', '#27ae60', '#d35400', '#8e44ad'
    ];
    const colorIndex = state.sensorColors.size % colorPalette.length;
    state.sensorColors.set(sid, colorPalette[colorIndex]);
  }
  return state.sensorColors.get(sid);
}

/**
 * Update FPS tracking
 */
export function updateFPS() {
  const now = performance.now();
  state.frameTimestamps.push(now);
  
  // Keep only last 30 frames for FPS calculation
  if (state.frameTimestamps.length > 30) {
    state.frameTimestamps.shift();
  }
  
  if (state.frameTimestamps.length >= 2) {
    const timeSpan = state.frameTimestamps[state.frameTimestamps.length - 1] - state.frameTimestamps[0];
    state.lastFPS = Math.round((state.frameTimestamps.length - 1) * 1000 / timeSpan);
  }
}

/**
 * Toggle performance mode
 */
export function togglePerformanceMode() {
  state.performanceMode = !state.performanceMode;
  showNotification(
    `Performance mode ${state.performanceMode ? 'enabled' : 'disabled'}. ${
      state.performanceMode ? 'Large point clouds will be optimized for better frame rate.' : 'Full quality rendering restored.'
    }`,
    'info'
  );
}

// Export state for direct access (read-only)
export { state as readonly };