// Filter configuration UI management

import * as store from './store.js';
import * as ws from './ws.js';
import { setPanelMessage, debounce } from './utils.js';
import { createDefaultFilterConfig } from './types.js';

// UI elements
let filterElements = {};
let filterMsg = null;

// Debounced update function
let debouncedSendConfig = null;

/**
 * Initialize filters module
 */
export function init() {
  // Get all filter UI elements
  filterElements = {
    // Prefilter elements
    prefilterEnabled: document.getElementById('prefilter-enabled'),
    prefilterNeighborhoodEnabled: document.getElementById('prefilter-neighborhood-enabled'),
    prefilterNeighborhoodK: document.getElementById('prefilter-neighborhood-k'),
    prefilterNeighborhoodRBase: document.getElementById('prefilter-neighborhood-r-base'),
    prefilterNeighborhoodRScale: document.getElementById('prefilter-neighborhood-r-scale'),
    
    prefilterSpikeEnabled: document.getElementById('prefilter-spike-enabled'),
    prefilterSpikeDrThreshold: document.getElementById('prefilter-spike-dr-threshold'),
    prefilterSpikeWindowSize: document.getElementById('prefilter-spike-window-size'),
    
    prefilterOutlierEnabled: document.getElementById('prefilter-outlier-enabled'),
    prefilterOutlierMedianWindow: document.getElementById('prefilter-outlier-median-window'),
    prefilterOutlierThreshold: document.getElementById('prefilter-outlier-threshold'),
    
    prefilterIntensityEnabled: document.getElementById('prefilter-intensity-enabled'),
    prefilterIntensityMin: document.getElementById('prefilter-intensity-min'),
    prefilterIntensityReliability: document.getElementById('prefilter-intensity-reliability'),
    
    prefilterIsolationEnabled: document.getElementById('prefilter-isolation-enabled'),
    prefilterIsolationMinSize: document.getElementById('prefilter-isolation-min-size'),
    prefilterIsolationRadius: document.getElementById('prefilter-isolation-radius'),
    
    // Postfilter elements
    postfilterEnabled: document.getElementById('postfilter-enabled'),
    postfilterIsolationEnabled: document.getElementById('postfilter-isolation-enabled'),
    postfilterIsolationMinSize: document.getElementById('postfilter-isolation-min-size'),
    postfilterIsolationRadius: document.getElementById('postfilter-isolation-radius'),
    postfilterIsolationRequiredNeighbors: document.getElementById('postfilter-isolation-required-neighbors'),

    // Control elements
    filterMsg: document.getElementById('filter-msg')
  };
  
  filterMsg = filterElements.filterMsg;
  
  // Create debounced send function
  debouncedSendConfig = debounce(sendFilterConfig, 500);
  
  // Set up event handlers
  setupEventHandlers();
  
  // Set up accordion functionality
  setupFilterAccordion();
  
  // Subscribe to state changes
  store.subscribe('currentFilterConfig', applyFilterConfigToUI);
  
  // Register WebSocket message handlers
  ws.registerHandler('filter.config', handleFilterConfig);
  ws.registerHandler('filter.updated', handleFilterUpdated);
  
  // Initialize UI state
  updateFilterUIState();
  
  console.log('Filters module initialized');
}

/**
 * Set up event handlers
 */
