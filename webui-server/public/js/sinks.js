// Sink/publisher management UI

import * as store from './store.js';
import * as ws from './ws.js';
import * as api from './api.js';
import { setPanelMessage, debounce } from './utils.js';
import { SinkTypes, EncodingTypes, createDefaultSink } from './types.js';

// UI elements
let sinksAccordion = null;
let sinksMsg = null;
let btnAddSink = null;

// State
let sinks = [];

/**
 * Initialize sinks module
 */
export function init() {
  // Get UI elements
  sinksAccordion = document.getElementById('sinks-accordion');
  sinksMsg = document.getElementById('sinks-msg');
  btnAddSink = document.getElementById('btn-add-sink');
  
  if (!sinksAccordion) {
    console.warn('Sinks accordion element not found');
    return;
  }
  
  // Set up event handlers
  setupEventHandlers();
  
  // Subscribe to store updates
  store.subscribe('sinks', renderSinks);
  
  // Register WebSocket message handlers
  ws.registerHandler('sensor.snapshot', handleSensorSnapshot);
  
  console.log('Sinks module initialized');
}

/**
 * Set up event handlers
 */
function setupEventHandlers() {
  // Add sink button
  if (btnAddSink) {
    btnAddSink.addEventListener('click', showAddSinkModal);
  }
}

/**
 * Render sinks list
 * @param {Array} sinksArray - Array of sink objects
 */
function renderSinks(sinksArray) {
  if (!sinksAccordion) return;
  
  sinks = sinksArray || [];
  sinksAccordion.innerHTML = '';
  
  if (!Array.isArray(sinksArray) || sinksArray.length === 0) {
    const emptyMessage = document.createElement('div');
    emptyMessage.style.textAlign = 'center';
    emptyMessage.style.color = '#666';
    emptyMessage.style.padding = '20px';
    emptyMessage.textContent = 'No sinks configured';
    sinksAccordion.appendChild(emptyMessage);
    return;
  }
  
  console.log('Rendering sinks:', sinksArray.map(s => ({ ...s })));
  
  for (let i = 0; i < sinksArray.length; i++) {
    const sink = sinksArray[i];
    const item = createSinkAccordionItem(sink, i);
    sinksAccordion.appendChild(item);
  }
  
  setSinksMessage(`${sinksArray.length} sink(s) configured`);
}

/**
 * Create sink accordion item
 * @param {Object} sink - Sink data
 * @param {number} index - Sink index
 * @returns {HTMLElement} Accordion item element
 */
