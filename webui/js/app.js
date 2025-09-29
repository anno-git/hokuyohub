// Main application entry point - initializes all modules

import * as store from './store.js';
import * as ws from './ws.js';
import * as canvas from './canvas.js';
import * as utils from './utils.js';
import { showNotification } from './utils.js';
import { server as serverApi, getAuthToken, handleApiError, retryOperation } from './api.js';

// Import UI modules
import * as sensors from './sensors.js';
import * as filters from './filters.js';
import * as roi from './roi.js';
import * as dbscan from './dbscan.js';
import * as sinks from './sinks.js';
import * as configs from './configs.js';

// Change tracking system for unsaved changes detection
class ChangeTracker {
  constructor() {
    this.isDirty = false;
    this.changeListeners = [];
    this.originalFunctions = new Map();
    
    // Initialize change tracking
    this.init();
  }
  
  init() {
    console.log('Initializing ChangeTracker...');
    
    // Hook into existing update functions after modules are loaded
    setTimeout(() => {
      this.hookUpdateFunctions();
    }, 100);
  }
  
  hookUpdateFunctions() {
    // Hook sensor update functions
    this.hookModuleFunction('sensors', 'ws.enableSensor');
    this.hookModuleFunction('sensors', 'ws.updateSensor');
    
    // Hook filter update function (from filters.js)
    this.hookGlobalFunction('sendFilterConfig');
    
    // Hook DBSCAN update function (from dbscan.js) 
    this.hookGlobalFunction('sendDbscanConfig');
    
    // Hook config operations (from configs.js)
    this.hookGlobalFunction('saveCurrentConfig');
    
    console.log('ChangeTracker hooks installed');
  }
  
  hookModuleFunction(moduleName, funcPath) {
    try {
      const pathParts = funcPath.split('.');
      let obj = window;
      
      // Navigate to the function
      for (let i = 0; i < pathParts.length - 1; i++) {
        obj = obj[pathParts[i]];
        if (!obj) return;
      }
      
      const funcName = pathParts[pathParts.length - 1];
      const originalFunc = obj[funcName];
      
      if (typeof originalFunc === 'function') {
        obj[funcName] = (...args) => {
          this.setDirty();
          return originalFunc.apply(obj, args);
        };
        
        this.originalFunctions.set(funcPath, originalFunc);
        console.log(`Hooked ${funcPath}`);
      }
    } catch (error) {
      console.warn(`Failed to hook ${funcPath}:`, error);
    }
  }
  
  hookGlobalFunction(funcName) {
    try {
      // Try to find function in global scope or modules
      let targetObj = null;
      let originalFunc = null;
      
      // Check if it's a global function
      if (typeof window[funcName] === 'function') {
        targetObj = window;
        originalFunc = window[funcName];
      } else {
        // Check in HokuyoHub modules
        const modules = ['sensors', 'filters', 'dbscan', 'configs', 'ws'];
        for (const moduleName of modules) {
          const module = window.HokuyoHub?.[moduleName];
          if (module && typeof module[funcName] === 'function') {
            targetObj = module;
            originalFunc = module[funcName];
            break;
          }
        }
      }
      
      if (targetObj && originalFunc) {
        targetObj[funcName] = (...args) => {
          this.setDirty();
          return originalFunc.apply(targetObj, args);
        };
        
        this.originalFunctions.set(funcName, originalFunc);
        console.log(`Hooked global function ${funcName}`);
      }
    } catch (error) {
      console.warn(`Failed to hook global function ${funcName}:`, error);
    }
  }
  
  setDirty() {
    if (!this.isDirty) {
      this.isDirty = true;
      this.notifyChange();
      console.log('Configuration changes detected - marking as dirty');
    }
  }
  
  clearDirty() {
    if (this.isDirty) {
      this.isDirty = false;
      this.notifyChange();
      console.log('Configuration changes cleared - marking as clean');
    }
  }
  
  isDirtyState() {
    return this.isDirty;
  }
  
  notifyChange() {
    this.changeListeners.forEach(listener => {
      try {
        listener(this.isDirty);
      } catch (error) {
        console.error('Error in change listener:', error);
      }
    });
  }
  
  addChangeListener(listener) {
    this.changeListeners.push(listener);
  }
  
  removeChangeListener(listener) {
    const index = this.changeListeners.indexOf(listener);
    if (index > -1) {
      this.changeListeners.splice(index, 1);
    }
  }
  
  // Method to check for unsaved changes before restart
  async confirmUnsavedChanges() {
    if (!this.isDirty) return true;
    
    return new Promise((resolve) => {
      const message = 'You have unsaved configuration changes. Restarting the server will lose these changes. Continue anyway?';
      const confirmed = confirm(message);
      resolve(confirmed);
    });
  }
}