function setupEventHandlers() {
  // Main enable/disable checkboxes
  if (filterElements.prefilterEnabled) {
    filterElements.prefilterEnabled.addEventListener('change', () => {
      updateFilterUIState();
      sendFilterConfig();
    });
  }
  
  if (filterElements.postfilterEnabled) {
    filterElements.postfilterEnabled.addEventListener('change', () => {
      updateFilterUIState();
      sendFilterConfig();
    });
  }
  
  // Strategy enable/disable checkboxes
  const strategyCheckboxes = [
    filterElements.prefilterNeighborhoodEnabled,
    filterElements.prefilterSpikeEnabled,
    filterElements.prefilterOutlierEnabled,
    filterElements.prefilterIntensityEnabled,
    filterElements.prefilterIsolationEnabled,
    filterElements.postfilterIsolationEnabled
  ];
  
  strategyCheckboxes.forEach(checkbox => {
    if (checkbox) {
      checkbox.addEventListener('change', () => {
        updateFilterUIState();
        sendFilterConfig();
      });
    }
  });
  
  // Parameter input fields
  const parameterInputs = [
    filterElements.prefilterNeighborhoodK,
    filterElements.prefilterNeighborhoodRBase,
    filterElements.prefilterNeighborhoodRScale,
    filterElements.prefilterSpikeDrThreshold,
    filterElements.prefilterSpikeWindowSize,
    filterElements.prefilterOutlierMedianWindow,
    filterElements.prefilterOutlierThreshold,
    filterElements.prefilterIntensityMin,
    filterElements.prefilterIntensityReliability,
    filterElements.prefilterIsolationMinSize,
    filterElements.prefilterIsolationRadius,
    filterElements.postfilterIsolationMinSize,
    filterElements.postfilterIsolationRadius,
    filterElements.postfilterIsolationRequiredNeighbors
  ];
  
  parameterInputs.forEach(input => {
    if (input) {
      input.addEventListener('input', debouncedSendConfig);
      input.addEventListener('change', sendFilterConfig);
      input.addEventListener('blur', sendFilterConfig);
    }
  });
}

/**
 * Set up filter accordion functionality
 */
function setupFilterAccordion() {
  // Setup section-level accordion (Prefilter/Postfilter)
  const filterSections = document.querySelectorAll('#filter-panel .filter-section');
  
  filterSections.forEach((section, index) => {
    const header = section.querySelector('h3');
    const content = section.querySelector('.filter-content');
    
    if (!header || !content) return;
    
    // Ensure content has an ID for aria-controls
    if (!content.id) {
      content.id = `filter-content-${index}`;
    }
    
    // Set up ARIA attributes
    header.setAttribute('aria-controls', content.id);
    content.setAttribute('role', 'region');
    
    // Toggle function
    const toggleSection = () => {
      const isCollapsed = section.classList.toggle('collapsed');
      header.setAttribute('aria-expanded', isCollapsed ? 'false' : 'true');
      
      // Update caret direction
      const caret = header.querySelector('.toggle-caret');
      if (caret) {
        caret.textContent = isCollapsed ? '▶' : '▼';
      }
    };
    
    // Click handler
    header.addEventListener('click', (e) => {
      // Don't toggle if clicking on the checkbox
      if (e.target.type === 'checkbox' || e.target.closest('.control-item input[type="checkbox"]')) {
        return;
      }
      toggleSection();
    });
    
    // Keyboard handler
    header.addEventListener('keydown', (e) => {
      if (e.key === ' ' || e.key === 'Enter') {
        e.preventDefault();
        toggleSection();
      }
    });
    
    // Prevent checkbox clicks from bubbling to header
    const checkbox = header.querySelector('input[type="checkbox"]');
    if (checkbox) {
      checkbox.addEventListener('click', (e) => {
        e.stopPropagation();
      });
    }
  });

  // Setup strategy-group level accordion (Neighborhood Filter, Spike Removal, etc.)
  const strategyGroups = document.querySelectorAll('#filter-panel .strategy-group');
  
  strategyGroups.forEach((group, index) => {
    const header = group.querySelector('h4');
    const params = group.querySelector('.strategy-params');
    
    if (!header || !params) return;
    
    // Ensure params has an ID for aria-controls
    if (!params.id) {
      params.id = `strategy-params-${index}`;
    }
    
    // Set up ARIA attributes
    header.setAttribute('aria-controls', params.id);
    params.setAttribute('role', 'region');
    
    // Toggle function
    const toggleGroup = () => {
      const isCollapsed = group.classList.toggle('collapsed');
      header.setAttribute('aria-expanded', isCollapsed ? 'false' : 'true');
      
      // Update caret direction
      const caret = header.querySelector('.toggle-caret');
      if (caret) {
        caret.textContent = isCollapsed ? '▶' : '▼';
      }
    };
    
    // Click handler
    header.addEventListener('click', (e) => {
      // Don't toggle if clicking on the checkbox
      if (e.target.type === 'checkbox' || e.target.closest('.control-item input[type="checkbox"]')) {
        return;
      }
      toggleGroup();
    });
    
    // Keyboard handler
    header.addEventListener('keydown', (e) => {
      if (e.key === ' ' || e.key === 'Enter') {
        e.preventDefault();
        toggleGroup();
      }
    });
    
    // Prevent checkbox clicks from bubbling to header
    const checkbox = header.querySelector('input[type="checkbox"]');
    if (checkbox) {
      checkbox.addEventListener('click', (e) => {
        e.stopPropagation();
      });
    }
  });
}

