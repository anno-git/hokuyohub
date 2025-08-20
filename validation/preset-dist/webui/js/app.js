// Main application entry point - initializes all modules

import * as store from './store.js';
import * as ws from './ws.js';
import * as canvas from './canvas.js';
import * as utils from './utils.js';
import { showNotification } from './utils.js';

// Import UI modules
import * as sensors from './sensors.js';
import * as filters from './filters.js';
import * as roi from './roi.js';
import * as dbscan from './dbscan.js';
import * as sinks from './sinks.js';
import * as configs from './configs.js';

/**
 * Initialize the application
 */
async function bootstrap() {
  try {
    console.log('Initializing Hokuyo-hub WebUI...');
    
    // Initialize core modules in order
    store.init();
    console.log('✓ Store initialized');
    
    canvas.init();
    console.log('✓ Canvas initialized');
    
    // Initialize UI modules
    sensors.init();
    console.log('✓ Sensors module initialized');
    
    filters.init();
    console.log('✓ Filters module initialized');
    
    roi.init();
    console.log('✓ ROI module initialized');
    
    dbscan.init();
    console.log('✓ DBSCAN module initialized');
    
    sinks.init();
    console.log('✓ Sinks module initialized');
    
    configs.init();
    console.log('✓ Configs module initialized');
    
    // Set up global UI event handlers
    setupGlobalEventHandlers();
    console.log('✓ Global event handlers set up');
    
    // Initialize WebSocket connection last
    ws.init();
    console.log('✓ WebSocket connection initialized');
    
    // Set up periodic updates
    setupPeriodicUpdates();
    console.log('✓ Periodic updates set up');
    
    console.log('🚀 Hokuyo-hub WebUI initialized successfully');
    showNotification('WebUI initialized successfully', 'success');
    
  } catch (error) {
    console.error('Failed to initialize WebUI:', error);
    showNotification(`Failed to initialize WebUI: ${error.message}`, 'error');
  }
}

/**
 * Set up global event handlers
 */
function setupGlobalEventHandlers() {
  // Display controls
  const showRawCheckbox = document.getElementById('show-raw-points');
  const showFilteredCheckbox = document.getElementById('show-filtered-data');
  const perSensorColorsCheckbox = document.getElementById('per-sensor-colors');
  
  if (showRawCheckbox) {
    showRawCheckbox.addEventListener('change', () => {
      store.set('showRaw', showRawCheckbox.checked);
      updateLegend();
    });
  }
  
  if (showFilteredCheckbox) {
    showFilteredCheckbox.addEventListener('change', () => {
      store.set('showFiltered', showFilteredCheckbox.checked);
      updateLegend();
    });
  }
  
  if (perSensorColorsCheckbox) {
    perSensorColorsCheckbox.addEventListener('change', () => {
      store.set('perSensorColor', perSensorColorsCheckbox.checked);
      updateLegend();
    });
  }
  
  // Viewport controls
  const btnResetViewport = document.getElementById('btn-reset-viewport');
  if (btnResetViewport) {
    btnResetViewport.addEventListener('click', () => {
      canvas.resetViewport();
      updateViewportInfo();
    });
  }
  
  // Subscribe to state changes for UI updates
  store.subscribe('connectionStatus', updateConnectionStatus);
  store.subscribe('lastSeq', updateStatsDisplay);
  store.subscribe('clusterItems', updateStatsDisplay);
  store.subscribe('rawPoints', updateStatsDisplay);
  store.subscribe('filteredPoints', updateStatsDisplay);
  store.subscribe('lastFPS', updateStatsDisplay);
  store.subscribe('viewport', updateViewportInfo);
  store.subscribe('rawPoints', updateLegend);
  store.subscribe('filteredPoints', updateLegend);
  store.subscribe('perSensorColor', updateLegend);
  store.subscribe('showRaw', updateLegend);
  store.subscribe('showFiltered', updateLegend);
}

/**
 * Set up periodic updates
 */
function setupPeriodicUpdates() {
  // Update stats display every second
  setInterval(() => {
    updateStatsDisplay();
    updateLastSeqAge();
  }, 1000);
  
  // Update viewport info periodically
  setInterval(updateViewportInfo, 1000);
}

/**
 * Update connection status display
 */
function updateConnectionStatus() {
  const connectionStatus = document.getElementById('connection-status');
  if (!connectionStatus) return;
  
  const status = store.get('connectionStatus');
  connectionStatus.textContent = status;
  
  // Apply CSS classes based on status
  connectionStatus.className = '';
  if (status === 'connected') {
    connectionStatus.classList.add('connected');
  } else if (status === 'connecting') {
    connectionStatus.classList.add('connecting');
  } else if (status === 'error') {
    connectionStatus.classList.add('error');
  } else {
    connectionStatus.classList.add('disconnected');
  }
}

/**
 * Update stats display
 */