// Global change tracker instance
let changeTracker = null;

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
    
    // Initialize change tracking system
    changeTracker = new ChangeTracker();
    console.log('✓ Change tracking system initialized');
    
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
  
  // Server restart button
  const btnRestartServer = document.getElementById('btn-restart-server');
  if (btnRestartServer) {
    btnRestartServer.addEventListener('click', handleServerRestart);
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

/**
 * Handle server restart request
 */
async function handleServerRestart() {
  try {
    console.log('Server restart requested...');
    
    // Check for unsaved changes using the ChangeTracker
    if (changeTracker) {
      const canProceed = await changeTracker.confirmUnsavedChanges();
      if (!canProceed) {
        console.log('Server restart cancelled by user');
        return;
      }
    }
    
    // Disable the restart button to prevent multiple requests
    const btnRestartServer = document.getElementById('btn-restart-server');
    if (btnRestartServer) {
      btnRestartServer.disabled = true;
      btnRestartServer.textContent = 'Restarting...';
    }
    
    // Show notification
    showNotification('Initiating server restart...', 'info');
    
    // Get authentication token and make restart request
    const token = getAuthToken();
    
    try {
      // Make the restart request with retry logic
      await retryOperation(async () => {
        return serverApi.restart(token);
      }, 2, 500); // 2 retries with 500ms base delay
      
      console.log('Server restart request sent successfully');
      showNotification('Server restart initiated. Attempting to reconnect...', 'info');
      
      // Start reconnection process
      await handleServerReconnection();
      
    } catch (restartError) {
      console.error('Server restart request failed:', restartError);
      
      // Re-enable button on failure
      if (btnRestartServer) {
        btnRestartServer.disabled = false;
        btnRestartServer.textContent = 'Restart Server';
      }
      
      handleApiError(restartError, 'Server restart');
      throw restartError;
    }
    
  } catch (error) {
    console.error('Error during server restart process:', error);
    handleApiError(error, 'Server restart process');
  }
}

/**
 * Handle reconnection after server restart
 */
async function handleServerReconnection() {
  const MAX_RECONNECT_ATTEMPTS = 20;
  const INITIAL_DELAY = 3000; // 3 seconds initial delay
  const RECONNECT_DELAY = 2000; // 2 seconds between attempts
  const btnRestartServer = document.getElementById('btn-restart-server');
  
  let attempts = 0;
  
  // Close current WebSocket connection
  ws.disconnect();
  
  console.log('Starting reconnection process...');
  
  const checkConnection = () => {
    return new Promise((resolve) => {
      attempts++;
      console.log(`Reconnection attempt ${attempts}/${MAX_RECONNECT_ATTEMPTS}`);
      
      if (btnRestartServer) {
        btnRestartServer.textContent = `Reconnecting... (${attempts}/${MAX_RECONNECT_ATTEMPTS})`;
      }
      
      // Use the existing ws.reconnect() function which handles connection properly
      try {
        ws.reconnect();
        
        // Wait a moment for connection to establish
        setTimeout(() => {
          if (ws.getConnectionState() === 'connected') {
            console.log('Reconnection successful');
            showNotification('Server restart completed! Connection restored.', 'success');
            
            // Reset button
            if (btnRestartServer) {
              btnRestartServer.disabled = false;
              btnRestartServer.textContent = 'Restart Server';
            }
            
            // Clear dirty state since restart is complete
            if (changeTracker) {
              changeTracker.clearDirty();
            }
            
            // Request fresh data after a short delay
            setTimeout(() => {
              ws.send({ type: 'sensor.requestSnapshot' });
            }, 500);
            
            resolve(true);
          } else {
            // Connection not ready yet, try again
            if (attempts < MAX_RECONNECT_ATTEMPTS) {
              setTimeout(checkConnection, RECONNECT_DELAY);
            } else {
              console.error('Max reconnection attempts reached');
              showNotification('Server restart completed, but reconnection failed. Please refresh the page.', 'error');
              
              if (btnRestartServer) {
                btnRestartServer.disabled = false;
                btnRestartServer.textContent = 'Restart Server (Failed)';
              }
              resolve(false);
            }
          }
        }, 1000); // Give 1 second for connection to establish
        
      } catch (error) {
        console.warn(`Reconnection attempt ${attempts} failed:`, error);
        
        if (attempts < MAX_RECONNECT_ATTEMPTS) {
          setTimeout(checkConnection, RECONNECT_DELAY);
        } else {
          console.error('Max reconnection attempts reached');
          showNotification('Server restart completed, but reconnection failed. Please refresh the page.', 'error');
          
          if (btnRestartServer) {
            btnRestartServer.disabled = false;
            btnRestartServer.textContent = 'Restart Server (Failed)';
          }
          resolve(false);
        }
      }
    });
  };
  
  // Start reconnection after initial delay to allow server to fully restart
  setTimeout(checkConnection, INITIAL_DELAY);
}

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
  changeTracker: () => changeTracker, // Function to get current instance
  version: '2.0.0-refactored'
};

console.log('Hokuyo-hub WebUI v2.0.0-refactored loaded');