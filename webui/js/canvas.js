// Canvas drawing and viewport management

import * as store from './store.js';
import { clamp, distance, isPointInPolygon } from './utils.js';

// Import ROI module for mouse handling integration
let roiModule = null;

let canvas = null;
let ctx = null;
let animationFrameId = null;
let needsRedraw = false;

/**
 * Initialize canvas module
 */
export function init() {
  canvas = document.getElementById('cv');
  if (!canvas) {
    console.error('Canvas element not found');
    return;
  }
  
  ctx = canvas.getContext('2d');
  
  // Set up canvas resize handling
  setupResize();
  
  // Set up mouse and keyboard event handlers
  setupEventHandlers();
  
  // Subscribe to state changes that require redraw
  setupStateSubscriptions();
  
  // Start render loop
  startRenderLoop();
  
  // Import ROI module dynamically to avoid circular dependency
  import('./roi.js').then(module => {
    roiModule = module;
  });
  
  console.log('Canvas module initialized');
}

/**
 * Request a canvas redraw
 */
export function requestRedraw() {
  needsRedraw = true;
}

/**
 * Force immediate redraw
 */
export function redraw() {
  needsRedraw = true;
  render();
}

/**
 * World to screen coordinate conversion
 * @param {number} worldX - World X coordinate
 * @param {number} worldY - World Y coordinate
 * @returns {Object} Screen coordinates {x, y}
 */
export function worldToScreen(worldX, worldY) {
  const viewport = store.get('viewport');
  const cx = canvas.width / 2 + viewport.x;
  const cy = canvas.height * 0.8 + viewport.y;
  return {
    x: cx + worldX * viewport.scale,
    y: cy - worldY * viewport.scale
  };
}

/**
 * Screen to world coordinate conversion
 * @param {number} screenX - Screen X coordinate
 * @param {number} screenY - Screen Y coordinate
 * @returns {Object} World coordinates {x, y}
 */
export function screenToWorld(screenX, screenY) {
  const viewport = store.get('viewport');
  const cx = canvas.width / 2 + viewport.x;
  const cy = canvas.height * 0.8 + viewport.y;
  return {
    x: (screenX - cx) / viewport.scale,
    y: (cy - screenY) / viewport.scale
  };
}

/**
 * Get mouse position relative to canvas
 * @param {MouseEvent} e - Mouse event
 * @returns {Object} Mouse position {x, y}
 */
export function getMousePos(e) {
  const rect = canvas.getBoundingClientRect();
  const scaleX = canvas.width / rect.width;
  const scaleY = canvas.height / rect.height;
  
  return {
    x: (e.clientX - rect.left) * scaleX,
    y: (e.clientY - rect.top) * scaleY
  };
}

/**
 * Reset viewport to default position and scale
 */
export function resetViewport() {
  store.set('viewport', {
    x: 0,
    y: 0,
    scale: 60
  });
  requestRedraw();
}

// Private functions
function setupResize() {
  const resizeCanvas = () => {
    const centerCanvas = document.getElementById('center-canvas');
    if (centerCanvas) {
      const rect = centerCanvas.getBoundingClientRect();
      canvas.width = rect.width;
      canvas.height = rect.height;
    } else {
      canvas.width = window.innerWidth;
      canvas.height = Math.floor(window.innerHeight * 0.6);
    }
    requestRedraw();
  };
  
  window.addEventListener('resize', resizeCanvas);
  resizeCanvas();
}

function setupEventHandlers() {
  // Mouse events
  canvas.addEventListener('mousedown', handleMouseDown);
  canvas.addEventListener('mousemove', handleMouseMove);
  canvas.addEventListener('mouseup', handleMouseUp);
  canvas.addEventListener('wheel', handleWheel);
  canvas.addEventListener('dblclick', handleDoubleClick);
  
  // Keyboard events
  document.addEventListener('keydown', handleKeyDown);
}

function setupStateSubscriptions() {
  // Subscribe to state changes that require redraw
  const redrawKeys = [
    'rawPoints', 'filteredPoints', 'clusterItems', 'sensors', 'worldMask',
    'viewport', 'showRaw', 'showFiltered', 'perSensorColor', 'selectedSensor',
    'selectedROI', 'selectedROIType', 'selectedVertex', 'roiPoints', 'roiEditMode'
  ];
  
  redrawKeys.forEach(key => {
    store.subscribe(key, () => requestRedraw());
  });
}

