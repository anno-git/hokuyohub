// Sensor UI and operations management

import * as store from './store.js';
import * as ws from './ws.js';
import * as api from './api.js';
import { setPanelMessage, debounce } from './utils.js';
import { SensorTypes, SensorModes, createDefaultSensor } from './types.js';

// UI elements
let sensorsAccordion = null;
let panelMsg = null;
let btnAddSensor = null;

// Modal elements
let modalBackdrop = null;
let modalRoot = null;
let modalTitle = null;
let modalBody = null;
let modalClose = null;
let modalCancel = null;
let modalSave = null;

// State
let modalSensorId = null;
let modalWorking = null;
let pendingSaveForId = null;

// State preservation
let accordionStates = new Map();

/**
 * Initialize sensors module
 */
export function init() {
  // Get UI elements
  sensorsAccordion = document.getElementById('sensors-accordion');
  panelMsg = document.getElementById('panel-msg');
  btnAddSensor = document.getElementById('btn-add-sensor');
  
  // Get modal elements
  modalBackdrop = document.getElementById('sensor-modal-backdrop');
  modalRoot = document.getElementById('sensor-modal');
  modalTitle = document.getElementById('sensor-modal-title');
  modalBody = document.getElementById('sensor-modal-body');
  modalClose = document.getElementById('sensor-modal-close');
  modalCancel = document.getElementById('sensor-modal-cancel');
  modalSave = document.getElementById('sensor-modal-save');
  
  if (!sensorsAccordion) {
    console.warn('Sensors accordion element not found');
    return;
  }
  
  // Set up event handlers
  setupEventHandlers();
  
  // Subscribe to state changes
  store.subscribe('sensors', renderSensors);
  
  // Register WebSocket message handlers
  ws.registerHandler('sensor.snapshot', handleSensorSnapshot);
  ws.registerHandler('sensor.updated', handleSensorUpdated);
  ws.registerHandler('ok', handleOkResponse);
  ws.registerHandler('error', handleErrorResponse);
  
  console.log('Sensors module initialized');
}

/**
 * Set up event handlers
 */
function setupEventHandlers() {
  // Add sensor button
  if (btnAddSensor) {
    btnAddSensor.addEventListener('click', showAddSensorModal);
  }
  
  // Modal event handlers
  if (modalClose) modalClose.addEventListener('click', closeSensorModal);
  if (modalCancel) modalCancel.addEventListener('click', closeSensorModal);
  if (modalSave) modalSave.addEventListener('click', saveSensorModal);
  if (modalBackdrop) {
    modalBackdrop.addEventListener('click', (e) => {
      if (e.target === modalBackdrop) closeSensorModal();
    });
  }
}

/**
 * Render sensors list
 */
function renderSensors() {
  if (!sensorsAccordion) return;
  
  // Preserve UI state before re-rendering
  preserveAccordionStates();
  
  sensorsAccordion.innerHTML = '';
  const sensors = store.get('sensors');
  const sensorsArray = Array.from(sensors.values()).sort((a, b) => a.id - b.id);
  
  console.log('Rendering sensors:', sensorsArray.map(s => ({ ...s })));
  
  for (const sensor of sensorsArray) {
    const item = createSensorAccordionItem(sensor);
    sensorsAccordion.appendChild(item);
  }
  
  // Restore UI state after re-rendering
  restoreAccordionStates();
}

/**
 * Create sensor accordion item
 * @param {Object} sensor - Sensor data
 * @returns {HTMLElement} Accordion item element
 */