function updateStatsDisplay() {
  const state = store.getState();
  
  // Update sequence info
  const seqInfo = document.getElementById('seq-info');
  if (seqInfo) {
    const rawCount = state.rawPoints.xy.length / 2;
    const filteredCount = state.filteredPoints.xy.length / 2;
    seqInfo.textContent = `seq=${state.lastSeq} clusters=${state.clusterItems.length} raw=${rawCount} filtered=${filteredCount}`;
  }
  
  // Update FPS info
  const fpsInfo = document.getElementById('fps-info');
  if (fpsInfo) {
    fpsInfo.textContent = `fps=${state.lastFPS}`;
  }
}

/**
 * Update last sequence age display
 */
function updateLastSeqAge() {
  const lastSeqAgeEl = document.getElementById('last-seq-age');
  if (!lastSeqAgeEl) return;
  
  const state = store.getState();
  
  if (state.lastFrameTime > 0 && state.lastReceiveTime > 0) {
    const ageMs = Date.now() - state.lastReceiveTime;
    const ageSec = (ageMs / 1000).toFixed(1);
    lastSeqAgeEl.textContent = `age=${ageSec}s`;
    
    // Apply warning/error classes
    lastSeqAgeEl.className = '';
    if (ageMs > 3000) {
      lastSeqAgeEl.classList.add('error');
    } else if (ageMs > 1000) {
      lastSeqAgeEl.classList.add('warn');
    }
  } else {
    lastSeqAgeEl.textContent = 'age=--';
    lastSeqAgeEl.className = '';
  }
}

/**
 * Update viewport info display
 */
function updateViewportInfo() {
  const viewportInfo = document.getElementById('viewport-info');
  if (!viewportInfo) return;
  
  const viewport = store.get('viewport');
  const scale = Math.round(viewport.scale);
  const metersPerPixel = (1 / viewport.scale).toFixed(3);
  viewportInfo.textContent = `Scale: ${scale}px/m (${metersPerPixel}m/px)`;
}

/**
 * Update legend display
 */
function updateLegend() {
  const legend = document.getElementById('legend');
  const legendItems = document.getElementById('legend-items');
  
  if (!legend || !legendItems) return;
  
  const state = store.getState();
  
  if (!state.perSensorColor) {
    legend.hidden = true;
    return;
  }
  
  // Get unique sensor IDs from both raw and filtered points
  const allSids = [];
  if (state.showRaw && state.rawPoints.sid.length > 0) {
    allSids.push(...state.rawPoints.sid);
  }
  if (state.showFiltered && state.filteredPoints.sid.length > 0) {
    allSids.push(...state.filteredPoints.sid);
  }
  
  const uniqueSids = [...new Set(allSids)].sort((a, b) => a - b);
  
  if (uniqueSids.length === 0) {
    legend.hidden = true;
    return;
  }
  
  legend.hidden = false;
  legendItems.innerHTML = '';
  
  for (const sid of uniqueSids) {
    const item = document.createElement('div');
    item.className = 'legend-item';
    
    const swatch = document.createElement('div');
    swatch.className = 'legend-swatch';
    swatch.style.backgroundColor = store.getSensorColor(sid);
    
    const label = document.createElement('span');
    label.textContent = `Sensor ${sid}`;
    
    item.appendChild(swatch);
    item.appendChild(label);
    legendItems.appendChild(item);
  }
}

/**
 * Handle application errors
 */
function handleError(error, context = 'Application') {
  console.error(`${context} error:`, error);
  showNotification(`${context} error: ${error.message}`, 'error');
}

/**
 * Handle unhandled promise rejections
 */
window.addEventListener('unhandledrejection', (event) => {
  handleError(event.reason, 'Unhandled Promise');
  event.preventDefault();
});

/**
 * Handle global errors
 */
window.addEventListener('error', (event) => {
  handleError(event.error || new Error(event.message), 'Global');
});

/**
 * Handle page visibility changes
 */
document.addEventListener('visibilitychange', () => {
  if (document.hidden) {
    console.log('Page hidden - reducing activity');
  } else {
    console.log('Page visible - resuming normal activity');
    // Request fresh data when page becomes visible again
    if (ws.getConnectionState() === 'connected') {
      ws.send({ type: 'sensor.requestSnapshot' });
    }
  }
});

/**
 * Handle page unload
 */
window.addEventListener('beforeunload', () => {
  console.log('Page unloading - cleaning up');
  ws.disconnect();
});

// Initialize the application when DOM is ready
if (document.readyState === 'loading') {
  document.addEventListener('DOMContentLoaded', bootstrap);
} else {
  bootstrap();
}

// Export for debugging purposes
window.HokuyoHub = {
  store,
  ws,
  canvas,
  utils,
  version: '2.0.0-refactored'
};

console.log('Hokuyo-hub WebUI v2.0.0-refactored loaded');