// ROI creation and editing functionality

import * as store from './store.js';
import * as ws from './ws.js';
import * as canvas from './canvas.js';
import { isPointInPolygon, distance } from './utils.js';
import { ROITypes, ROIEditModes } from './types.js';

// UI elements
let btnCreateInclude = null;
let btnCreateExclude = null;
let btnClearROI = null;
let roiInstructions = null;

/**
 * Initialize ROI module
 */
export function init() {
  // Get UI elements
  btnCreateInclude = document.getElementById('btn-create-include');
  btnCreateExclude = document.getElementById('btn-create-exclude');
  btnClearROI = document.getElementById('btn-clear-roi');
  roiInstructions = document.getElementById('roi-instructions');
  
  // Set up event handlers
  setupEventHandlers();
  
  // Subscribe to state changes
  store.subscribe('roiEditMode', updateROIInstructions);
  store.subscribe('roiEditMode', updateButtonStates);
  store.subscribe('roiPoints', updateROIInstructions);
  
  // Register WebSocket message handlers
  ws.registerHandler('world.updated', handleWorldUpdated);
  
  // Set up keyboard handlers
  setupKeyboardHandlers();
  
  // Initialize UI state
  updateROIInstructions();
  updateButtonStates();
  
  console.log('ROI module initialized');
}

/**
 * Set up event handlers
 */
function setupEventHandlers() {
  // Create include region button
  if (btnCreateInclude) {
    btnCreateInclude.addEventListener('click', () => {
      const currentMode = store.get('roiEditMode');
      if (currentMode === ROIEditModes.CREATE_INCLUDE) {
        // Cancel current creation
        cancelROICreation();
      } else {
        // Start include region creation
        startROICreation(ROIEditModes.CREATE_INCLUDE);
      }
    });
  }
  
  // Create exclude region button
  if (btnCreateExclude) {
    btnCreateExclude.addEventListener('click', () => {
      const currentMode = store.get('roiEditMode');
      if (currentMode === ROIEditModes.CREATE_EXCLUDE) {
        // Cancel current creation
        cancelROICreation();
      } else {
        // Start exclude region creation
        startROICreation(ROIEditModes.CREATE_EXCLUDE);
      }
    });
  }
  
  // Clear all ROI button
  if (btnClearROI) {
    btnClearROI.addEventListener('click', () => {
      if (confirm('Clear all ROI regions?')) {
        clearAllROI();
      }
    });
  }
}

/**
 * Set up keyboard handlers
 */
function setupKeyboardHandlers() {
  document.addEventListener('keydown', (e) => {
    if (e.target.tagName === 'INPUT' || e.target.tagName === 'TEXTAREA') return;
    
    const state = store.getState();
    
    switch (e.key) {
      case 'Enter':
        if (state.roiEditMode.startsWith('create_') && state.roiPoints.length >= 3) {
          finishROICreation();
        }
        break;
        
      case 'Escape':
        cancelROICreation();
        clearSelection();
        break;
        
      case 'Delete':
      case 'Backspace':
        if (state.selectedVertex !== null && state.selectedROI !== null && state.selectedROIType) {
          deleteSelectedVertex();
        } else if (state.selectedROI !== null && state.selectedROIType) {
          deleteSelectedROI();
        }
        break;
        
      case 'Insert':
        if (state.selectedVertex !== null && state.selectedROI !== null && state.selectedROIType) {
          addVertexAfterSelected();
        }
        break;
    }
  });
}

/**
 * Start ROI creation
 * @param {string} mode - ROI edit mode
 */
function startROICreation(mode) {
  store.update({
    roiEditMode: mode,
    roiPoints: [],
    selectedSensor: null,
    selectedROI: null,
    selectedROIType: null,
    selectedVertex: null
  });
  
  updateCanvasCursor();
}

/**
 * Cancel ROI creation
 */
function cancelROICreation() {
  store.update({
    roiEditMode: ROIEditModes.NONE,
    roiPoints: [],
    selectedSensor: null,
    selectedROI: null,
    selectedROIType: null,
    selectedVertex: null
  });
  
  updateCanvasCursor();
}

/**
 * Clear selection
 */
function clearSelection() {
  store.update({
    selectedSensor: null,
    selectedROI: null,
    selectedROIType: null,
    selectedVertex: null
  });
}

/**
 * Finish ROI creation
 */
function finishROICreation() {
  const state = store.getState();
  
  if (!state.roiEditMode.startsWith('create_') || state.roiPoints.length < 3) {
    return;
  }
  
  const regionType = state.roiEditMode === ROIEditModes.CREATE_INCLUDE ? 'include' : 'exclude';
  const worldMask = { ...state.worldMask };
  worldMask[regionType] = [...worldMask[regionType], [...state.roiPoints]];
  
  store.update({
    worldMask: worldMask,
    roiPoints: [],
    roiEditMode: ROIEditModes.NONE
  });
  
  // Send world mask update to server
  sendWorldMaskUpdate();
  updateCanvasCursor();
}

/**
 * Clear all ROI regions
 */
