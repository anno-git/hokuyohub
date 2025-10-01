// Configuration save/load/import/export operations

import * as store from './store.js';
import * as ws from './ws.js';
import * as api from './api.js';
import { showNotification, setPanelMessage } from './utils.js';

// UI elements
let btnSaveDefault = null;
let btnSaveConfig = null;
let btnLoadConfig = null;
let btnExportConfig = null;
let btnImportConfig = null;
let configFileInput = null;

/**
 * Initialize configs module
 */
export function init() {
  // Get UI elements
  btnSaveDefault = document.getElementById('btn-save-default');
  btnSaveConfig = document.getElementById('btn-save-config');
  btnLoadConfig = document.getElementById('btn-load-config');
  btnExportConfig = document.getElementById('btn-export-config');
  btnImportConfig = document.getElementById('btn-import-config');
  configFileInput = document.getElementById('config-file-input');
  
  // Set up event handlers
  setupEventHandlers();
  
  console.log('Configs module initialized');
}

/**
 * Set up event handlers
 */
function setupEventHandlers() {
  // Save default configuration
  if (btnSaveDefault) {
    btnSaveDefault.addEventListener('click', saveDefaultConfiguration);
  }
  
  // Save configuration with custom name
  if (btnSaveConfig) {
    btnSaveConfig.addEventListener('click', saveCurrentConfiguration);
  }
  
  // Load configuration
  if (btnLoadConfig) {
    btnLoadConfig.addEventListener('click', loadConfiguration);
  }
  
  // Export configuration
  if (btnExportConfig) {
    btnExportConfig.addEventListener('click', exportConfiguration);
  }
  
  // Import configuration
  if (btnImportConfig) {
    btnImportConfig.addEventListener('click', importConfiguration);
  }
  
  // Handle file selection for import
  if (configFileInput) {
    configFileInput.addEventListener('change', handleFileImport);
  }
}

/**
 * Save current configuration to default.yaml
 */
async function saveDefaultConfiguration() {
  try {
    const result = await api.configs.save('default');
    showNotification('Configuration saved successfully to default.yaml', 'success');
    console.log('Default configuration saved:', result);
  } catch (error) {
    const message = `Save failed: ${error.message}`;
    showNotification(message, 'error');
    console.error('Failed to save default configuration:', error);
  }
}

/**
 * Save current configuration with custom name
 */
async function saveCurrentConfiguration() {
  let name = prompt('Enter configuration name:', 'webui_saved');
  if (!name) return;
  
  // Strip .yaml extension if user added it
  name = name.replace(/\.ya?ml$/i, '');
  
  try {
    const result = await api.configs.save(name);
    showNotification(`Configuration saved successfully as ${result.name || name}`, 'success');
    console.log('Configuration saved:', result);
  } catch (error) {
    const message = `Save failed: ${error.message}`;
    showNotification(message, 'error');
    console.error('Failed to save configuration:', error);
  }
}

/**
 * Load configuration from server
 */
async function loadConfiguration() {
  try {
    // First, get the list of available configurations
    const files = await api.configs.list();

    if (files.length === 0) {
      showNotification('No configuration files found', 'warning');
      return;
    }
    
    // Create a simple selection dialog
    const options = files.map(({name}, index) => `${index + 1}. ${name}`).join('\n');
    const selection = prompt(`Select configuration to load:\n${options}\n\nEnter number or name:`);
    
    if (!selection) return;
    
    let selectedFile;
    // Check if user entered a number
    const num = parseInt(selection);
    if (!isNaN(num) && num >= 1 && num <= files.length) {
      selectedFile = files[num - 1];
    } else {
      // Check if the entered name exists in the list
      selectedFile = files.find(({name}) => name.toLowerCase() === selection.toLowerCase());
      if (!selectedFile) {
        showNotification(`Configuration "${selection}" not found`, 'error');
        return;
      }
    }

    // Load the selected configuration
    const result = await api.configs.load(selectedFile.name);
    showNotification(`Configuration loaded successfully: ${selectedFile.name}`, 'success');
    console.log('Configuration loaded:', result);
    
    // Refresh all browser state from server
    await refreshBrowserStateFromServer();
    
  } catch (error) {
    const message = `Load failed: ${error.message}`;
    showNotification(message, 'error');
    console.error('Failed to load configuration:', error);
  }
}