function startRenderLoop() {
  const render = () => {
    if (needsRedraw) {
      drawFrame();
      needsRedraw = false;
    }
    animationFrameId = requestAnimationFrame(render);
  };
  render();
}

function drawFrame() {
  const now = performance.now();
  const state = store.getState();
  
  // Frame rate limiting for performance
  if (now - state.lastRenderTime < state.frameInterval) {
    return;
  }
  store.set('lastRenderTime', now, false);
  
  ctx.clearRect(0, 0, canvas.width, canvas.height);
  
  // Draw coordinate grid
  drawGrid();
  
  // Draw ROI regions
  drawROI();
  
  // Draw raw points if enabled
  if (state.showRaw && state.rawPoints.xy.length > 0) {
    drawPoints(state.rawPoints, '#4fc3f7', 'raw');
  }
  
  // Draw filtered points if enabled
  if (state.showFiltered && state.filteredPoints.xy.length > 0) {
    drawPoints(state.filteredPoints, '#ff9800', 'filtered');
  }
  
  // Draw clusters
  if (state.clusterItems.length > 0) {
    drawClusters();
  }
  
  // Draw sensors
  drawSensors();
  
  // Draw ROI creation preview
  if (state.roiEditMode.startsWith('create_') && state.roiPoints.length > 0) {
    drawROIPreview();
  }
}

function drawGrid() {
  const viewport = store.get('viewport');
  
  ctx.save();
  ctx.strokeStyle = '#333';
  ctx.lineWidth = 1;
  ctx.globalAlpha = 0.3;
  
  const gridSize = 1.0; // 1 meter grid for lines
  const screenGridSize = gridSize * viewport.scale;
  
  // Calculate grid offset
  const cx = canvas.width / 2 + viewport.x;
  const cy = canvas.height * 0.8 + viewport.y;
  
  const startX = cx % screenGridSize;
  const startY = cy % screenGridSize;
  
  // Draw vertical lines
  for (let x = startX; x < canvas.width; x += screenGridSize) {
    ctx.beginPath();
    ctx.moveTo(x, 0);
    ctx.lineTo(x, canvas.height);
    ctx.stroke();
  }
  
  // Draw horizontal lines
  for (let y = startY; y < canvas.height; y += screenGridSize) {
    ctx.beginPath();
    ctx.moveTo(0, y);
    ctx.lineTo(canvas.width, y);
    ctx.stroke();
  }
  
  // Draw origin
  ctx.strokeStyle = '#666';
  ctx.lineWidth = 2;
  ctx.globalAlpha = 0.8;
  ctx.beginPath();
  ctx.moveTo(cx - 10, cy);
  ctx.lineTo(cx + 10, cy);
  ctx.moveTo(cx, cy - 10);
  ctx.lineTo(cx, cy + 10);
  ctx.stroke();
  
  // Draw numeric labels
  drawGridLabels(viewport, cx, cy);
  
  ctx.restore();
}

function drawGridLabels(viewport, cx, cy) {
  const labelStep = chooseGridStepMeters(viewport.scale);
  const labelScreenStep = labelStep * viewport.scale;
  
  ctx.fillStyle = '#ddd';
  ctx.font = '12px monospace';
  ctx.textAlign = 'center';
  ctx.textBaseline = 'middle';
  ctx.globalAlpha = 0.8;
  
  // Calculate world coordinates of the center
  const worldCenter = screenToWorld(cx, cy);
  
  // Draw vertical labels (X axis)
  const startWorldX = Math.floor(worldCenter.x / labelStep) * labelStep;
  for (let worldX = startWorldX - labelStep * 10; worldX <= startWorldX + labelStep * 10; worldX += labelStep) {
    const screenPos = worldToScreen(worldX, 0);
    if (screenPos.x >= 0 && screenPos.x <= canvas.width) {
      const labelY = Math.min(Math.max(cy + 15, 15), canvas.height - 5);
      if (Math.abs(worldX) > 0.001) {
        ctx.fillText(worldX.toFixed(worldX < 1 ? 1 : 0) + 'm', screenPos.x, labelY);
      }
    }
  }
  
  // Draw horizontal labels (Y axis)
  const startWorldY = Math.floor(worldCenter.y / labelStep) * labelStep;
  for (let worldY = startWorldY - labelStep * 10; worldY <= startWorldY + labelStep * 10; worldY += labelStep) {
    const screenPos = worldToScreen(0, worldY);
    if (screenPos.y >= 0 && screenPos.y <= canvas.height) {
      const labelX = Math.min(Math.max(cx - 25, 25), canvas.width - 25);
      if (Math.abs(worldY) > 0.001) {
        ctx.fillText(worldY.toFixed(worldY < 1 ? 1 : 0) + 'm', labelX, screenPos.y);
      }
    }
  }
  
  // Draw origin label
  ctx.fillStyle = '#fff';
  ctx.fillText('0,0', cx + 15, cy - 15);
}

