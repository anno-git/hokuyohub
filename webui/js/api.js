// REST API wrapper functions

import { showNotification } from './utils.js';

const DEFAULT_TIMEOUT = 10000; // 10 seconds
const DEFAULT_HEADERS = {
  'Content-Type': 'application/json'
};

/**
 * Generic API request wrapper with error handling and timeout
 * @param {string} url - The API endpoint URL
 * @param {Object} options - Fetch options
 * @returns {Promise<any>} The response data
 */
async function apiRequest(url, options = {}) {
  const controller = new AbortController();
  const timeoutId = setTimeout(() => controller.abort(), options.timeout || DEFAULT_TIMEOUT);
  
  try {
    const response = await fetch(url, {
      ...options,
      signal: controller.signal,
      headers: {
        ...DEFAULT_HEADERS,
        ...options.headers
      }
    });
    
    clearTimeout(timeoutId);
    
    if (!response.ok) {
      let errorMessage = `HTTP ${response.status}`;
      try {
        const errorData = await response.json();
        errorMessage = errorData.message || errorData.error || errorMessage;
      } catch (e) {
        // If we can't parse error as JSON, use status text
        errorMessage = response.statusText || errorMessage;
      }
      throw new Error(errorMessage);
    }
    
    // Handle different content types
    const contentType = response.headers.get('content-type');
    if (contentType && contentType.includes('application/json')) {
      return await response.json();
    } else if (contentType && contentType.includes('text/')) {
      return await response.text();
    } else {
      return await response.blob();
    }
    
  } catch (error) {
    clearTimeout(timeoutId);
    
    if (error.name === 'AbortError') {
      throw new Error('Request timeout');
    }
    throw error;
  }
}

/**
 * GET request wrapper
 * @param {string} url - The API endpoint URL
 * @param {Object} options - Additional fetch options
 * @returns {Promise<any>} The response data
 */
export async function get(url, options = {}) {
  return apiRequest(url, {
    method: 'GET',
    ...options
  });
}

/**
 * POST request wrapper
 * @param {string} url - The API endpoint URL
 * @param {any} data - The request body data
 * @param {Object} options - Additional fetch options
 * @returns {Promise<any>} The response data
 */
export async function post(url, data = null, options = {}) {
  return apiRequest(url, {
    method: 'POST',
    body: data ? JSON.stringify(data) : null,
    ...options
  });
}

/**
 * PUT request wrapper
 * @param {string} url - The API endpoint URL
 * @param {any} data - The request body data
 * @param {Object} options - Additional fetch options
 * @returns {Promise<any>} The response data
 */
export async function put(url, data = null, options = {}) {
  return apiRequest(url, {
    method: 'PUT',
    body: data ? JSON.stringify(data) : null,
    ...options
  });
}

/**
 * PATCH request wrapper
 * @param {string} url - The API endpoint URL
 * @param {any} data - The request body data
 * @param {Object} options - Additional fetch options
 * @returns {Promise<any>} The response data
 */
export async function patch(url, data = null, options = {}) {
  return apiRequest(url, {
    method: 'PATCH',
    body: data ? JSON.stringify(data) : null,
    ...options
  });
}

/**
 * DELETE request wrapper
 * @param {string} url - The API endpoint URL
 * @param {Object} options - Additional fetch options
 * @returns {Promise<any>} The response data
 */
export async function del(url, options = {}) {
  return apiRequest(url, {
    method: 'DELETE',
    ...options
  });
}

// Sensor API functions
export const sensors = {
  /**
   * Get all sensors
   * @returns {Promise<Array>} Array of sensor objects
   */
  async list() {
    return get('/api/v1/sensors');
  },
  
  /**
   * Get a specific sensor
   * @param {string} id - Sensor ID
   * @returns {Promise<Object>} Sensor object
   */
  async get(id) {
    return get(`/api/v1/sensors/${encodeURIComponent(id)}`);
  },
  
  /**
   * Create a new sensor
   * @param {Object} sensorData - Sensor configuration
   * @returns {Promise<Object>} Created sensor object
   */
  async create(sensorData) {
    return post('/api/v1/sensors', sensorData);
  },
  
  /**
   * Update a sensor
   * @param {string} id - Sensor ID
   * @param {Object} updates - Sensor updates
   * @returns {Promise<Object>} Updated sensor object
   */
  async update(id, updates) {
    return patch(`/api/v1/sensors/${encodeURIComponent(id)}`, updates);
  },
  
  /**
   * Delete a sensor
   * @param {string} id - Sensor ID
   * @returns {Promise<void>}
   */
  async delete(id) {
    return del(`/api/v1/sensors/${encodeURIComponent(id)}`);
  }
};