/**
 * Export current configuration
 */
async function exportConfiguration() {
  try {
    const yamlContent = await api.configs.export();
    
    // Create download link
    api.downloadData(yamlContent, 'hokuyo_config.yaml', 'text/yaml');
    
    showNotification('Configuration exported successfully', 'success');
    console.log('Configuration exported');
    
  } catch (error) {
    const message = `Export failed: ${error.message}`;
    showNotification(message, 'error');
    console.error('Failed to export configuration:', error);
  }
}

/**
 * Import configuration from file
 */
function importConfiguration() {
  if (configFileInput) {
    configFileInput.click();
  } else {
    // Fallback: use API uploadFile function
    api.uploadFile('.yaml,.yml,text/yaml,text/plain')
      .then(handleFileContent)
      .catch(error => {
        showNotification(`Import failed: ${error.message}`, 'error');
      });
  }
}

/**
 * Handle file selection for import
 * @param {Event} event - File input change event
 */
async function handleFileImport(event) {
  const file = event.target.files[0];
  if (!file) return;
  
  try {
    const yamlContent = await readFileAsText(file);
    await handleFileContent(yamlContent, file.name);
  } catch (error) {
    showNotification(`Import failed: ${error.message}`, 'error');
    console.error('Failed to import configuration:', error);
  }
  
  // Clear the file input
  event.target.value = '';
}

/**
 * Handle file content for import
 * @param {string} yamlContent - YAML content
 * @param {string} fileName - File name (optional)
 */
async function handleFileContent(yamlContent, fileName = 'imported file') {
  try {
    const result = await api.configs.import(yamlContent);
    showNotification(`Configuration imported successfully from ${fileName}`, 'success');
    console.log('Configuration imported:', result);
    
    // Refresh all browser state from server
    await refreshBrowserStateFromServer();
    
  } catch (error) {
    const message = `Import failed: ${error.message}`;
    showNotification(message, 'error');
    console.error('Failed to import configuration:', error);
  }
}

/**
 * Read file as text
 * @param {File} file - File to read
 * @returns {Promise<string>} File content as text
 */
function readFileAsText(file) {
  return new Promise((resolve, reject) => {
    const reader = new FileReader();
    reader.onload = (e) => resolve(e.target.result);
    reader.onerror = (e) => reject(new Error('Failed to read file'));
    reader.readAsText(file);
  });
}

/**
 * Refresh browser state from server
 */
async function refreshBrowserStateFromServer() {
  try {
    // Request fresh sensor snapshot
    ws.send({ type: 'sensor.requestSnapshot' });
    
    // Request fresh filter configuration
    ws.send({ type: 'filter.requestConfig' });
    
    // Request fresh DBSCAN configuration
    ws.send({ type: 'dbscan.requestConfig' });
    
    // Wait a bit for the snapshots to arrive and update UI
    await new Promise(resolve => setTimeout(resolve, 500));
    
    console.log('Browser state refreshed from server');
    
  } catch (error) {
    console.error('Failed to refresh browser state:', error);
  }
}

/**
 * Load configuration from server by path
 * @param {string} path - Configuration file path
 */
export async function loadConfigurationFromServer(path) {
  try {
    const result = await api.configs.load(path);
    showNotification(`Configuration loaded: ${path}`, 'success');
    console.log('Configuration loaded from path:', result);
    
    // Refresh sensor data
    ws.send({ type: 'sensor.requestSnapshot' });
    
  } catch (error) {
    const message = `Load failed: ${error.message}`;
    showNotification(message, 'error');
    console.error('Failed to load configuration from path:', error);
  }
}