function createSensorAccordionItem(sensor) {
  const item = document.createElement('div');
  item.className = 'accordion-item';
  item.setAttribute('data-sensor-id', sensor.id);
  
  // Create header
  const header = document.createElement('div');
  header.className = 'accordion-header';
  header.setAttribute('tabindex', '0');
  header.setAttribute('role', 'button');
  header.setAttribute('aria-expanded', 'false');
  header.setAttribute('aria-controls', `sensor-content-${sensor.id}`);
  
  header.innerHTML = `
    <div class="accordion-header-info">
      <div class="accordion-header-title">${sensor.id}</div>
      <div class="accordion-header-status">
        <label class="toggle-switch">
          <input type="checkbox" ${sensor.enabled ? 'checked' : ''} data-sensor-id="${sensor.id}" />
          <span class="toggle-slider"></span>
        </label>
        <span class="status-badge ${sensor.enabled ? 'status-enabled' : 'status-disabled'}">
          ${sensor.enabled ? 'ON' : 'OFF'}
        </span>
      </div>
    </div>
    <div class="accordion-header-actions">
      <button class="btn" data-delete-sensor="${sensor.id}" style="background-color: #dc3545; color: white; font-size: 11px; padding: 2px 6px;">Delete</button>
      <span class="accordion-toggle-caret">▼</span>
    </div>
  `;
  
  // Create content
  const content = document.createElement('div');
  content.className = 'accordion-content';
  content.id = `sensor-content-${sensor.id}`;
  content.setAttribute('role', 'region');
  
  content.innerHTML = `
    <div class="accordion-form">
      <div class="accordion-form-row">
        <label>Position X:</label>
        <input class="pose-tx" type="number" step="0.01" value="${Number(sensor.pose?.tx ?? 0)}" data-sensor-id="${sensor.id}" />
      </div>
      <div class="accordion-form-row">
        <label>Position Y:</label>
        <input class="pose-ty" type="number" step="0.01" value="${Number(sensor.pose?.ty ?? 0)}" data-sensor-id="${sensor.id}" />
      </div>
      <div class="accordion-form-row">
        <label>Rotation (°):</label>
        <input class="pose-theta" type="number" step="0.1" value="${Number(sensor.pose?.theta_deg ?? 0)}" data-sensor-id="${sensor.id}" />
      </div>
      <div class="accordion-form-actions">
        <button class="btn" data-open-modal="${sensor.id}">Advanced Settings…</button>
      </div>
    </div>
  `;
  
  // Set up accordion functionality
  setupAccordionItem(item, header, content, sensor);

  item.appendChild(header);
  item.appendChild(content);

  return item;
}

/**
 * Set up accordion item functionality
 * @param {HTMLElement} item - Accordion item
 * @param {HTMLElement} header - Header element
 * @param {HTMLElement} content - Content element
 * @param {Object} sensor - Sensor data
 */