function createSinkAccordionItem(sink, index) {
  const item = document.createElement('div');
  item.className = 'accordion-item';
  item.setAttribute('data-sink-id', index);
  
  const type = sink.type || 'unknown';
  const enabled = sink.enabled;
  const url = sink.url || '';
  
  // Create title with type and URL
  const title = url && url !== '-' && url !== '' ? `${type.toUpperCase()}(${url})` : `${type.toUpperCase()}`;
  
  // Create header
  const header = document.createElement('div');
  header.className = 'accordion-header';
  header.setAttribute('tabindex', '0');
  header.setAttribute('role', 'button');
  header.setAttribute('aria-expanded', 'false');
  header.setAttribute('aria-controls', `sink-content-${index}`);
  
  header.innerHTML = `
    <div class="accordion-header-info">
      <div class="accordion-header-title">${title}</div>
      <div class="accordion-header-status">
        <label class="toggle-switch">
          <input type="checkbox" ${enabled ? 'checked' : ''} data-sink-id="${index}" />
          <span class="toggle-slider"></span>
        </label>
        <span class="status-badge ${enabled ? 'status-enabled' : 'status-disabled'}">
          ${enabled ? 'ON' : 'OFF'}
        </span>
      </div>
    </div>
    <div class="accordion-header-actions">
      <button class="btn" data-delete-sink="${index}" style="background-color: #dc3545; color: white; font-size: 11px; padding: 2px 6px;">Delete</button>
      <span class="accordion-toggle-caret">▼</span>
    </div>
  `;
  
  // Create content
  const content = document.createElement('div');
  content.className = 'accordion-content';
  content.id = `sink-content-${index}`;
  content.setAttribute('role', 'region');
  
  const topic = sink.topic || '';
  const encoding = sink.encoding || '';
  const rateLimit = sink.rate_limit || 0;
  const inBundle = sink.in_bundle || false;
  const bundleFragmentSize = sink.bundle_fragment_size || 1024;
  
  content.innerHTML = `
    <div class="accordion-form">
      <div class="accordion-form-row">
        <label>URL:</label>
        <input class="sink-url" type="text" value="${url}" data-sink-id="${index}" />
      </div>
      <div class="accordion-form-row">
        <label>Topic:</label>
        <input class="sink-topic" type="text" value="${topic}" data-sink-id="${index}" />
      </div>
      <div class="accordion-form-row">
        <label>Rate Limit (Hz):</label>
        <input class="sink-rate-limit" type="number" min="0" value="${rateLimit}" data-sink-id="${index}" />
      </div>
      ${type === 'nng' ? `
      <div class="accordion-form-row">
        <label>Encoding:</label>
        <select class="sink-encoding" data-sink-id="${index}">
          <option value="msgpack" ${encoding === 'msgpack' ? 'selected' : ''}>MessagePack</option>
          <option value="json" ${encoding === 'json' ? 'selected' : ''}>JSON</option>
        </select>
      </div>
      ` : ''}
      ${type === 'osc' ? `
      <div class="accordion-form-row">
        <label>In Bundle:</label>
        <input class="sink-in-bundle" type="checkbox" ${inBundle ? 'checked' : ''} data-sink-id="${index}" />
      </div>
      <div class="accordion-form-row">
        <label>Bundle Fragment Size:</label>
        <input class="sink-bundle-fragment-size" type="number" min="0" value="${bundleFragmentSize}" data-sink-id="${index}" />
      </div>
      ` : ''}
    </div>
  `;
  
  // Set up accordion functionality
  setupAccordionItem(item, header, content, sink, index);

  item.appendChild(header);
  item.appendChild(content);
  
  return item;
}

/**
 * Set up accordion item functionality
 * @param {HTMLElement} item - Accordion item
 * @param {HTMLElement} header - Header element
 * @param {HTMLElement} content - Content element
 * @param {Object} sink - Sink data
 * @param {number} index - Sink index
 */
function setupAccordionItem(item, header, content, sink, index) {
  // Toggle function
  const toggleAccordion = () => {
    const isCollapsed = item.classList.toggle('collapsed');
    header.setAttribute('aria-expanded', isCollapsed ? 'false' : 'true');
    const caret = header.querySelector('.accordion-toggle-caret');
    if (caret) {
      caret.textContent = isCollapsed ? '▶' : '▼';
    }
  };
  
  // Initially collapsed
  item.classList.add('collapsed');
  header.setAttribute('aria-expanded', 'false');
  
  // Header click handler
  header.addEventListener('click', (e) => {
    if (e.target.closest('.toggle-switch') || e.target.closest('[data-delete-sink]')) {
      return;
    }
    toggleAccordion();
  });
  
  // Keyboard handler
  header.addEventListener('keydown', (e) => {
    if (e.key === ' ' || e.key === 'Enter') {
      e.preventDefault();
      toggleAccordion();
    }
  });
  
  // Enable/disable toggle
  const toggleSwitch = header.querySelector('input[type=checkbox]');
  const statusBadge = header.querySelector('.status-badge');
  toggleSwitch.addEventListener('change', async () => {
    const enabled = toggleSwitch.checked;
    statusBadge.textContent = enabled ? 'ON' : 'OFF';
    statusBadge.className = `status-badge ${enabled ? 'status-enabled' : 'status-disabled'}`;
    
    try {
      await api.sinks.update(index, { enabled });
      setSinksMessage(`Sink ${index} ${enabled ? 'enabled' : 'disabled'}`, false);
    } catch (error) {
      setSinksMessage(`Failed to update sink: ${error.message}`, true);
      // Revert toggle state
      toggleSwitch.checked = !enabled;
      statusBadge.textContent = !enabled ? 'ON' : 'OFF';
      statusBadge.className = `status-badge ${!enabled ? 'status-enabled' : 'status-disabled'}`;
    }
  });
  
  // Input field handlers
  setupInputHandlers(content, sink, index);
  
  // Delete handler
  header.querySelector('[data-delete-sink]').addEventListener('click', () => deleteSink(index));
  
  // Prevent toggle switch clicks from bubbling
  toggleSwitch.addEventListener('click', (e) => {
    e.stopPropagation();
  });
}