function chooseGridStepMeters(scalePxPerMeter) {
  const minLabelPx = 80;
  const steps = [0.1, 0.2, 0.5, 1, 2, 5, 10, 20, 50, 100, 200, 500, 1000];
  
  for (const step of steps) {
    if (step * scalePxPerMeter >= minLabelPx) {
      return step;
    }
  }
  return steps[steps.length - 1];
}

function drawPoints(points, defaultColor, type) {
  const state = store.getState();
  
  ctx.save();
  
  // Apply performance optimizations
  let optimizedPoints = points;
  
  if (state.performanceMode) {
    optimizedPoints = cullPointsToViewport(points);
    if (optimizedPoints.xy.length / 2 > state.maxPointsToRender) {
      optimizedPoints = decimatePoints(optimizedPoints, state.maxPointsToRender);
    }
  }
  
  if (state.perSensorColor && optimizedPoints.sid.length > 0) {
    drawPointsBySensor(optimizedPoints, type);
  } else {
    drawPointsSingleColor(optimizedPoints, defaultColor, type);
  }
  
  ctx.restore();
}

function drawPointsBySensor(points, type) {
  // Group points by sensor ID
  const pointsBySensor = new Map();
  for (let i = 0; i < points.xy.length; i += 2) {
    const sidIndex = Math.floor(i / 2);
    const sid = points.sid[sidIndex] || 0;
    if (!pointsBySensor.has(sid)) {
      pointsBySensor.set(sid, []);
    }
    pointsBySensor.get(sid).push(points.xy[i], points.xy[i + 1]);
  }
  
  // Draw each sensor's points in its color
  for (const [sid, pointList] of pointsBySensor) {
    ctx.fillStyle = store.getSensorColor(sid);
    ctx.beginPath();
    for (let i = 0; i < pointList.length; i += 2) {
      const screen = worldToScreen(pointList[i], pointList[i + 1]);
      if (type === 'filtered') {
        ctx.moveTo(screen.x + 2, screen.y);
        ctx.arc(screen.x, screen.y, 2, 0, Math.PI * 2);
      } else {
        ctx.rect(screen.x - 1, screen.y - 1, 2, 2);
      }
    }
    ctx.fill();
  }
}

function drawPointsSingleColor(points, color, type) {
  ctx.fillStyle = color;
  ctx.beginPath();
  for (let i = 0; i < points.xy.length; i += 2) {
    const screen = worldToScreen(points.xy[i], points.xy[i + 1]);
    if (type === 'filtered') {
      ctx.moveTo(screen.x + 2, screen.y);
      ctx.arc(screen.x, screen.y, 2, 0, Math.PI * 2);
    } else {
      ctx.rect(screen.x - 1, screen.y - 1, 2, 2);
    }
  }
  ctx.fill();
}

function drawClusters() {
  const clusterItems = store.get('clusterItems');
  
  ctx.save();
  ctx.strokeStyle = '#33c9';
  ctx.fillStyle = '#6cf9';
  ctx.lineWidth = 2;
  
  for (const cluster of clusterItems) {
    const center = worldToScreen(cluster.cx, cluster.cy);
    ctx.beginPath();
    ctx.arc(center.x, center.y, 4, 0, Math.PI * 2);
    ctx.fill();
    
    const min = worldToScreen(cluster.minx, cluster.miny);
    const max = worldToScreen(cluster.maxx, cluster.maxy);
    ctx.strokeRect(min.x, max.y, (max.x - min.x), (min.y - max.y));
  }
  
  ctx.restore();
}

