// DBSCAN configuration UI management

import * as store from './store.js';
import * as ws from './ws.js';
import { setPanelMessage, debounce } from './utils.js';
import { createDefaultDbscanConfig } from './types.js';

// UI elements
let dbscanElements = {};
let dbscanMsg = null;

// Debounced update function
let debouncedSendConfig = null;

/**
 * Initialize DBSCAN module
 */
export function init() {
  // Get all DBSCAN UI elements
  dbscanElements = {
    epsNorm: document.getElementById('dbscan-eps-norm'),
    minPts: document.getElementById('dbscan-min-pts'),
    kScale: document.getElementById('dbscan-k-scale'),
    hMin: document.getElementById('dbscan-h-min'),
    hMax: document.getElementById('dbscan-h-max'),
    rMax: document.getElementById('dbscan-r-max'),
    mMax: document.getElementById('dbscan-m-max'),
    msg: document.getElementById('dbscan-msg')
  };
  
  dbscanMsg = dbscanElements.msg;
  
  // Create debounced send function
  debouncedSendConfig = debounce(sendDbscanConfig, 500);
  
  // Set up event handlers
  setupEventHandlers();
  
  // Set up accordion functionality
  setupDbscanAccordion();
  
  // Subscribe to state changes
  store.subscribe('currentDbscanConfig', applyDbscanConfigToUI);
  
  // Register WebSocket message handlers
  ws.registerHandler('dbscan.config', handleDbscanConfig);
  ws.registerHandler('dbscan.updated', handleDbscanUpdated);
  
  console.log('DBSCAN module initialized');
}

/**
 * Set up event handlers
 */
function setupEventHandlers() {
  const parameterInputs = [
    dbscanElements.epsNorm,
    dbscanElements.minPts,
    dbscanElements.kScale,
    dbscanElements.hMin,
    dbscanElements.hMax,
    dbscanElements.rMax,
    dbscanElements.mMax
  ];
  
  parameterInputs.forEach(input => {
    if (input) {
      input.addEventListener('input', debouncedSendConfig);
      input.addEventListener('change', sendDbscanConfig);
      input.addEventListener('blur', sendDbscanConfig);
    }
  });
}

/**
 * Set up DBSCAN accordion functionality
 */
function setupDbscanAccordion() {
  const dbscanPanel = document.getElementById('dbscan-panel');
  if (!dbscanPanel) return;
  
  const header = dbscanPanel.querySelector('h3[role="button"]');
  const content = document.getElementById('dbscan-content');
  
  if (!header || !content) return;
  
  // Toggle function
  const toggleAccordion = () => {
    const isCollapsed = content.classList.toggle('collapsed');
    header.setAttribute('aria-expanded', isCollapsed ? 'false' : 'true');
    
    // Update caret direction
    const caret = header.querySelector('.toggle-caret');
    if (caret) {
      caret.textContent = isCollapsed ? '▶' : '▼';
    }
  };
  
  // Click handler
  header.addEventListener('click', (e) => {
    toggleAccordion();
  });
  
  // Keyboard handler
  header.addEventListener('keydown', (e) => {
    if (e.key === ' ' || e.key === 'Enter') {
      e.preventDefault();
      toggleAccordion();
    }
  });
}

/**
 * Load DBSCAN configuration from UI elements
 * @returns {Object} DBSCAN configuration
 */
function loadDbscanConfigFromUI() {
  return {
    eps_norm: parseFloat(dbscanElements.epsNorm?.value ?? 2.5),
    minPts: parseInt(dbscanElements.minPts?.value ?? 5),
    k_scale: parseFloat(dbscanElements.kScale?.value ?? 1.0),
    h_min: parseFloat(dbscanElements.hMin?.value ?? 0.01),
    h_max: parseFloat(dbscanElements.hMax?.value ?? 0.20),
    R_max: parseInt(dbscanElements.rMax?.value ?? 5),
    M_max: parseInt(dbscanElements.mMax?.value ?? 600)
  };
}

/**
 * Apply DBSCAN configuration to UI elements
 * @param {Object} config - DBSCAN configuration
 */
function applyDbscanConfigToUI(config) {
  if (!config) return;
  
  if (dbscanElements.epsNorm) dbscanElements.epsNorm.value = config.eps_norm;
  if (dbscanElements.minPts) dbscanElements.minPts.value = config.minPts;
  if (dbscanElements.kScale) dbscanElements.kScale.value = config.k_scale;
  if (dbscanElements.hMin) dbscanElements.hMin.value = config.h_min;
  if (dbscanElements.hMax) dbscanElements.hMax.value = config.h_max;
  if (dbscanElements.rMax) dbscanElements.rMax.value = config.R_max;
  if (dbscanElements.mMax) dbscanElements.mMax.value = config.M_max;
}

/**
 * Send DBSCAN configuration to server
 */