/**
 * Update filter UI state (enable/disable sections based on checkboxes)
 */
function updateFilterUIState() {
  // Update prefilter section state
  const prefilterSection = document.querySelector('.filter-section:first-child');
  if (prefilterSection && filterElements.prefilterEnabled) {
    prefilterSection.classList.toggle('disabled', !filterElements.prefilterEnabled.checked);
  }
  
  // Update postfilter section state
  const postfilterSection = document.querySelector('.filter-section:last-child');
  if (postfilterSection && filterElements.postfilterEnabled) {
    postfilterSection.classList.toggle('disabled', !filterElements.postfilterEnabled.checked);
  }
  
  // Update strategy group states
  const strategyGroups = document.querySelectorAll('.strategy-group');
  strategyGroups.forEach(group => {
    const checkbox = group.querySelector('input[type="checkbox"]');
    if (checkbox) {
      group.classList.toggle('disabled', !checkbox.checked);
    }
  });
}

/**
 * Load filter configuration from UI elements
 * @returns {Object} Filter configuration
 */
function loadFilterConfigFromUI() {
  const config = {
    prefilter: {
      enabled: filterElements.prefilterEnabled?.checked ?? true,
      neighborhood: {
        enabled: filterElements.prefilterNeighborhoodEnabled?.checked ?? true,
        k: parseInt(filterElements.prefilterNeighborhoodK?.value ?? 5),
        r_base: parseFloat(filterElements.prefilterNeighborhoodRBase?.value ?? 0.05),
        r_scale: parseFloat(filterElements.prefilterNeighborhoodRScale?.value ?? 1.0)
      },
      spike_removal: {
        enabled: filterElements.prefilterSpikeEnabled?.checked ?? true,
        dr_threshold: parseFloat(filterElements.prefilterSpikeDrThreshold?.value ?? 0.3),
        window_size: parseInt(filterElements.prefilterSpikeWindowSize?.value ?? 3)
      },
      outlier_removal: {
        enabled: filterElements.prefilterOutlierEnabled?.checked ?? true,
        median_window: parseInt(filterElements.prefilterOutlierMedianWindow?.value ?? 5),
        outlier_threshold: parseFloat(filterElements.prefilterOutlierThreshold?.value ?? 2.0)
      },
      intensity_filter: {
        enabled: filterElements.prefilterIntensityEnabled?.checked ?? false,
        min_intensity: parseFloat(filterElements.prefilterIntensityMin?.value ?? 0),
        min_reliability: parseFloat(filterElements.prefilterIntensityReliability?.value ?? 0)
      },
      isolation_removal: {
        enabled: filterElements.prefilterIsolationEnabled?.checked ?? true,
        min_cluster_size: parseInt(filterElements.prefilterIsolationMinSize?.value ?? 3),
        isolation_radius: parseFloat(filterElements.prefilterIsolationRadius?.value ?? 0.1)
      }
    },
    postfilter: {
      enabled: filterElements.postfilterEnabled?.checked ?? true,
      isolation_removal: {
        enabled: filterElements.postfilterIsolationEnabled?.checked ?? true,
        min_points_size: parseInt(filterElements.postfilterIsolationMinSize?.value ?? 3),
        isolation_radius: parseFloat(filterElements.postfilterIsolationRadius?.value ?? 0.2),
        required_neighbors: parseInt(filterElements.postfilterIsolationRequiredNeighbors?.value ?? 2)
      }
    }
  };
  
  return config;
}

/**
 * Apply filter configuration to UI elements
 * @param {Object} config - Filter configuration
 */