// Sink API functions
export const sinks = {
  /**
   * Get all sinks
   * @returns {Promise<Array>} Array of sink objects
   */
  async list() {
    return get('/api/v1/sinks');
  },
  
  /**
   * Get a specific sink
   * @param {number} index - Sink index
   * @returns {Promise<Object>} Sink object
   */
  async get(index) {
    return get(`/api/v1/sinks/${index}`);
  },
  
  /**
   * Create a new sink
   * @param {Object} sinkData - Sink configuration
   * @returns {Promise<Object>} Created sink object
   */
  async create(sinkData) {
    return post('/api/v1/sinks', sinkData);
  },
  
  /**
   * Update a sink
   * @param {number} index - Sink index
   * @param {Object} updates - Sink updates
   * @returns {Promise<Object>} Updated sink object
   */
  async update(index, updates) {
    return patch(`/api/v1/sinks/${index}`, updates);
  },
  
  /**
   * Delete a sink
   * @param {number} index - Sink index
   * @returns {Promise<void>}
   */
  async delete(index) {
    return del(`/api/v1/sinks/${index}`);
  }
};

// Configuration API functions
export const configs = {
  /**
   * List available configuration files
   * @returns {Promise<Object>} Object with files array
   */
  async list() {
    return get('/api/v1/configs/list');
  },
  
  /**
   * Save current configuration
   * @param {string} name - Configuration name
   * @returns {Promise<Object>} Save result
   */
  async save(name) {
    return post('/api/v1/configs/save', { name });
  },
  
  /**
   * Load a configuration
   * @param {string} name - Configuration name
   * @returns {Promise<Object>} Load result
   */
  async load(name) {
    return post('/api/v1/configs/load', { name });
  },
  
  /**
   * Export current configuration
   * @returns {Promise<string>} YAML configuration content
   */
  async export() {
    return get('/api/v1/configs/export');
  },
  
  /**
   * Import configuration from YAML content
   * @param {string} yamlContent - YAML configuration content
   * @returns {Promise<Object>} Import result
   */
  async import(yamlContent) {
    return apiRequest('/api/v1/configs/import', {
      method: 'POST',
      headers: {
        'Content-Type': 'text/plain'
      },
      body: yamlContent
    });
  }
};

/**
 * Handle API errors with user-friendly notifications
 * @param {Error} error - The error to handle
 * @param {string} operation - Description of the operation that failed
 * @param {boolean} showNotif - Whether to show notification (default: true)
 */
export function handleApiError(error, operation = 'Operation', showNotif = true) {
  const message = `${operation} failed: ${error.message}`;
  console.error(message, error);
  
  if (showNotif) {
    showNotification(message, 'error');
  }
  
  return message;
}

/**
 * Handle API success with optional notification
 * @param {any} result - The successful result
 * @param {string} operation - Description of the operation that succeeded
 * @param {boolean} showNotif - Whether to show notification (default: false)
 */
export function handleApiSuccess(result, operation = 'Operation', showNotif = false) {
  const message = `${operation} completed successfully`;
  console.log(message, result);
  
  if (showNotif) {
    showNotification(message, 'success');
  }
  
  return result;
}

/**
 * Create a download link for data
 * @param {string|Blob} data - The data to download
 * @param {string} filename - The filename for the download
 * @param {string} mimeType - The MIME type (default: 'text/plain')
 */
export function downloadData(data, filename, mimeType = 'text/plain') {
  const blob = data instanceof Blob ? data : new Blob([data], { type: mimeType });
  const url = window.URL.createObjectURL(blob);
  const a = document.createElement('a');
  a.href = url;
  a.download = filename;
  document.body.appendChild(a);
  a.click();
  document.body.removeChild(a);
  window.URL.revokeObjectURL(url);
}

/**
 * Upload a file and return its content
 * @param {string} accept - File types to accept (e.g., '.yaml,.yml')
 * @returns {Promise<string>} The file content
 */
export function uploadFile(accept = '*') {
  return new Promise((resolve, reject) => {
    const input = document.createElement('input');
    input.type = 'file';
    input.accept = accept;
    input.style.display = 'none';
    
    input.addEventListener('change', (event) => {
      const file = event.target.files[0];
      if (!file) {
        reject(new Error('No file selected'));
        return;
      }
      
      const reader = new FileReader();
      reader.onload = (e) => resolve(e.target.result);
      reader.onerror = (e) => reject(new Error('Failed to read file'));
      reader.readAsText(file);
    });
    
    document.body.appendChild(input);
    input.click();
    document.body.removeChild(input);
  });
}

/**
 * Retry an API operation with exponential backoff
 * @param {Function} operation - The operation to retry
 * @param {number} maxRetries - Maximum number of retries (default: 3)
 * @param {number} baseDelay - Base delay in milliseconds (default: 1000)
 * @returns {Promise<any>} The operation result
 */
export async function retryOperation(operation, maxRetries = 3, baseDelay = 1000) {
  let lastError;
  
  for (let attempt = 0; attempt <= maxRetries; attempt++) {
    try {
      return await operation();
    } catch (error) {
      lastError = error;
      
      if (attempt === maxRetries) {
        break;
      }
      
      const delay = baseDelay * Math.pow(2, attempt);
      console.log(`Operation failed (attempt ${attempt + 1}/${maxRetries + 1}), retrying in ${delay}ms...`);
      await new Promise(resolve => setTimeout(resolve, delay));
    }
  }
  
  throw lastError;
}