function drawSensors() {
  const sensors = store.get('sensors');
  const selectedSensor = store.get('selectedSensor');
  
  ctx.save();
  
  for (const [id, sensor] of sensors.entries()) {
    if (!sensor || !sensor.pose) continue;
    
    const tx = sensor.pose.tx || 0;
    const ty = sensor.pose.ty || 0;
    const theta = (sensor.pose.theta_deg || 0) * Math.PI / 180;
    
    const pos = worldToScreen(tx, ty);
    
    // Draw sensor body
    ctx.fillStyle = selectedSensor === id ? '#ff6b6b' : '#4ecdc4';
    ctx.strokeStyle = '#2c3e50';
    ctx.lineWidth = 2;
    
    ctx.beginPath();
    ctx.arc(pos.x, pos.y, 8, 0, Math.PI * 2);
    ctx.fill();
    ctx.stroke();
    
    // Draw orientation arrow
    const arrowLength = 15;
    const arrowX = pos.x + Math.cos(theta) * arrowLength;
    const arrowY = pos.y - Math.sin(theta) * arrowLength;
    
    ctx.strokeStyle = '#2c3e50';
    ctx.lineWidth = 3;
    ctx.beginPath();
    ctx.moveTo(pos.x, pos.y);
    ctx.lineTo(arrowX, arrowY);
    ctx.stroke();
    
    // Draw sensor ID
    ctx.fillStyle = '#2c3e50';
    ctx.font = '12px sans-serif';
    ctx.textAlign = 'center';
    const displayText = sensor && sensor.id ? sensor.id : `slot${id}`;
    ctx.fillText(displayText, pos.x, pos.y - 15);
  }
  
  ctx.restore();
}

function drawROI() {
  const worldMask = store.get('worldMask');
  const selectedROI = store.get('selectedROI');
  const selectedROIType = store.get('selectedROIType');
  
  ctx.save();
  
  // Draw include regions
  for (let i = 0; i < worldMask.include.length; i++) {
    const polygon = worldMask.include[i];
    if (polygon.length < 3) continue;
    
    const isSelected = (selectedROI === i && selectedROIType === 'include');
    drawROIPolygon(polygon, isSelected, 'include');
  }
  
  // Draw exclude regions
  for (let i = 0; i < worldMask.exclude.length; i++) {
    const polygon = worldMask.exclude[i];
    if (polygon.length < 3) continue;
    
    const isSelected = (selectedROI === i && selectedROIType === 'exclude');
    drawROIPolygon(polygon, isSelected, 'exclude');
  }
  
  ctx.restore();
}

function drawROIPolygon(polygon, isSelected, type) {
  const colors = {
    include: {
      fill: isSelected ? 'rgba(46, 204, 113, 0.3)' : 'rgba(46, 204, 113, 0.2)',
      stroke: isSelected ? '#1e8449' : '#27ae60'
    },
    exclude: {
      fill: isSelected ? 'rgba(231, 76, 60, 0.3)' : 'rgba(231, 76, 60, 0.2)',
      stroke: isSelected ? '#c0392b' : '#e74c3c'
    }
  };
  
  ctx.fillStyle = colors[type].fill;
  ctx.strokeStyle = colors[type].stroke;
  ctx.lineWidth = isSelected ? 3 : 2;
  
  ctx.beginPath();
  const start = worldToScreen(polygon[0][0], polygon[0][1]);
  ctx.moveTo(start.x, start.y);
  
  for (let j = 1; j < polygon.length; j++) {
    const point = worldToScreen(polygon[j][0], polygon[j][1]);
    ctx.lineTo(point.x, point.y);
  }
  ctx.closePath();
  ctx.fill();
  ctx.stroke();
  
  // Draw vertex handles if selected
  if (isSelected) {
    drawVertexHandles(polygon, colors[type].stroke);
  }
}