function sendDbscanConfig() {
  const config = loadDbscanConfigFromUI();
  store.set('currentDbscanConfig', config);
  
  if (ws.updateDbscan(config)) {
    setDbscanMessage('DBSCAN configuration updated');
  } else {
    setDbscanMessage('Failed to send DBSCAN configuration', true);
  }
}

/**
 * Set DBSCAN message
 * @param {string} text - Message text
 * @param {boolean} isError - Whether this is an error message
 */
function setDbscanMessage(text, isError = false) {
  setPanelMessage(dbscanMsg, text, isError, 3000);
}

/**
 * Reset DBSCAN configuration to defaults
 */
export function resetToDefaults() {
  const defaultConfig = createDefaultDbscanConfig();
  store.set('currentDbscanConfig', defaultConfig);
  sendDbscanConfig();
}

/**
 * Validate DBSCAN configuration
 * @param {Object} config - Configuration to validate
 * @returns {Object} Validation result {valid: boolean, errors: string[]}
 */
export function validateConfig(config) {
  const errors = [];
  
  if (!config) {
    errors.push('Configuration is required');
    return { valid: false, errors };
  }
  
  // Validate numeric ranges
  if (config.eps_norm <= 0) {
    errors.push('eps_norm must be greater than 0');
  }
  
  if (config.minPts < 1) {
    errors.push('minPts must be at least 1');
  }
  
  if (config.k_scale <= 0) {
    errors.push('k_scale must be greater than 0');
  }
  
  if (config.h_min <= 0) {
    errors.push('h_min must be greater than 0');
  }
  
  if (config.h_max <= config.h_min) {
    errors.push('h_max must be greater than h_min');
  }
  
  if (config.R_max < 1) {
    errors.push('R_max must be at least 1');
  }
  
  if (config.M_max < 1) {
    errors.push('M_max must be at least 1');
  }
  
  return {
    valid: errors.length === 0,
    errors
  };
}

/**
 * Get current DBSCAN configuration
 * @returns {Object} Current configuration
 */
export function getCurrentConfig() {
  return store.get('currentDbscanConfig');
}

/**
 * Set DBSCAN configuration
 * @param {Object} config - Configuration to set
 * @param {boolean} sendToServer - Whether to send to server (default: true)
 */
export function setConfig(config, sendToServer = true) {
  const validation = validateConfig(config);
  
  if (!validation.valid) {
    setDbscanMessage(`Invalid configuration: ${validation.errors.join(', ')}`, true);
    return false;
  }
  
  store.set('currentDbscanConfig', config);
  
  if (sendToServer) {
    sendDbscanConfig();
  }
  
  return true;
}

/**
 * Get parameter info for UI tooltips/help
 * @returns {Object} Parameter information
 */
export function getParameterInfo() {
  return {
    eps_norm: {
      name: 'Epsilon Norm',
      description: 'The maximum distance between two samples for one to be considered as in the neighborhood of the other',
      min: 0.1,
      max: 10.0,
      step: 0.1,
      default: 2.5
    },
    minPts: {
      name: 'Minimum Points',
      description: 'The number of samples in a neighborhood for a point to be considered as a core point',
      min: 1,
      max: 100,
      step: 1,
      default: 5
    },
    k_scale: {
      name: 'K Scale',
      description: 'Scaling factor for the k-nearest neighbor distance',
      min: 0.1,
      max: 10.0,
      step: 0.1,
      default: 1.0
    },
    h_min: {
      name: 'Height Min (m)',
      description: 'Minimum height threshold for clustering',
      min: 0.001,
      max: 1.0,
      step: 0.001,
      default: 0.01
    },
    h_max: {
      name: 'Height Max (m)',
      description: 'Maximum height threshold for clustering',
      min: 0.001,
      max: 1.0,
      step: 0.001,
      default: 0.20
    },
    R_max: {
      name: 'R Max',
      description: 'Maximum radius for cluster consideration',
      min: 1,
      max: 50,
      step: 1,
      default: 5
    },
    M_max: {
      name: 'M Max',
      description: 'Maximum number of points in a cluster',
      min: 10,
      max: 5000,
      step: 1,
      default: 600
    }
  };
}

/**
 * Export configuration as JSON
 * @returns {string} JSON string of current configuration
 */
export function exportConfig() {
  const config = getCurrentConfig();
  return JSON.stringify(config, null, 2);
}

/**
 * Import configuration from JSON
 * @param {string} jsonString - JSON string to import
 * @returns {boolean} Success status
 */
export function importConfig(jsonString) {
  try {
    const config = JSON.parse(jsonString);
    return setConfig(config);
  } catch (error) {
    setDbscanMessage(`Failed to import configuration: ${error.message}`, true);
    return false;
  }
}

// WebSocket message handlers
function handleDbscanConfig(message) {
  if (message.config) {
    store.set('currentDbscanConfig', message.config);
    setDbscanMessage('DBSCAN configuration loaded from server');
  }
}

function handleDbscanUpdated(message) {
  if (message.config) {
    store.set('currentDbscanConfig', message.config);
    setDbscanMessage('DBSCAN configuration updated');
  }
}