/**
 * Set up input field handlers
 * @param {HTMLElement} content - Content element
 * @param {Object} sink - Sink data
 * @param {number} index - Sink index
 */
function setupInputHandlers(content, sink, index) {
  const setupInputHandler = (selector, key) => {
    const input = content.querySelector(selector);
    if (!input) return;
    
    const debouncedUpdate = debounce(async () => {
      let value = input.value;
      
      // Convert to appropriate type
      if (key === 'rate_limit' || key === 'bundle_fragment_size') {
        value = Number(value) || 0;
      } else if (key === 'in_bundle') {
        value = input.checked;
      }
      
      const patchData = {};
      patchData[key] = value;
      
      try {
        await api.sinks.update(index, patchData);
        setSinksMessage(`Sink ${index} updated`, false);
      } catch (error) {
        setSinksMessage(`Failed to update sink: ${error.message}`, true);
      }
    }, 500);
    
    if (key === 'in_bundle') {
      input.addEventListener('change', debouncedUpdate);
    } else {
      input.addEventListener('input', debouncedUpdate);
      input.addEventListener('blur', debouncedUpdate);
    }
  };
  
  setupInputHandler('.sink-url', 'url');
  setupInputHandler('.sink-topic', 'topic');
  setupInputHandler('.sink-rate-limit', 'rate_limit');
  
  if (sink.type === 'nng') {
    setupInputHandler('.sink-encoding', 'encoding');
  }
  
  if (sink.type === 'osc') {
    setupInputHandler('.sink-in-bundle', 'in_bundle');
    setupInputHandler('.sink-bundle-fragment-size', 'bundle_fragment_size');
  }
}

/**
 * Show add sink modal
 */