function drawVertexHandles(polygon, color) {
  const selectedVertex = store.get('selectedVertex');
  
  ctx.save();
  ctx.fillStyle = color;
  ctx.strokeStyle = '#fff';
  ctx.lineWidth = 2;
  
  for (let i = 0; i < polygon.length; i++) {
    const screen = worldToScreen(polygon[i][0], polygon[i][1]);
    const isSelectedVertex = (selectedVertex === i);
    const radius = isSelectedVertex ? 6 : 4;
    
    ctx.beginPath();
    ctx.arc(screen.x, screen.y, radius, 0, Math.PI * 2);
    ctx.fill();
    ctx.stroke();
    
    if (isSelectedVertex) {
      ctx.strokeStyle = '#000';
      ctx.lineWidth = 1;
      ctx.stroke();
      ctx.strokeStyle = '#fff';
      ctx.lineWidth = 2;
    }
  }
  
  ctx.restore();
}

function drawROIPreview() {
  const roiPoints = store.get('roiPoints');
  const roiEditMode = store.get('roiEditMode');
  
  if (roiPoints.length < 2) return;
  
  ctx.save();
  ctx.strokeStyle = roiEditMode === 'create_include' ? '#27ae60' : '#e74c3c';
  ctx.fillStyle = roiEditMode === 'create_include' ? 'rgba(46, 204, 113, 0.1)' : 'rgba(231, 76, 60, 0.1)';
  ctx.lineWidth = 2;
  ctx.setLineDash([5, 5]);
  
  ctx.beginPath();
  const start = worldToScreen(roiPoints[0][0], roiPoints[0][1]);
  ctx.moveTo(start.x, start.y);
  
  for (let i = 1; i < roiPoints.length; i++) {
    const point = worldToScreen(roiPoints[i][0], roiPoints[i][1]);
    ctx.lineTo(point.x, point.y);
  }
  
  if (roiPoints.length > 2) {
    ctx.closePath();
    ctx.fill();
  }
  ctx.stroke();
  
  // Draw points
  ctx.setLineDash([]);
  ctx.fillStyle = ctx.strokeStyle;
  for (const [wx, wy] of roiPoints) {
    const screen = worldToScreen(wx, wy);
    ctx.beginPath();
    ctx.arc(screen.x, screen.y, 4, 0, Math.PI * 2);
    ctx.fill();
  }
  
  ctx.restore();
}

// Performance optimization functions
function cullPointsToViewport(points) {
  const culledXY = [];
  const culledSID = [];
  const margin = 50; // pixels margin for smooth scrolling
  
  for (let i = 0; i < points.xy.length; i += 2) {
    if (i + 1 < points.xy.length) {
      const worldX = points.xy[i];
      const worldY = points.xy[i + 1];
      const screen = worldToScreen(worldX, worldY);
      
      if (screen.x >= -margin && screen.x <= canvas.width + margin &&
          screen.y >= -margin && screen.y <= canvas.height + margin) {
        culledXY.push(worldX, worldY);
        culledSID.push(points.sid[i / 2]);
      }
    }
  }
  
  return {
    xy: culledXY,
    sid: culledSID
  };
}

function decimatePoints(points, maxPoints) {
  if (points.xy.length / 2 <= maxPoints) {
    return points;
  }
  
  const step = Math.ceil((points.xy.length / 2) / maxPoints);
  const decimatedXY = [];
  const decimatedSID = [];
  
  for (let i = 0; i < points.xy.length; i += step * 2) {
    if (i + 1 < points.xy.length) {
      decimatedXY.push(points.xy[i], points.xy[i + 1]);
      decimatedSID.push(points.sid[i / 2]);
    }
  }
  
  return {
    xy: decimatedXY,
    sid: decimatedSID
  };
}