/**
 * Save configuration with specific name
 * @param {string} name - Configuration name
 * @returns {Promise<boolean>} Success status
 */
export async function saveConfiguration(name) {
  try {
    const result = await api.configs.save(name);
    showNotification(`Configuration saved: ${result.name || name}`, 'success');
    console.log('Configuration saved with name:', result);
    return true;
  } catch (error) {
    const message = `Save failed: ${error.message}`;
    showNotification(message, 'error');
    console.error('Failed to save configuration with name:', error);
    return false;
  }
}

/**
 * Get list of available configurations
 * @returns {Promise<Array>} List of configuration names
 */
export async function getConfigurationList() {
  try {
    const result = await api.configs.list();
    return result.files || [];
  } catch (error) {
    console.error('Failed to get configuration list:', error);
    return [];
  }
}

/**
 * Export configuration as string
 * @returns {Promise<string>} YAML configuration content
 */
export async function exportConfigurationAsString() {
  try {
    return await api.configs.export();
  } catch (error) {
    console.error('Failed to export configuration as string:', error);
    throw error;
  }
}

/**
 * Import configuration from string
 * @param {string} yamlContent - YAML configuration content
 * @returns {Promise<boolean>} Success status
 */
export async function importConfigurationFromString(yamlContent) {
  try {
    const result = await api.configs.import(yamlContent);
    showNotification('Configuration imported successfully', 'success');
    console.log('Configuration imported from string:', result);
    
    // Refresh browser state
    await refreshBrowserStateFromServer();
    
    return true;
  } catch (error) {
    const message = `Import failed: ${error.message}`;
    showNotification(message, 'error');
    console.error('Failed to import configuration from string:', error);
    return false;
  }
}

/**
 * Create a backup of current configuration
 * @returns {Promise<boolean>} Success status
 */
export async function createBackup() {
  const timestamp = new Date().toISOString().replace(/[:.]/g, '-').slice(0, -5);
  const backupName = `backup_${timestamp}`;
  
  return await saveConfiguration(backupName);
}

/**
 * Restore from backup
 * @param {string} backupName - Backup configuration name
 * @returns {Promise<boolean>} Success status
 */
export async function restoreFromBackup(backupName) {
  try {
    const result = await api.configs.load(backupName);
    showNotification(`Restored from backup: ${backupName}`, 'success');
    console.log('Restored from backup:', result);
    
    // Refresh browser state
    await refreshBrowserStateFromServer();
    
    return true;
  } catch (error) {
    const message = `Restore failed: ${error.message}`;
    showNotification(message, 'error');
    console.error('Failed to restore from backup:', error);
    return false;
  }
}

/**
 * Validate configuration before import
 * @param {string} yamlContent - YAML content to validate
 * @returns {Object} Validation result {valid: boolean, errors: string[]}
 */
export function validateConfiguration(yamlContent) {
  const errors = [];
  
  if (!yamlContent || typeof yamlContent !== 'string') {
    errors.push('Configuration content is required');
    return { valid: false, errors };
  }
  
  if (yamlContent.trim().length === 0) {
    errors.push('Configuration content cannot be empty');
    return { valid: false, errors };
  }
  
  // Basic YAML structure validation
  try {
    // Try to parse as JSON first (simple validation)
    if (yamlContent.trim().startsWith('{')) {
      JSON.parse(yamlContent);
    }
  } catch (e) {
    // If it's not JSON, assume it's YAML and do basic checks
    if (!yamlContent.includes(':')) {
      errors.push('Configuration does not appear to be valid YAML format');
    }
  }
  
  // Check for required sections (basic validation)
  const requiredSections = ['sensors', 'world_mask', 'filter_config'];
  const hasAnySections = requiredSections.some(section => 
    yamlContent.includes(section) || yamlContent.includes(section.replace('_', '-'))
  );
  
  if (!hasAnySections) {
    errors.push('Configuration does not contain expected sections (sensors, world_mask, filter_config)');
  }
  
  return {
    valid: errors.length === 0,
    errors
  };
}