function clearAllROI() {
  const worldMask = {
    include: [],
    exclude: []
  };
  
  store.set('worldMask', worldMask);
  sendWorldMaskUpdate();
}

/**
 * Delete selected ROI
 */
function deleteSelectedROI() {
  const state = store.getState();
  
  if (state.selectedROI === null || !state.selectedROIType) return;
  
  const worldMask = { ...state.worldMask };
  worldMask[state.selectedROIType].splice(state.selectedROI, 1);
  
  store.update({
    worldMask: worldMask,
    selectedROI: null,
    selectedROIType: null,
    selectedVertex: null
  });
  
  sendWorldMaskUpdate();
}

/**
 * Delete selected vertex
 */
function deleteSelectedVertex() {
  const state = store.getState();
  
  if (state.selectedVertex === null || state.selectedROI === null || !state.selectedROIType) return;
  
  const worldMask = { ...state.worldMask };
  const polygon = worldMask[state.selectedROIType][state.selectedROI];
  
  if (polygon.length > 3) { // Keep at least 3 vertices for a valid polygon
    polygon.splice(state.selectedVertex, 1);
    
    store.update({
      worldMask: worldMask,
      selectedVertex: null
    });
    
    sendWorldMaskUpdate();
  }
}

/**
 * Add vertex after selected vertex
 */
function addVertexAfterSelected() {
  const state = store.getState();
  
  if (state.selectedVertex === null || state.selectedROI === null || !state.selectedROIType) return;
  
  const worldMask = { ...state.worldMask };
  const polygon = worldMask[state.selectedROIType][state.selectedROI];
  const currentVertex = polygon[state.selectedVertex];
  const nextVertex = polygon[(state.selectedVertex + 1) % polygon.length];
  
  // Calculate midpoint
  const midPoint = [
    (currentVertex[0] + nextVertex[0]) / 2,
    (currentVertex[1] + nextVertex[1]) / 2
  ];
  
  polygon.splice(state.selectedVertex + 1, 0, midPoint);
  
  store.update({
    worldMask: worldMask,
    selectedVertex: state.selectedVertex + 1
  });
  
  sendWorldMaskUpdate();
}

/**
 * Handle mouse click for ROI operations
 * @param {Object} pos - Mouse position {x, y}
 * @returns {boolean} True if click was handled
 */
export function handleMouseClick(pos) {
  const state = store.getState();
  
  if (state.roiEditMode.startsWith('create_')) {
    // Add point to ROI
    const world = canvas.screenToWorld(pos.x, pos.y);
    const newPoints = [...state.roiPoints, [world.x, world.y]];
    store.set('roiPoints', newPoints);
    return true;
  }
  
  // Check if clicking on a vertex of selected ROI
  if (state.selectedROI !== null && state.selectedROIType) {
    const roi = {
      type: state.selectedROIType,
      index: state.selectedROI,
      polygon: state.selectedROIType === 'include' ? 
        state.worldMask.include[state.selectedROI] : 
        state.worldMask.exclude[state.selectedROI]
    };
    
    const vertex = findVertexAt(pos.x, pos.y, roi);
    if (vertex) {
      store.set('selectedVertex', vertex.index);
      return true;
    }
  }
  
  // Check if clicking on a ROI region
  const roi = findROIAt(pos.x, pos.y);
  if (roi) {
    store.update({
      selectedROI: roi.index,
      selectedROIType: roi.type,
      selectedVertex: null,
      selectedSensor: null
    });
    return true;
  }
  
  return false;
}

/**
 * Handle mouse drag for ROI operations
 * @param {Object} pos - Mouse position {x, y}
 * @param {Object} dragStart - Drag start position {x, y}
 * @returns {boolean} True if drag was handled
 */
export function handleMouseDrag(pos, dragStart) {
  const state = store.getState();
  
  if (state.dragMode === 'vertex' && state.selectedROI !== null && state.selectedROIType && state.selectedVertex !== null) {
    // Move vertex
    const worldMask = { ...state.worldMask };
    const polygon = worldMask[state.selectedROIType][state.selectedROI];
    
    if (polygon && state.selectedVertex < polygon.length) {
      const worldPos = canvas.screenToWorld(pos.x, pos.y);
      polygon[state.selectedVertex][0] = worldPos.x;
      polygon[state.selectedVertex][1] = worldPos.y;
      
      store.set('worldMask', worldMask);
    }
    return true;
  }
  
  if (state.dragMode === 'roi' && state.selectedROI !== null && state.selectedROIType) {
    // Move entire ROI
    const worldMask = { ...state.worldMask };
    const polygon = worldMask[state.selectedROIType][state.selectedROI];
    
    if (polygon) {
      const dx = pos.x - dragStart.x;
      const dy = pos.y - dragStart.y;
      const worldDelta = canvas.screenToWorld(dx, dy);
      const worldOrigin = canvas.screenToWorld(0, 0);
      const deltaX = worldDelta.x - worldOrigin.x;
      const deltaY = worldDelta.y - worldOrigin.y;
      
      for (let i = 0; i < polygon.length; i++) {
        polygon[i][0] += deltaX;
        polygon[i][1] += deltaY;
      }
      
      store.set('worldMask', worldMask);
    }
    return true;
  }
  
  return false;
}