// Event handlers
function handleMouseDown(e) {
  const pos = getMousePos(e);
  const state = store.getState();
  
  store.update({
    dragStart: { x: pos.x, y: pos.y },
    isDragging: true
  });
  
  if (state.roiEditMode.startsWith('create_')) {
    // Add point to ROI
    const world = screenToWorld(pos.x, pos.y);
    const newPoints = [...state.roiPoints, [world.x, world.y]];
    store.set('roiPoints', newPoints);
    return;
  }
  
  // Check if clicking on a sensor
  const sensorId = findSensorAt(pos.x, pos.y);
  if (sensorId !== null) {
    store.update({
      selectedSensor: sensorId,
      selectedROI: null,
      selectedROIType: null,
      selectedVertex: null,
      dragMode: 'sensor'
    });
    requestRedraw();
    return;
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
      store.update({
        selectedVertex: vertex.index,
        dragMode: 'vertex'
      });
      requestRedraw();
      return;
    }
  }
  
  const roi = findROIAt(pos.x, pos.y);
  if (roi) {
    store.update({
      selectedROI: roi.index,
      selectedROIType: roi.type,
      selectedVertex: null,
      selectedSensor: null,
      dragMode: 'roi'
    });
    requestRedraw();
    return;
  }
  
  // Default to pan mode
  store.update({
    selectedSensor: null,
    selectedROI: null,
    selectedROIType: null,
    selectedVertex: null,
    dragMode: 'pan'
  });
  requestRedraw();
}

function handleMouseMove(e) {
  const state = store.getState();
  if (!state.isDragging) return;
  
  const pos = getMousePos(e);
  const dx = pos.x - state.dragStart.x;
  const dy = pos.y - state.dragStart.y;
  
  if (state.dragMode === 'pan') {
    const viewport = state.viewport;
    store.set('viewport', {
      ...viewport,
      x: viewport.x + dx,
      y: viewport.y + dy
    });
  } else if (state.dragMode === 'sensor' && state.selectedSensor !== null) {
    // Move sensor
    const sensor = store.getSensor(state.selectedSensor);
    if (sensor && sensor.pose) {
      const worldDelta = screenToWorld(dx, dy);
      const worldOrigin = screenToWorld(0, 0);
      
      const newTx = (sensor.pose.tx || 0) + (worldDelta.x - worldOrigin.x);
      const newTy = (sensor.pose.ty || 0) + (worldDelta.y - worldOrigin.y);
      
      // Update local state immediately for smooth dragging
      sensor.pose.tx = newTx;
      sensor.pose.ty = newTy;
      
      // Trigger canvas redraw
      store.set('sensors', store.get('sensors'), true);
    }
  } else if (state.dragMode === 'vertex' && state.selectedROI !== null && state.selectedROIType && state.selectedVertex !== null) {
    // Move vertex
    const polygon = state.selectedROIType === 'include' ?
      state.worldMask.include[state.selectedROI] :
      state.worldMask.exclude[state.selectedROI];
    if (polygon && state.selectedVertex < polygon.length) {
      const worldPos = screenToWorld(pos.x, pos.y);
      polygon[state.selectedVertex][0] = worldPos.x;
      polygon[state.selectedVertex][1] = worldPos.y;
      requestRedraw();
    }
  } else if (state.dragMode === 'roi' && state.selectedROI !== null && state.selectedROIType) {
    // Move entire ROI
    const polygon = state.selectedROIType === 'include' ?
      state.worldMask.include[state.selectedROI] :
      state.worldMask.exclude[state.selectedROI];
    if (polygon) {
      const worldDelta = screenToWorld(dx, dy);
      const worldOrigin = screenToWorld(0, 0);
      const deltaX = worldDelta.x - worldOrigin.x;
      const deltaY = worldDelta.y - worldOrigin.y;
      
      for (let i = 0; i < polygon.length; i++) {
        polygon[i][0] += deltaX;
        polygon[i][1] += deltaY;
      }
      requestRedraw();
    }
  }
  
  store.set('dragStart', { x: pos.x, y: pos.y });
}

function handleMouseUp(e) {
  const state = store.getState();
  
  if (state.isDragging && state.dragMode === 'sensor' && state.selectedSensor !== null) {
    // Send sensor update to server
    const sensor = store.getSensor(state.selectedSensor);
    if (sensor && sensor.pose) {
      const patch = {
        pose: {
          tx: sensor.pose.tx,
          ty: sensor.pose.ty,
          theta_deg: sensor.pose.theta_deg || 0
        }
      };
      
      // Send with client ID for sender identification
      import('./ws.js').then(ws => {
        ws.updateSensor(state.selectedSensor, patch, store.get('clientId'));
      });
    }
  } else if (state.isDragging && (state.dragMode === 'vertex' || state.dragMode === 'roi') && state.selectedROI !== null) {
    // Send world mask update to server
    import('./ws.js').then(ws => {
      ws.send({
        type: 'world.update',
        patch: {
          world_mask: {
            includes: state.worldMask.include,
            excludes: state.worldMask.exclude
          }
        }
      });
    });
  }
  
  store.update({
    isDragging: false,
    dragMode: 'pan'
  });
}