function setupAccordionItem(item, header, content, sensor) {
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
    if (e.target.closest('.toggle-switch') || e.target.closest('[data-delete-sensor]')) {
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
  toggleSwitch.addEventListener('change', () => {
    const enabled = toggleSwitch.checked;
    statusBadge.textContent = enabled ? 'ON' : 'OFF';
    statusBadge.className = `status-badge ${enabled ? 'status-enabled' : 'status-disabled'}`;
    
    ws.enableSensor(Number(toggleSwitch.dataset.sensorId), enabled);
  });
  
  // Pose input handlers with optimistic updates
  setupPoseInputs(content, sensor);
  
  // Modal and delete handlers
  content.querySelector('[data-open-modal]').addEventListener('click', () => openSensorModal(sensor.id));
  header.querySelector('[data-delete-sensor]').addEventListener('click', () => deleteSensor(sensor.id));
  
  // Prevent toggle switch clicks from bubbling
  toggleSwitch.addEventListener('click', (e) => {
    e.stopPropagation();
  });
}

/**
 * Set up pose input handlers
 * @param {HTMLElement} content - Content element
 * @param {Object} sensor - Sensor data
 */
function setupPoseInputs(content, sensor) {
  const setupPoseInput = (selector, key) => {
    const input = content.querySelector(selector);
    if (!input) return;
    
    const debouncedUpdate = debounce(() => {
      const sensorId = Number(input.dataset.sensorId);
      const value = Number(input.value || 0);
      
      // Optimistic update: immediately update local sensor state
      const currentSensor = store.getSensor(sensorId);
      if (currentSensor && currentSensor.pose) {
        currentSensor.pose[key] = value;
        // Trigger canvas redraw
        store.set('sensors', store.get('sensors'), true);
      }
      
      const patch = { pose: {} };
      patch.pose[key] = value;
      
      // Send with client ID for sender identification
      ws.updateSensor(sensorId, patch, store.get('clientId'));
    }, 300);
    
    input.addEventListener('input', debouncedUpdate);
    input.addEventListener('blur', debouncedUpdate);
  };
  
  setupPoseInput('.pose-tx', 'tx');
  setupPoseInput('.pose-ty', 'ty');
  setupPoseInput('.pose-theta', 'theta_deg');
}

/**
 * Show add sensor modal
 */
function showAddSensorModal() {
  const modalHtml = `
    <div id="sensor-add-modal-backdrop" class="modal-backdrop">
      <div id="sensor-add-modal" class="modal">
        <div class="modal__header">
          <h3>Add New Sensor</h3>
          <button id="sensor-add-modal-close" class="btn btn-ghost">×</button>
        </div>
        <div class="modal__body">
          <div class="modal__row">
            <label>Sensor Type:</label>
            <select id="sensor-type-select">
              <option value="${SensorTypes.HOKUYO_URG_ETH}">Hokuyo URG Ethernet</option>
              <option value="${SensorTypes.UNKNOWN}">Unknown</option>
            </select>
          </div>
          <div class="modal__row">
            <label>Name:</label>
            <input type="text" id="sensor-name-input" value="New Sensor">
          </div>
          <div class="modal__row">
            <label>Host:</label>
            <input type="text" id="sensor-host-input" value="192.168.1.10">
          </div>
          <div class="modal__row">
            <label>Port:</label>
            <input type="number" id="sensor-port-input" min="1" max="65535" value="10940">
          </div>
          <div class="modal__row">
            <label>Mode:</label>
            <select id="sensor-mode-select">
              <option value="${SensorModes.ME}">ME (Distance + Intensity)</option>
              <option value="${SensorModes.MD}">MD (Distance Only)</option>
            </select>
          </div>
          <div class="modal__row">
            <label>Enabled:</label>
            <input type="checkbox" id="sensor-enabled-input" checked>
          </div>
        </div>
        <div class="modal__footer">
          <button id="sensor-add-cancel" class="btn">Cancel</button>
          <button id="sensor-add-save" class="btn btn-primary">Add Sensor</button>
        </div>
      </div>
    </div>
  `;
  
  document.body.insertAdjacentHTML('beforeend', modalHtml);
  
  const backdrop = document.getElementById('sensor-add-modal-backdrop');
  const closeBtn = document.getElementById('sensor-add-modal-close');
  const cancelBtn = document.getElementById('sensor-add-cancel');
  const saveBtn = document.getElementById('sensor-add-save');
  
  const closeModal = () => backdrop.remove();
  
  closeBtn.addEventListener('click', closeModal);
  cancelBtn.addEventListener('click', closeModal);
  backdrop.addEventListener('click', (e) => {
    if (e.target === backdrop) closeModal();
  });
  
  saveBtn.addEventListener('click', async () => {
    const sensorData = {
      type: document.getElementById('sensor-type-select').value,
      name: document.getElementById('sensor-name-input').value,
      endpoint: document.getElementById('sensor-host-input').value + ':' + document.getElementById('sensor-port-input').value,
      mode: document.getElementById('sensor-mode-select').value,
      enabled: document.getElementById('sensor-enabled-input').checked
    };
    
    try {
      await api.sensors.create(sensorData);
      setPanelMessage(panelMsg, `Sensor added successfully`, false);
      closeModal();
      // Request fresh sensor snapshot
      ws.send({ type: 'sensor.requestSnapshot' });
    } catch (error) {
      setPanelMessage(panelMsg, `Failed to add sensor: ${error.message}`, true);
    }
  });
}

/**
 * Open sensor modal for editing
 * @param {number} id - Sensor ID
 */
function openSensorModal(id) {
  const sensor = store.getSensor(id);
  if (!sensor) return;
  
  modalSensorId = id;
  modalWorking = buildEditableSnapshot(sensor);
  renderSensorModal(modalWorking);
  
  if (modalTitle) modalTitle.textContent = `Sensor #${id} Details`;
  if (modalBackdrop) modalBackdrop.hidden = false;
  if (modalRoot) modalRoot.hidden = false;
}

/**
 * Close sensor modal
 */
function closeSensorModal() {
  modalSensorId = null;
  modalWorking = null;
  pendingSaveForId = null;
  
  if (modalBackdrop) modalBackdrop.hidden = true;
  if (modalRoot) modalRoot.hidden = true;
}

/**
 * Save sensor modal
 */
function saveSensorModal() {
  if (modalSensorId == null) return;
  
  const patch = buildPatchFromModal();
  if (!patch) return;
  
  pendingSaveForId = modalSensorId;
  ws.updateSensor(modalSensorId, patch);
}

/**
 * Build editable snapshot from sensor data
 * @param {Object} sensor - Sensor data
 * @returns {Object} Editable snapshot
 */
function buildEditableSnapshot(sensor) {
  const out = {};
  
  // Endpoint normalization
  if (sensor.endpoint != null) {
    if (typeof sensor.endpoint === 'string') {
      const [host = '', portStr = ''] = sensor.endpoint.split(':');
      out.endpoint = { host, port: Number(portStr || 0) || '' };
    } else if (typeof sensor.endpoint === 'object') {
      out.endpoint = { host: sensor.endpoint.host ?? '', port: sensor.endpoint.port ?? '' };
    }
  }
  
  if (sensor.mode != null) out.mode = String(sensor.mode);
  if (typeof sensor.ignore_checksum_error !== 'undefined') out.ignore_checksum_error = Number(sensor.ignore_checksum_error) ? 1 : 0;
  if (typeof sensor.skip_step !== 'undefined') out.skip_step = Number(sensor.skip_step) || 1;
  
  if (sensor.mask && typeof sensor.mask === 'object') {
    out.mask = {};
    if (sensor.mask.angle) {
      out.mask.angle = {
        min_deg: Number(sensor.mask.angle.min_deg ?? sensor.mask.angle.min ?? 0),
        max_deg: Number(sensor.mask.angle.max_deg ?? sensor.mask.angle.max ?? 0),
      };
    }
    if (sensor.mask.range) {
      const near = Number(sensor.mask.range.near_m ?? sensor.mask.range.min_m ?? sensor.mask.range.min ?? 0);
      const far = Number(sensor.mask.range.far_m ?? sensor.mask.range.max_m ?? sensor.mask.range.max ?? 0);
      out.mask.range = { near_m: near, far_m: far };
    }
  }
  
  return out;
}

/**
 * Render sensor modal content
 * @param {Object} edit - Editable data
 */
function renderSensorModal(edit) {
  if (!modalBody) return;
  
  modalBody.innerHTML = '';
  
  // EndPoint
  if (edit.endpoint) {
    const group = document.createElement('div');
    group.className = 'modal__group';
    group.innerHTML = `
      <h4>EndPoint</h4>
      <div class="modal__row">
        <label>Host</label>
        <input id="ep-host" type="text" value="${edit.endpoint.host}">
      </div>
      <div class="modal__row">
        <label>Port</label>
        <input id="ep-port" type="number" min="0" value="${edit.endpoint.port}">
      </div>
    `;
    modalBody.appendChild(group);
  }
  
  // Mode
  if (Object.prototype.hasOwnProperty.call(edit, 'mode')) {
    const row = document.createElement('div');
    row.className = 'modal__row';
    row.innerHTML = `<label>Mode</label><input id="mode" type="text" value="${edit.mode}">`;
    modalBody.appendChild(row);
  }
  
  // ignore_checksum_error
  if (Object.prototype.hasOwnProperty.call(edit, 'ignore_checksum_error')) {
    const row = document.createElement('div');
    row.className = 'modal__row';
    row.innerHTML = `
      <label>ignore_checksum_error</label>
      <input id="ice" type="checkbox" ${edit.ignore_checksum_error ? 'checked' : ''}>
    `;
    modalBody.appendChild(row);
  }
  
  // skip_step
  if (Object.prototype.hasOwnProperty.call(edit, 'skip_step')) {
    const row = document.createElement('div');
    row.className = 'modal__row';
    row.innerHTML = `
      <label>skip_step (>=1)</label>
      <input id="skip" type="number" min="1" value="${edit.skip_step}">
    `;
    modalBody.appendChild(row);
  }
  
  // mask
  if (edit.mask) {
    const group = document.createElement('div');
    group.className = 'modal__group';
    group.innerHTML = '<h4>Mask</h4>';
    
    if (edit.mask.angle) {
      group.innerHTML += `
        <div class="modal__row">
          <label>mask_angle (deg)</label>
          <div>
            <input id="ma-min" type="number" step="0.1" style="width:110px" value="${edit.mask.angle.min_deg}">
            <input id="ma-max" type="number" step="0.1" style="width:110px; margin-left:8px" value="${edit.mask.angle.max_deg}">
          </div>
        </div>
      `;
    }
    
    if (edit.mask.range) {
      group.innerHTML += `
        <div class="modal__row">
          <label>mask_range (m)</label>
          <div>
            <input id="mr-near" type="number" step="0.01" style="width:110px" value="${edit.mask.range.near_m}">
            <input id="mr-far" type="number" step="0.01" style="width:110px; margin-left:8px" value="${edit.mask.range.far_m}">
          </div>
        </div>
      `;
    }
    
    modalBody.appendChild(group);
  }
}

/**
 * Build patch from modal inputs
 * @returns {Object|null} Patch object
 */
function buildPatchFromModal() {
  if (modalWorking == null) return null;
  
  const patch = {};
  
  if (modalWorking.endpoint) {
    const host = document.getElementById('ep-host')?.value ?? '';
    const port = Number(document.getElementById('ep-port')?.value ?? 0);
    patch.endpoint = { host, port: Number.isFinite(port) ? port : 0 };
  }
  
  if (Object.prototype.hasOwnProperty.call(modalWorking, 'mode')) {
    patch.mode = String(document.getElementById('mode')?.value ?? '');
  }
  
  if (Object.prototype.hasOwnProperty.call(modalWorking, 'ignore_checksum_error')) {
    patch.ignore_checksum_error = document.getElementById('ice')?.checked ? 1 : 0;
  }
  
  if (Object.prototype.hasOwnProperty.call(modalWorking, 'skip_step')) {
    patch.skip_step = Math.max(1, Number(document.getElementById('skip')?.value ?? 1));
  }
  
  if (modalWorking.mask) {
    const mask = {};
    if (modalWorking.mask.angle) {
      const min = Number(document.getElementById('ma-min')?.value ?? modalWorking.mask.angle.min_deg);
      const max = Number(document.getElementById('ma-max')?.value ?? modalWorking.mask.angle.max_deg);
      mask.angle = { min_deg: min, max_deg: max };
    }
    if (modalWorking.mask.range) {
      const near = Number(document.getElementById('mr-near')?.value ?? modalWorking.mask.range.near_m);
      const far = Number(document.getElementById('mr-far')?.value ?? modalWorking.mask.range.far_m);
      mask.range = { near_m: near, far_m: far };
    }
    patch.mask = mask;
  }
  
  return patch;
}

/**
 * Delete sensor
 * @param {string} id - Sensor ID
 */
async function deleteSensor(id) {
  const sensor = store.getSensor(id);
  if (!sensor || !sensor.id) {
    setPanelMessage(panelMsg, 'Sensor not found or missing config ID', true);
    return;
  }
  
  if (!confirm(`Are you sure you want to delete sensor ${sensor.id}?`)) {
    return;
  }
  
  try {
    await api.sensors.delete(sensor.id);
    setPanelMessage(panelMsg, `Sensor ${sensor.id} deleted successfully`, false);
    // Request fresh sensor snapshot
    ws.send({ type: 'sensor.requestSnapshot' });
  } catch (error) {
    setPanelMessage(panelMsg, `Failed to delete sensor: ${error.message}`, true);
  }
}

function preserveAccordionStates() {
  accordionStates.clear();
  document.querySelectorAll('.accordion-item').forEach(item => {
    const sensorId = item.dataset.sensorId;
    if (sensorId) {
      const isExpanded = !item.classList.contains('collapsed');
      accordionStates.set(sensorId, isExpanded);
    }
  });
}

function restoreAccordionStates() {
  accordionStates.forEach((isExpanded, sensorId) => {
    const item = document.querySelector(`[data-sensor-id="${sensorId}"]`);
    if (item) {
      const header = item.querySelector('.accordion-header');
      const caret = header?.querySelector('.accordion-toggle-caret');
      
      item.classList.toggle('collapsed', !isExpanded);
      if (header) {
        header.setAttribute('aria-expanded', isExpanded ? 'true' : 'false');
      }
      if (caret) {
        caret.textContent = isExpanded ? '▼' : '▶';
      }
    }
  });
}

// WebSocket message handlers
function handleSensorSnapshot(message) {
  if (Array.isArray(message.sensors)) {
    store.clearSensors();
    for (const sensor of message.sensors) {
      store.setSensor(sensor.id, sensor);
    }
  }
}

function handleSensorUpdated(message) {
  if (message.sensor) {
    const clientId = store.get('clientId');
    // Check if this update is from another client (not our own)
    if (message.clientId && message.clientId !== clientId) {
      // Remote update: update sensor state
      store.setSensor(message.sensor.id, message.sensor);
    } else if (!message.clientId) {
      // Legacy update without clientId: treat as remote to be safe
      store.setSensor(message.sensor.id, message.sensor);
    }
  }
  
  maybeCloseModalOnAck(message);
}

function handleOkResponse(message) {
  if (message.ref) {
    setPanelMessage(panelMsg, `OK: ${message.ref}`, false);
  }
  maybeCloseModalOnAck(message);
}

function handleErrorResponse(message) {
  setPanelMessage(panelMsg, message.message || 'Error', true);
}

function maybeCloseModalOnAck(message) {
  if (pendingSaveForId == null) return;
  
  if (message.type === 'ok' && message.ref === 'sensor.update' && message.sensor && message.sensor.id === pendingSaveForId) {
    closeSensorModal();
  } else if (message.type === 'sensor.updated' && message.sensor && message.sensor.id === pendingSaveForId) {
    closeSensorModal();
  }
}