/**
 * Handle mouse up for ROI operations
 * @returns {boolean} True if mouse up was handled
 */
export function handleMouseUp() {
  const state = store.getState();
  
  if (state.isDragging && (state.dragMode === 'vertex' || state.dragMode === 'roi') && state.selectedROI !== null) {
    // Send world mask update to server
    sendWorldMaskUpdate();
    return true;
  }
  
  return false;
}

/**
 * Find ROI at screen coordinates
 * @param {number} screenX - Screen X coordinate
 * @param {number} screenY - Screen Y coordinate
 * @returns {Object|null} ROI info or null
 */
function findROIAt(screenX, screenY) {
  const worldPos = canvas.screenToWorld(screenX, screenY);
  const worldMask = store.get('worldMask');
  
  // Check include regions first
  for (let i = 0; i < worldMask.include.length; i++) {
    const polygon = worldMask.include[i];
    if (polygon.length >= 3 && isPointInPolygon(worldPos, polygon)) {
      return { type: 'include', index: i, polygon: polygon };
    }
  }
  
  // Check exclude regions
  for (let i = 0; i < worldMask.exclude.length; i++) {
    const polygon = worldMask.exclude[i];
    if (polygon.length >= 3 && isPointInPolygon(worldPos, polygon)) {
      return { type: 'exclude', index: i, polygon: polygon };
    }
  }
  
  return null;
}

/**
 * Find vertex at screen coordinates
 * @param {number} screenX - Screen X coordinate
 * @param {number} screenY - Screen Y coordinate
 * @param {Object} roi - ROI object
 * @returns {Object|null} Vertex info or null
 */
function findVertexAt(screenX, screenY, roi) {
  if (!roi) return null;
  
  const threshold = 8; // pixels
  
  for (let i = 0; i < roi.polygon.length; i++) {
    const vertex = roi.polygon[i];
    const screen = canvas.worldToScreen(vertex[0], vertex[1]);
    const dist = distance({ x: screenX, y: screenY }, { x: screen.x, y: screen.y });
    
    if (dist <= threshold) {
      return { index: i, vertex: vertex };
    }
  }
  
  return null;
}

/**
 * Send world mask update to server
 */
function sendWorldMaskUpdate() {
  const worldMask = store.get('worldMask');
  ws.updateWorldMask(worldMask);
}

/**
 * Update ROI instructions display
 */
function updateROIInstructions() {
  if (!roiInstructions) return;
  
  const state = store.getState();
  
  if (state.roiEditMode.startsWith('create_')) {
    const regionType = state.roiEditMode === ROIEditModes.CREATE_INCLUDE ? 'Include' : 'Exclude';
    roiInstructions.textContent = `Creating ${regionType} Region: Click points, Enter to finish, Esc to cancel`;
    roiInstructions.style.color = state.roiEditMode === ROIEditModes.CREATE_INCLUDE ? '#28a745' : '#dc3545';
  } else {
    roiInstructions.textContent = 'Pan: Drag | Zoom: Wheel | Sensors: Drag to move, R to rotate | ROI: Click points, Enter to finish';
    roiInstructions.style.color = '#6c757d';
  }
}

/**
 * Update button states
 */
function updateButtonStates() {
  const roiEditMode = store.get('roiEditMode');
  
  if (btnCreateInclude) {
    btnCreateInclude.classList.toggle('active', roiEditMode === ROIEditModes.CREATE_INCLUDE);
    btnCreateInclude.classList.toggle('roi-creating', roiEditMode === ROIEditModes.CREATE_INCLUDE);
  }
  
  if (btnCreateExclude) {
    btnCreateExclude.classList.toggle('active', roiEditMode === ROIEditModes.CREATE_EXCLUDE);
    btnCreateExclude.classList.toggle('roi-creating', roiEditMode === ROIEditModes.CREATE_EXCLUDE);
  }
}

/**
 * Update canvas cursor based on ROI mode
 */
function updateCanvasCursor() {
  const canvasElement = canvas.canvas;
  if (!canvasElement) return;
  
  const roiEditMode = store.get('roiEditMode');
  const selectedSensor = store.get('selectedSensor');
  
  if (roiEditMode.startsWith('create_')) {
    canvasElement.className = 'canvas-roi';
  } else if (selectedSensor !== null) {
    canvasElement.className = 'canvas-sensor';
  } else {
    canvasElement.className = 'canvas-pan';
  }
}

// WebSocket message handlers
function handleWorldUpdated(message) {
  if (message.world_mask) {
    store.set('worldMask', {
      include: message.world_mask.includes || [],
      exclude: message.world_mask.excludes || []
    });
  }
}

/**
 * Load world mask from snapshot
 * @param {Object} snapshot - Snapshot data
 */
export function loadWorldMaskFromSnapshot(snapshot) {
  if (snapshot && snapshot.world_mask) {
    store.set('worldMask', {
      include: snapshot.world_mask.includes || [],
      exclude: snapshot.world_mask.excludes || []
    });
  }
}