function handleWheel(e) {
  e.preventDefault();
  
  const pos = getMousePos(e);
  const worldPos = screenToWorld(pos.x, pos.y);
  const viewport = store.get('viewport');
  
  const zoomFactor = e.deltaY > 0 ? 0.9 : 1.1;
  const newScale = clamp(viewport.scale * zoomFactor, 10, 200);
  
  // Zoom towards mouse position
  const scaleDelta = newScale - viewport.scale;
  const newViewport = {
    x: viewport.x - worldPos.x * scaleDelta,
    y: viewport.y + worldPos.y * scaleDelta,
    scale: newScale
  };
  
  store.set('viewport', newViewport);
}

function handleDoubleClick(e) {
  resetViewport();
}

function handleKeyDown(e) {
  if (e.target.tagName === 'INPUT' || e.target.tagName === 'TEXTAREA') return;
  
  const state = store.getState();
  const viewport = state.viewport;
  
  switch (e.key) {
    case 'ArrowLeft':
      store.set('viewport', { ...viewport, x: viewport.x + 20 });
      break;
    case 'ArrowRight':
      store.set('viewport', { ...viewport, x: viewport.x - 20 });
      break;
    case 'ArrowUp':
      store.set('viewport', { ...viewport, y: viewport.y + 20 });
      break;
    case 'ArrowDown':
      store.set('viewport', { ...viewport, y: viewport.y - 20 });
      break;
    case '+':
    case '=':
      store.set('viewport', { ...viewport, scale: Math.min(200, viewport.scale * 1.1) });
      break;
    case '-':
      store.set('viewport', { ...viewport, scale: Math.max(10, viewport.scale * 0.9) });
      break;
    case 'p':
    case 'P':
      store.togglePerformanceMode();
      break;
    case 'r':
    case 'R':
      if(state.selectedSensor !== null) {
        const sensor = store.getSensor(state.selectedSensor);
        if (sensor && sensor.pose) {
          const d = 15 * (e.key === 'R' ? -1 : 1);
          sensor.pose.theta_deg = (sensor.pose.theta_deg || 0) + d;
          const patch = {
            pose: {
              theta_deg: sensor.pose.theta_deg
            }
          };
          import('./ws.js').then(ws => {
            ws.updateSensor(state.selectedSensor, patch, store.get('clientId'));
          });
        }
      }
      break;
    case 'Escape':
      // Cancel ROI creation or clear selection
      store.update({
        roiPoints: [],
        roiEditMode: 'none',
        selectedSensor: null,
        selectedROI: null,
        selectedROIType: null,
        selectedVertex: null
      });
      break;
  }
}

// Helper functions for mouse interactions
function findSensorAt(screenX, screenY) {
  const sensors = store.get('sensors');
  for (const [id, sensor] of sensors) {
    if (!sensor.pose) continue;
    
    const pos = worldToScreen(sensor.pose.tx || 0, sensor.pose.ty || 0);
    const dx = screenX - pos.x;
    const dy = screenY - pos.y;
    const distance = Math.sqrt(dx * dx + dy * dy);
    
    if (distance <= 12) { // sensor radius + margin
      return id;
    }
  }
  return null;
}

function findROIAt(screenX, screenY) {
  const worldPos = screenToWorld(screenX, screenY);
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

function findVertexAt(screenX, screenY, roi) {
  if (!roi) return null;
  
  const threshold = 8; // pixels
  
  for (let i = 0; i < roi.polygon.length; i++) {
    const vertex = roi.polygon[i];
    const screen = worldToScreen(vertex[0], vertex[1]);
    const dx = screenX - screen.x;
    const dy = screenY - screen.y;
    const distance = Math.sqrt(dx * dx + dy * dy);
    
    if (distance <= threshold) {
      return { index: i, vertex: vertex };
    }
  }
  
  return null;
}

// Export canvas and context for direct access if needed
export { canvas, ctx };