function applyFilterConfigToUI(config) {
  if (!config) return;
  
  // Prefilter settings
  if (filterElements.prefilterEnabled) filterElements.prefilterEnabled.checked = config.prefilter.enabled;
  
  if (config.prefilter.neighborhood) {
    if (filterElements.prefilterNeighborhoodEnabled) filterElements.prefilterNeighborhoodEnabled.checked = config.prefilter.neighborhood.enabled;
    if (filterElements.prefilterNeighborhoodK) filterElements.prefilterNeighborhoodK.value = config.prefilter.neighborhood.k;
    if (filterElements.prefilterNeighborhoodRBase) filterElements.prefilterNeighborhoodRBase.value = config.prefilter.neighborhood.r_base;
    if (filterElements.prefilterNeighborhoodRScale) filterElements.prefilterNeighborhoodRScale.value = config.prefilter.neighborhood.r_scale;
  }
  
  if (config.prefilter.spike_removal) {
    if (filterElements.prefilterSpikeEnabled) filterElements.prefilterSpikeEnabled.checked = config.prefilter.spike_removal.enabled;
    if (filterElements.prefilterSpikeDrThreshold) filterElements.prefilterSpikeDrThreshold.value = config.prefilter.spike_removal.dr_threshold;
    if (filterElements.prefilterSpikeWindowSize) filterElements.prefilterSpikeWindowSize.value = config.prefilter.spike_removal.window_size;
  }
  
  if (config.prefilter.outlier_removal) {
    if (filterElements.prefilterOutlierEnabled) filterElements.prefilterOutlierEnabled.checked = config.prefilter.outlier_removal.enabled;
    if (filterElements.prefilterOutlierMedianWindow) filterElements.prefilterOutlierMedianWindow.value = config.prefilter.outlier_removal.median_window;
    if (filterElements.prefilterOutlierThreshold) filterElements.prefilterOutlierThreshold.value = config.prefilter.outlier_removal.outlier_threshold;
  }
  
  if (config.prefilter.intensity_filter) {
    if (filterElements.prefilterIntensityEnabled) filterElements.prefilterIntensityEnabled.checked = config.prefilter.intensity_filter.enabled;
    if (filterElements.prefilterIntensityMin) filterElements.prefilterIntensityMin.value = config.prefilter.intensity_filter.min_intensity;
    if (filterElements.prefilterIntensityReliability) filterElements.prefilterIntensityReliability.value = config.prefilter.intensity_filter.min_reliability;
  }
  
  if (config.prefilter.isolation_removal) {
    if (filterElements.prefilterIsolationEnabled) filterElements.prefilterIsolationEnabled.checked = config.prefilter.isolation_removal.enabled;
    if (filterElements.prefilterIsolationMinSize) filterElements.prefilterIsolationMinSize.value = config.prefilter.isolation_removal.min_cluster_size;
    if (filterElements.prefilterIsolationRadius) filterElements.prefilterIsolationRadius.value = config.prefilter.isolation_removal.isolation_radius;
  }
  
  // Postfilter settings
  if (filterElements.postfilterEnabled) filterElements.postfilterEnabled.checked = config.postfilter.enabled;
  
  if (config.postfilter.isolation_removal) {
    if (filterElements.postfilterIsolationEnabled) filterElements.postfilterIsolationEnabled.checked = config.postfilter.isolation_removal.enabled;
    if (filterElements.postfilterIsolationMinSize) filterElements.postfilterIsolationMinSize.value = config.postfilter.isolation_removal.min_points_size;
    if (filterElements.postfilterIsolationRadius) filterElements.postfilterIsolationRadius.value = config.postfilter.isolation_removal.isolation_radius;
    if (filterElements.postfilterIsolationRequiredNeighbors) filterElements.postfilterIsolationRequiredNeighbors.value = config.postfilter.isolation_removal.required_neighbors;
  }

  updateFilterUIState();
}

/**
 * Send filter configuration to server
 */
function sendFilterConfig() {
  const config = loadFilterConfigFromUI();
  store.set('currentFilterConfig', config);
  
  if (ws.updateFilter(config)) {
    setFilterMessage('Filter configuration updated');
  } else {
    setFilterMessage('Failed to send filter configuration', true);
  }
}

/**
 * Set filter message
 * @param {string} text - Message text
 * @param {boolean} isError - Whether this is an error message
 */
function setFilterMessage(text, isError = false) {
  setPanelMessage(filterMsg, text, isError, 3000);
}

// WebSocket message handlers
function handleFilterConfig(message) {
  if (message.config) {
    store.set('currentFilterConfig', message.config);
    setFilterMessage('Filter configuration received from server');
  }
}

function handleFilterUpdated(message) {
  if (message.config) {
    store.set('currentFilterConfig', message.config);
    setFilterMessage('Filter configuration updated');
  }
}