function showAddSinkModal() {
  const modalHtml = `
    <div id="sink-add-modal-backdrop" class="modal-backdrop">
      <div id="sink-add-modal" class="modal">
        <div class="modal__header">
          <h3>Add New Sink</h3>
          <button id="sink-add-modal-close" class="btn btn-ghost">×</button>
        </div>
        <div class="modal__body">
          <div class="modal__row">
            <label>Sink Type:</label>
            <select id="sink-type-select">
              <option value="${SinkTypes.NNG}">NNG</option>
              <option value="${SinkTypes.OSC}">OSC</option>
            </select>
          </div>
          <div class="modal__row">
            <label>URL:</label>
            <input type="text" id="sink-url-input" value="tcp://localhost:5555">
          </div>
          <div class="modal__row">
            <label>Topic:</label>
            <input type="text" id="sink-topic-input" value="lidar_data">
          </div>
          <div class="modal__row">
            <label>Rate Limit (Hz):</label>
            <input type="number" id="sink-rate-limit-input" min="0" value="30">
          </div>
          <div class="modal__row" id="sink-encoding-row">
            <label>Encoding:</label>
            <select id="sink-encoding-select">
              <option value="${EncodingTypes.MSGPACK}">MessagePack</option>
              <option value="${EncodingTypes.JSON}">JSON</option>
            </select>
          </div>
          <div class="modal__row" id="sink-bundle-row" style="display: none;">
            <label>In Bundle:</label>
            <input type="checkbox" id="sink-in-bundle-input">
          </div>
          <div class="modal__row" id="sink-fragment-row" style="display: none;">
            <label>Bundle Fragment Size:</label>
            <input type="number" id="sink-fragment-size-input" min="0" value="1024">
          </div>
          <div class="modal__row">
            <label>Enabled:</label>
            <input type="checkbox" id="sink-enabled-input" checked>
          </div>
        </div>
        <div class="modal__footer">
          <button id="sink-add-cancel" class="btn">Cancel</button>
          <button id="sink-add-save" class="btn btn-primary">Add Sink</button>
        </div>
      </div>
    </div>
  `;
  
  document.body.insertAdjacentHTML('beforeend', modalHtml);
  
  const backdrop = document.getElementById('sink-add-modal-backdrop');
  const closeBtn = document.getElementById('sink-add-modal-close');
  const cancelBtn = document.getElementById('sink-add-cancel');
  const saveBtn = document.getElementById('sink-add-save');
  const typeSelect = document.getElementById('sink-type-select');
  
  // Type-specific field visibility
  const updateFieldVisibility = () => {
    const type = typeSelect.value;
    const encodingRow = document.getElementById('sink-encoding-row');
    const bundleRow = document.getElementById('sink-bundle-row');
    const fragmentRow = document.getElementById('sink-fragment-row');
    
    if (type === SinkTypes.NNG) {
      encodingRow.style.display = 'flex';
      bundleRow.style.display = 'none';
      fragmentRow.style.display = 'none';
    } else if (type === SinkTypes.OSC) {
      encodingRow.style.display = 'none';
      bundleRow.style.display = 'flex';
      fragmentRow.style.display = 'flex';
    }
  };
  
  typeSelect.addEventListener('change', updateFieldVisibility);
  updateFieldVisibility();
  
  const closeModal = () => backdrop.remove();
  
  closeBtn.addEventListener('click', closeModal);
  cancelBtn.addEventListener('click', closeModal);
  backdrop.addEventListener('click', (e) => {
    if (e.target === backdrop) closeModal();
  });
  
  saveBtn.addEventListener('click', async () => {
    const type = document.getElementById('sink-type-select').value;
    const sinkData = {
      type: type,
      url: document.getElementById('sink-url-input').value,
      topic: document.getElementById('sink-topic-input').value,
      rate_limit: parseInt(document.getElementById('sink-rate-limit-input').value) || 0,
      enabled: document.getElementById('sink-enabled-input').checked
    };
    
    if (type === SinkTypes.NNG) {
      sinkData.encoding = document.getElementById('sink-encoding-select').value;
    } else if (type === SinkTypes.OSC) {
      sinkData.in_bundle = document.getElementById('sink-in-bundle-input').checked;
      sinkData.bundle_fragment_size = parseInt(document.getElementById('sink-fragment-size-input').value) || 1024;
    }
    
    try {
      await api.sinks.create(sinkData);
      setSinksMessage('Sink added successfully', false);
      closeModal();
      // Refresh sink list
      ws.send({ type: 'sensor.requestSnapshot' });
    } catch (error) {
      setSinksMessage(`Failed to add sink: ${error.message}`, true);
    }
  });
}

/**
 * Delete sink
 * @param {number} index - Sink index
 */
async function deleteSink(index) {
  if (!confirm(`Are you sure you want to delete sink ${index}?`)) {
    return;
  }
  
  try {
    await api.sinks.delete(index);
    setSinksMessage(`Sink ${index} deleted successfully`, false);
    // Refresh sink list
    ws.send({ type: 'sensor.requestSnapshot' });
  } catch (error) {
    setSinksMessage(`Failed to delete sink: ${error.message}`, true);
  }
}

/**
 * Set sinks message
 * @param {string} text - Message text
 * @param {boolean} isError - Whether this is an error message
 */
function setSinksMessage(text, isError = false) {
  setPanelMessage(sinksMsg, text, isError, 3000);
}

/**
 * Get current sinks
 * @returns {Array} Current sinks array
 */
export function getSinks() {
  return sinks;
}

/**
 * Refresh sinks from server
 */
export async function refreshSinks() {
  try {
    const sinksData = await api.sinks.list();
    renderSinks(sinksData);
  } catch (error) {
    setSinksMessage(`Failed to refresh sinks: ${error.message}`, true);
  }
}

// WebSocket message handlers
function handleSensorSnapshot(message) {
  // Handle sinks in snapshot
  if (message.publishers && message.publishers.sinks) {
    renderSinks(message.publishers.sinks);
  }
}