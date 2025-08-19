// UI elements
const stats = document.getElementById('stats');
const connectionStatus = document.getElementById('connection-status');
const seqInfo = document.getElementById('seq-info');
const lastSeqAgeEl = document.getElementById('last-seq-age');
const fpsInfo = document.getElementById('fps-info');
const cv = document.getElementById('cv');
const ctx = cv.getContext('2d');
const showRawCheckbox = document.getElementById('show-raw-points');
const showFilteredCheckbox = document.getElementById('show-filtered-data');
const perSensorColorsCheckbox = document.getElementById('per-sensor-colors');
const legend = document.getElementById('legend');
const legendItems = document.getElementById('legend-items');

// Display state
let showRaw = false;
let showFiltered = false;
let perSensorColor = false;

let sensors = new Map();

// Connection stats
let connectionCount = 0;
let disconnectionCount = 0;
let errorCount = 0;

// Frame data
let lastSeq = 0;
let lastFrameTime = 0; // t_ns from message
let lastReceiveTime = 0; // local timestamp
let rawPoints = { xy: [], sid: [] };
let filteredPoints = { xy: [], sid: [] };
let clusterItems = [];

// FPS tracking
let frameTimestamps = [];
let lastFPS = 0;

// Performance settings
let performanceMode = false;
let maxPointsToRender = 10000;
let lastRenderTime = 0;
let targetFPS = 60;
let frameInterval = 1000 / targetFPS;

// Color palette for sensors (stable mapping)
const sensorColors = new Map();
const colorPalette = [
  '#e74c3c', '#3498db', '#2ecc71', '#f39c12', '#9b59b6',
  '#1abc9c', '#e67e22', '#34495e', '#f1c40f', '#95a5a6',
  '#c0392b', '#2980b9', '#27ae60', '#d35400', '#8e44ad'
];

// Viewport state
let viewport = {
  x: 0,      // pan offset x
  y: 0,      // pan offset y
  scale: 60  // pixels per meter (zoom)
};

// Interaction state
let isDragging = false;
let dragStart = { x: 0, y: 0 };
let dragMode = 'pan'; // 'pan', 'sensor', 'roi'
let selectedSensor = null;
let selectedROI = null;
let selectedROIType = null; // 'include' or 'exclude'
let selectedVertex = null;
let roiEditMode = 'none'; // 'none', 'create_include', 'create_exclude', 'edit'
let roiPoints = []; // temporary points during creation

// ROI state
let worldMask = {
  include: [],
  exclude: []
};

function resize(){
  const centerCanvas = document.getElementById('center-canvas');
  if (centerCanvas) {
    const rect = centerCanvas.getBoundingClientRect();
    cv.width = rect.width;
    cv.height = rect.height;
  } else {
    // Fallback for when center-canvas is not available
    cv.width = window.innerWidth;
    cv.height = Math.floor(window.innerHeight * 0.6);
  }
  redrawCanvas();
}
window.addEventListener('resize', resize); resize();

const wsProto = location.protocol === 'https:' ? 'wss' : 'ws';
const ws = new WebSocket(`${wsProto}://${location.host}/ws/live`);

// Helper functions
function getSensorColor(sid) {
  if (!sensorColors.has(sid)) {
    const colorIndex = sensorColors.size % colorPalette.length;
    sensorColors.set(sid, colorPalette[colorIndex]);
  }
  return sensorColors.get(sid);
}

function updateStatsDisplay() {
  connectionStatus.textContent = ws.readyState === WebSocket.OPEN ? 'connected' :
                                 ws.readyState === WebSocket.CONNECTING ? 'connecting...' :
                                 ws.readyState === WebSocket.CLOSING ? 'closing...' : 'disconnected';
  
  const rawCount = rawPoints.xy.length/2;
  const filteredCount = filteredPoints.xy.length/2;
  seqInfo.textContent = `seq=${lastSeq} clusters=${clusterItems.length} raw=${rawCount} filtered=${filteredCount}`;
  
  // Last seq age
  if (lastFrameTime > 0) {
    const ageMs = Date.now() - lastReceiveTime;
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
  }
  
  fpsInfo.textContent = `fps=${lastFPS}`;
}

function updateFPS() {
  const now = performance.now();
  frameTimestamps.push(now);
  
  // Keep only last 30 frames for FPS calculation
  if (frameTimestamps.length > 30) {
    frameTimestamps.shift();
  }
  
  if (frameTimestamps.length >= 2) {
    const timeSpan = frameTimestamps[frameTimestamps.length - 1] - frameTimestamps[0];
    lastFPS = Math.round((frameTimestamps.length - 1) * 1000 / timeSpan);
  }
}

ws.onopen = () => {
  connectionCount++;
  updateStatsDisplay();
};

ws.onclose = () => {
  disconnectionCount++;
  updateStatsDisplay();
};

ws.onerror = () => {
  errorCount++;
  updateStatsDisplay();
};

ws.onmessage = (ev) => {
  try{
    const m = JSON.parse(ev.data);
    lastReceiveTime = Date.now();
    
    if(m.type === 'clusters-lite'){
      lastSeq = m.seq;
      lastFrameTime = m.t || 0;
      clusterItems = m.items || [];
      updateFPS();
      redrawCanvas();
      updateStatsDisplay();
    } else if(m.type === 'raw-lite'){
      lastSeq = m.seq;
      lastFrameTime = m.t || 0;
      rawPoints.xy = m.xy || [];
      rawPoints.sid = m.sid || [];
      updateFPS();
      redrawCanvas();
      updateStatsDisplay();
      updateLegend();
    } else if(m.type === 'filtered-lite'){
      filteredPoints.xy = m.xy || [];
      filteredPoints.sid = m.sid || [];
      redrawCanvas();
      updateStatsDisplay();
      updateLegend();
    } else if(m.type === 'filter.config'){
      // フィルター設定を受信
      if(m.config){
        currentFilterConfig = m.config;
        applyFilterConfigToUI(m.config);
        setFilterMessage('Filter configuration received from server');
      }
    } else if(m.type === 'filter.updated'){
      // フィルター設定更新通知を受信
      if(m.config){
        currentFilterConfig = m.config;
        applyFilterConfigToUI(m.config);
        setFilterMessage('Filter configuration updated');
      }
    }
  }catch(e){ /* ignore */ }
};

function redrawCanvas(){
  const now = performance.now();
  
  // Frame rate limiting for performance
  if (now - lastRenderTime < frameInterval) {
    requestAnimationFrame(redrawCanvas);
    return;
  }
  lastRenderTime = now;
  
  ctx.clearRect(0,0,cv.width,cv.height);
  
  // Draw coordinate grid
  drawGrid();
  
  // Draw ROI regions
  drawROI();
  
  // Draw raw points if enabled
  if (showRaw && rawPoints.xy.length > 0) {
    drawPoints(rawPoints, '#4fc3f7', 'raw');
  }
  
  // Draw filtered points if enabled
  if (showFiltered && filteredPoints.xy.length > 0) {
    drawPoints(filteredPoints, '#ff9800', 'filtered');
  }
  
  // Draw clusters
  if (clusterItems.length > 0) {
    drawClusters();
  }
  
  // Draw sensors
  drawSensors();
  
  // Draw ROI creation preview
  if (roiEditMode.startsWith('create_') && roiPoints.length > 0) {
    drawROIPreview();
  }
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

function isPointInViewport(worldX, worldY) {
  const screen = worldToScreen(worldX, worldY);
  const margin = 50; // pixels margin for smooth scrolling
  return screen.x >= -margin && screen.x <= cv.width + margin &&
         screen.y >= -margin && screen.y <= cv.height + margin;
}

function cullPointsToViewport(points) {
  if (!performanceMode) return points;
  
  const culledXY = [];
  const culledSID = [];
  
  for (let i = 0; i < points.xy.length; i += 2) {
    if (i + 1 < points.xy.length) {
      const worldX = points.xy[i];
      const worldY = points.xy[i + 1];
      
      if (isPointInViewport(worldX, worldY)) {
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

function togglePerformanceMode() {
  performanceMode = !performanceMode;
  showNotification(
    `Performance mode ${performanceMode ? 'enabled' : 'disabled'}. ${
      performanceMode ? 'Large point clouds will be optimized for better frame rate.' : 'Full quality rendering restored.'
    }`,
    'info'
  );
  redrawCanvas();
}

function worldToScreen(worldX, worldY) {
  const cx = cv.width/2 + viewport.x;
  const cy = cv.height*0.8 + viewport.y;
  return {
    x: cx + worldX * viewport.scale,
    y: cy - worldY * viewport.scale
  };
}

function screenToWorld(screenX, screenY) {
  const cx = cv.width/2 + viewport.x;
  const cy = cv.height*0.8 + viewport.y;
  return {
    x: (screenX - cx) / viewport.scale,
    y: (cy - screenY) / viewport.scale
  };
}

function chooseGridStepMeters(scalePxPerMeter) {
  const minLabelPx = 80; // Minimum pixels between labels
  const steps = [0.1, 0.2, 0.5, 1, 2, 5, 10, 20, 50, 100, 200, 500, 1000];
  
  for (const step of steps) {
    if (step * scalePxPerMeter >= minLabelPx) {
      return step;
    }
  }
  return steps[steps.length - 1]; // fallback to largest step
}

function drawGrid() {
  ctx.save();
  ctx.strokeStyle = '#333';
  ctx.lineWidth = 1;
  ctx.globalAlpha = 0.3;
  
  const gridSize = 1.0; // 1 meter grid for lines
  const screenGridSize = gridSize * viewport.scale;
  
  // Calculate grid offset
  const cx = cv.width/2 + viewport.x;
  const cy = cv.height*0.8 + viewport.y;
  
  const startX = cx % screenGridSize;
  const startY = cy % screenGridSize;
  
  // Draw vertical lines
  for (let x = startX; x < cv.width; x += screenGridSize) {
    ctx.beginPath();
    ctx.moveTo(x, 0);
    ctx.lineTo(x, cv.height);
    ctx.stroke();
  }
  
  // Draw horizontal lines
  for (let y = startY; y < cv.height; y += screenGridSize) {
    ctx.beginPath();
    ctx.moveTo(0, y);
    ctx.lineTo(cv.width, y);
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
    if (screenPos.x >= 0 && screenPos.x <= cv.width) {
      // Draw label near the horizontal axis
      const labelY = Math.min(Math.max(cy + 15, 15), cv.height - 5);
      if (Math.abs(worldX) > 0.001) { // Don't label the origin
        ctx.fillText(worldX.toFixed(worldX < 1 ? 1 : 0) + 'm', screenPos.x, labelY);
      }
    }
  }
  
  // Draw horizontal labels (Y axis)
  const startWorldY = Math.floor(worldCenter.y / labelStep) * labelStep;
  for (let worldY = startWorldY - labelStep * 10; worldY <= startWorldY + labelStep * 10; worldY += labelStep) {
    const screenPos = worldToScreen(0, worldY);
    if (screenPos.y >= 0 && screenPos.y <= cv.height) {
      // Draw label near the vertical axis
      const labelX = Math.min(Math.max(cx - 25, 25), cv.width - 25);
      if (Math.abs(worldY) > 0.001) { // Don't label the origin
        ctx.fillText(worldY.toFixed(worldY < 1 ? 1 : 0) + 'm', labelX, screenPos.y);
      }
    }
  }
  
  // Draw origin label
  ctx.fillStyle = '#fff';
  ctx.fillText('0,0', cx + 15, cy - 15);
  
  ctx.restore();
}

function drawPoints(points, defaultColor, type){
  ctx.save();
  
  // Apply performance optimizations
  let optimizedPoints = points;
  
  if (performanceMode) {
    // Cull points outside viewport
    optimizedPoints = cullPointsToViewport(points);
    
    // Decimate points if too many
    if (optimizedPoints.xy.length / 2 > maxPointsToRender) {
      optimizedPoints = decimatePoints(optimizedPoints, maxPointsToRender);
    }
  }
  
  if (perSensorColor && optimizedPoints.sid.length > 0) {
    // Group points by sensor ID for colored drawing
    const pointsBySensor = new Map();
    for (let i = 0; i < optimizedPoints.xy.length; i += 2) {
      const sidIndex = Math.floor(i / 2);
      const sid = optimizedPoints.sid[sidIndex] || 0;
      if (!pointsBySensor.has(sid)) {
        pointsBySensor.set(sid, []);
      }
      pointsBySensor.get(sid).push(optimizedPoints.xy[i], optimizedPoints.xy[i + 1]);
    }
    
    // Draw each sensor's points in its color
    for (const [sid, pointList] of pointsBySensor) {
      ctx.fillStyle = getSensorColor(sid);
      ctx.beginPath();
      for (let i = 0; i < pointList.length; i += 2) {
        const screen = worldToScreen(pointList[i], pointList[i + 1]);
        if (type === 'filtered') {
          // Draw filtered points as circles
          ctx.moveTo(screen.x + 2, screen.y);
          ctx.arc(screen.x, screen.y, 2, 0, Math.PI * 2);
        } else {
          // Draw raw points as squares
          ctx.rect(screen.x - 1, screen.y - 1, 2, 2);
        }
      }
      ctx.fill();
    }
  } else {
    // Single color for all points
    ctx.fillStyle = defaultColor;
    ctx.beginPath();
    for (let i = 0; i < optimizedPoints.xy.length; i += 2) {
      const screen = worldToScreen(optimizedPoints.xy[i], optimizedPoints.xy[i + 1]);
      if (type === 'filtered') {
        // Draw filtered points as circles
        ctx.moveTo(screen.x + 2, screen.y);
        ctx.arc(screen.x, screen.y, 2, 0, Math.PI * 2);
      } else {
        // Draw raw points as squares
        ctx.rect(screen.x - 1, screen.y - 1, 2, 2);
      }
    }
    ctx.fill();
  }
  
  ctx.restore();
}

function drawClusters(){
  ctx.save();
  ctx.strokeStyle = '#33c9';
  ctx.fillStyle = '#6cf9';
  ctx.lineWidth = 2;
  
  for(const c of clusterItems){
    const center = worldToScreen(c.cx, c.cy);
    ctx.beginPath();
    ctx.arc(center.x, center.y, 4, 0, Math.PI*2);
    ctx.fill();
    
    const min = worldToScreen(c.minx, c.miny);
    const max = worldToScreen(c.maxx, c.maxy);
    ctx.strokeRect(min.x, max.y, (max.x - min.x), (min.y - max.y));
  }
  
  ctx.restore();
}

function drawSensors() {
  ctx.save();
  
  for (let [id, sensor] of sensors.entries()) {
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
    
    // Draw sensor config ID and slot index
    ctx.fillStyle = '#2c3e50';
    ctx.font = '12px sans-serif';
    ctx.textAlign = 'center';
    const displayText = sensor && sensor.id ? sensor.id : `slot${id}`;
    ctx.fillText(displayText, pos.x, pos.y - 15);
  }
  
  ctx.restore();
}

function drawROI() {
  ctx.save();
  
  // Draw include regions
  for (let i = 0; i < worldMask.include.length; i++) {
    const polygon = worldMask.include[i];
    if (polygon.length < 3) continue;
    
    const isSelected = (selectedROI === i && selectedROIType === 'include');
    
    ctx.fillStyle = isSelected ? 'rgba(46, 204, 113, 0.3)' : 'rgba(46, 204, 113, 0.2)';
    ctx.strokeStyle = isSelected ? '#1e8449' : '#27ae60';
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
      drawVertexHandles(polygon, '#27ae60');
    }
  }
  
  // Draw exclude regions
  for (let i = 0; i < worldMask.exclude.length; i++) {
    const polygon = worldMask.exclude[i];
    if (polygon.length < 3) continue;
    
    const isSelected = (selectedROI === i && selectedROIType === 'exclude');
    
    ctx.fillStyle = isSelected ? 'rgba(231, 76, 60, 0.3)' : 'rgba(231, 76, 60, 0.2)';
    ctx.strokeStyle = isSelected ? '#c0392b' : '#e74c3c';
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
      drawVertexHandles(polygon, '#e74c3c');
    }
  }
  
  ctx.restore();
}

function drawVertexHandles(polygon, color) {
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
// Legend management
function updateLegend() {
  if (!perSensorColor) {
    legend.hidden = true;
    return;
  }
  
  // Get unique sensor IDs from both raw and filtered points
  const allSids = [];
  if (showRaw && rawPoints.sid.length > 0) {
    allSids.push(...rawPoints.sid);
  }
  if (showFiltered && filteredPoints.sid.length > 0) {
    allSids.push(...filteredPoints.sid);
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
    swatch.style.backgroundColor = getSensorColor(sid);
    
    const label = document.createElement('span');
    label.textContent = `Sensor ${sid}`;
    
    item.appendChild(swatch);
    item.appendChild(label);
    legendItems.appendChild(item);
  }
}

// UI event handlers
showRawCheckbox.addEventListener('change', () => {
  showRaw = showRawCheckbox.checked;
  updateLegend();
  redrawCanvas();
});

showFilteredCheckbox.addEventListener('change', () => {
  showFiltered = showFilteredCheckbox.checked;
  updateLegend();
  redrawCanvas();
});

perSensorColorsCheckbox.addEventListener('change', () => {
  perSensorColor = perSensorColorsCheckbox.checked;
  updateLegend();
  redrawCanvas();
});

// Periodic stats update
setInterval(() => {
  updateStatsDisplay();
}, 1000);

// ======== 追加：センサー状態 管理/描画/操作 ========
const tbody = document.getElementById('sensor-tbody');
const btnRefresh = document.getElementById('btn-refresh');
const panelMsg = document.getElementById('panel-msg');

function setPanelMsg(text, ok=true){
  panelMsg.textContent = text || '';
  panelMsg.style.color = ok ? '' : 'crimson';
  if (text) setTimeout(()=>{ panelMsg.textContent=''; }, 2000);
}

function renderSensors(){
  tbody.innerHTML = '';
  const rows = Array.from(sensors.values()).sort((a,b)=>{
    // Sort by slot index (id field in JSON response)
    return a.id - b.id;
  });
  console.log('Rendering sensors:', rows.map(s => ({
    ...s
  })));
  for(const s of rows){
    const tr = document.createElement('tr');
    // Display config ID but use slot index for operations
    tr.innerHTML = `
      <td style="padding:6px 8px; border-bottom:1px solid #eee;">${s.id}</td>
      <td style="padding:6px 8px; border-bottom:1px solid #eee;">
        <label class="toggle">
          <input type="checkbox" ${s.enabled ? 'checked' : ''} data-sensor-id="${s.id}" />
          <span>${s.enabled ? 'ON' : 'OFF'}</span>
        </label>
      </td>
      <td style="padding:6px 8px; border-bottom:1px solid #eee;">
        <input class="pose-tx" type="number" step="0.01" value="${Number(s.pose?.tx ?? s.tx ?? 0)}" data-sensor-id="${s.id}" style="width:100px" />
      </td>
      <td style="padding:6px 8px; border-bottom:1px solid #eee;">
        <input class="pose-ty" type="number" step="0.01" value="${Number(s.pose?.ty ?? s.ty ?? 0)}" data-sensor-id="${s.id}" style="width:100px" />
      </td>
      <td style="padding:6px 8px; border-bottom:1px solid #eee;">
        <input class="pose-theta" type="number" step="0.1" value="${Number(s.pose?.theta_deg ?? s.theta_deg ?? 0)}" data-sensor-id="${s.id}" style="width:100px" />
      </td>
      <td style="padding:6px 8px; border-bottom:1px solid #eee; text-align:right;">
        <button class="btn" data-open-modal="${s.id}">Details…</button>
        <button class="btn" data-delete-sensor="${s.id}" style="margin-left: 4px; background-color: #dc3545; color: white;">Delete</button>
      </td>
    `;

    // Enabled → sensor.enable (use slot index)
    const chk = tr.querySelector('input[type=checkbox]');
    const lbl = tr.querySelector('span');
    chk.addEventListener('change', () => {
      lbl.textContent = chk.checked ? 'ON' : 'OFF';
      try { ws.send(JSON.stringify({ type:'sensor.enable', id: Number(chk.dataset.slotIndex), enabled: chk.checked })); }
      catch(e){ setPanelMsg('send failed', false); }
    });

    // tx/ty/theta → blurで {pose:{...}} patch (use slot index)
    const onPoseBlur = (cls, key) => {
      const input = tr.querySelector(cls);
      input.addEventListener('blur', () => {
        const sensorId = Number(input.dataset.sensorId);
        const v = Number(input.value || 0);
        const patch = { pose: {} };
        patch.pose[key] = v;
        ws.send(JSON.stringify({ type:'sensor.update', id: sensorId, patch }));
      });
    };
    onPoseBlur('.pose-tx', 'tx');
    onPoseBlur('.pose-ty', 'ty');
    onPoseBlur('.pose-theta', 'theta_deg');

    // モーダル起動 (use slot index)
    tr.querySelector('[data-open-modal]').addEventListener('click', () => openSensorModal(s.id));

    // Delete sensor (use slot index)
    tr.querySelector('[data-delete-sensor]').addEventListener('click', () => deleteSensor(s.id));

    tbody.appendChild(tr);
  }
}

// 受信ハンドラの拡張：状態メッセージ
const prevOnMessage = ws.onmessage;
ws.onmessage = (ev) => {
  // 既存の clusters-lite ハンドリングを先に実行
  if (typeof prevOnMessage === 'function') {
    try { prevOnMessage.call(ws, ev); } catch(_e){}
  }
  // 以降：状態系の処理
  try{
    const m = JSON.parse(ev.data);
    if (m.type === 'sensor.snapshot' && Array.isArray(m.sensors)) {
      sensors.clear();
      for(const s of m.sensors) sensors.set(s.id, s);
      renderSensors();
      maybeCloseModalOnAck(m);
      
      // フィルター設定がスナップショットに含まれている場合は適用
      if (m.filter_config) {
        currentFilterConfig = m.filter_config;
        applyFilterConfigToUI(m.filter_config);
        setFilterMessage('Filter configuration loaded from server');
      }
    } else if (m.type === 'sensor.updated' && m.sensor && typeof m.sensor.id === 'number') {
      sensors.set(m.sensor.id, m.sensor);
      renderSensors();
      maybeCloseModalOnAck(m);
    } else if (m.type === 'filter.config') {
      // フィルター設定を受信
      if(m.config){
        currentFilterConfig = m.config;
        applyFilterConfigToUI(m.config);
        setFilterMessage('Filter configuration received from server');
      }
    } else if (m.type === 'filter.updated') {
      // フィルター設定更新通知を受信
      if(m.config){
        currentFilterConfig = m.config;
        applyFilterConfigToUI(m.config);
        setFilterMessage('Filter configuration updated');
      }
    } else if (m.type === 'ok') {
      // 任意：応答OKのとき軽く表示
      setPanelMsg(`ok: ${m.ref ?? ''}`);
      maybeCloseModalOnAck(m);
    } else if (m.type === 'error') {
      setPanelMsg(m.message || 'error', false);
    }
    
    // Handle publishers information in snapshots
    if (m.publishers && m.publishers.sinks) {
      renderPublishers(m.publishers.sinks);
    }
  }catch(e){ /* ignore */ }
};

// 接続時に snapshot 要求（サーバが自動送信しない環境でも取得できるように）
btnRefresh?.addEventListener('click', ()=>{
  try {
    ws.send(JSON.stringify({ type:'sensor.requestSnapshot' }));
    ws.send(JSON.stringify({ type:'filter.requestConfig' }));
  }
  catch(e){ setPanelMsg('send failed', false); }
});
ws.addEventListener('open', ()=>{
  try {
    ws.send(JSON.stringify({ type:'sensor.requestSnapshot' }));
    ws.send(JSON.stringify({ type:'filter.requestConfig' }));
  } catch(_e){}
});


// --- modal refs/state ---
const mBackdrop = document.getElementById('sensor-modal-backdrop');
const mRoot     = document.getElementById('sensor-modal');
const mTitle    = document.getElementById('sensor-modal-title');
const mBody     = document.getElementById('sensor-modal-body');
const mClose    = document.getElementById('sensor-modal-close');
const mCancel   = document.getElementById('sensor-modal-cancel');
const mSave     = document.getElementById('sensor-modal-save');

let modalSensorId = null;
let modalWorking  = null;         // モーダルに描画中の「存在する項目だけ」を保持
let pendingSaveForId = null;      // ← Save後、サーバ応答待ちのID

function openSensorModal(id){
  const s = sensors.get(id);
  if(!s) return;
  modalSensorId = id;
  modalWorking = buildEditableSnapshot(s);
  renderSensorModal(modalWorking);
  mTitle.textContent = `Sensor #${id} Details`;
  mBackdrop.hidden = false; mRoot.hidden = false;
}
function closeSensorModal(){
  modalSensorId = null; modalWorking = null;
  pendingSaveForId = null;
  mBackdrop.hidden = true; mRoot.hidden = true;
}
mClose?.addEventListener('click', closeSensorModal);
mCancel?.addEventListener('click', closeSensorModal);
mBackdrop?.addEventListener('click', (e)=>{ if(e.target===mBackdrop) closeSensorModal(); });

// 現在のセンサー JSON から「存在するキーだけ」を UI 用に抽出
function buildEditableSnapshot(s){
  const out = {};
  // endpoint -> {host,port} へ正規化
  if (s.endpoint != null) {
    if (typeof s.endpoint === 'string') {
      const [host='', portStr=''] = s.endpoint.split(':');
      out.endpoint = { host, port: Number(portStr || 0) || '' };
    } else if (typeof s.endpoint === 'object') {
      out.endpoint = { host: s.endpoint.host ?? '', port: s.endpoint.port ?? '' };
    }
  }
  if (s.mode != null) out.mode = String(s.mode);
  if (typeof s.ignore_checksum_error !== 'undefined') out.ignore_checksum_error = Number(s.ignore_checksum_error) ? 1 : 0;
  if (typeof s.skip_step !== 'undefined') out.skip_step = Number(s.skip_step) || 1;

  if (s.mask && typeof s.mask === 'object') {
    out.mask = {};
    if (s.mask.angle) {
      out.mask.angle = {
        min_deg: Number(s.mask.angle.min_deg ?? s.mask.angle.min ?? 0),
        max_deg: Number(s.mask.angle.max_deg ?? s.mask.angle.max ?? 0),
      };
    }
    if (s.mask.range) {
      const near = Number(s.mask.range.near_m ?? s.mask.range.min_m ?? s.mask.range.min ?? 0);
      const far  = Number(s.mask.range.far_m  ?? s.mask.range.max_m ?? s.mask.range.max ?? 0);
      out.mask.range = { near_m: near, far_m: far };
    }
  }
  return out;
}

function renderSensorModal(edit){
  mBody.innerHTML = '';

  // EndPoint
  if (edit.endpoint) {
    const g = document.createElement('div'); g.className='modal__group';
    g.innerHTML = `<h4>EndPoint</h4>
      <div class="modal__row"><label>Host</label><input id="ep-host" type="text" value="${edit.endpoint.host}"></div>
      <div class="modal__row"><label>Port</label><input id="ep-port" type="number" min="0" value="${edit.endpoint.port}"></div>`;
    mBody.appendChild(g);
  }

  // Mode
  if (Object.prototype.hasOwnProperty.call(edit, 'mode')) {
    const r = document.createElement('div'); r.className='modal__row';
    r.innerHTML = `<label>Mode</label><input id="mode" type="text" value="${edit.mode}">`;
    mBody.appendChild(r);
  }

  // ignore_checksum_error
  if (Object.prototype.hasOwnProperty.call(edit, 'ignore_checksum_error')) {
    const r = document.createElement('div'); r.className='modal__row';
    r.innerHTML = `<label>ignore_checksum_error</label>
      <input id="ice" type="checkbox" ${edit.ignore_checksum_error ? 'checked':''}>`;
    mBody.appendChild(r);
  }

  // skip_step
  if (Object.prototype.hasOwnProperty.call(edit, 'skip_step')) {
    const r = document.createElement('div'); r.className='modal__row';
    r.innerHTML = `<label>skip_step (>=1)</label>
      <input id="skip" type="number" min="1" value="${edit.skip_step}">`;
    mBody.appendChild(r);
  }

  // mask
  if (edit.mask) {
    const g = document.createElement('div'); g.className='modal__group'; g.innerHTML = `<h4>Mask</h4>`;
    if (edit.mask.angle) {
      g.innerHTML += `<div class="modal__row"><label>mask_angle (deg)</label>
        <div>
          <input id="ma-min" type="number" step="0.1" style="width:110px" value="${edit.mask.angle.min_deg}">
          <input id="ma-max" type="number" step="0.1" style="width:110px; margin-left:8px" value="${edit.mask.angle.max_deg}">
        </div></div>`;
    }
    if (edit.mask.range) {
      g.innerHTML += `<div class="modal__row"><label>mask_range (m)</label>
        <div>
          <input id="mr-near" type="number" step="0.01" style="width:110px" value="${edit.mask.range.near_m}">
          <input id="mr-far"  type="number" step="0.01" style="width:110px; margin-left:8px" value="${edit.mask.range.far_m}">
        </div></div>`;
    }
    mBody.appendChild(g);
  }

  // Save：送信のみ（ここでは閉じない）。サーバ応答で閉じる。
  mSave.onclick = () => {
    if (modalSensorId == null) return;
    const patch = buildPatchFromModal();
    if (!patch) return;
    pendingSaveForId = modalSensorId;        // ★ 応答待ちIDを記録
    ws.send(JSON.stringify({ type:'sensor.update', id: modalSensorId, patch }));
  };
}

function buildPatchFromModal(){
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
    const m = {};
    if (modalWorking.mask.angle) {
      const min = Number(document.getElementById('ma-min')?.value ?? modalWorking.mask.angle.min_deg);
      const max = Number(document.getElementById('ma-max')?.value ?? modalWorking.mask.angle.max_deg);
      m.angle = { min_deg: min, max_deg: max };
    }
    if (modalWorking.mask.range) {
      const near = Number(document.getElementById('mr-near')?.value ?? modalWorking.mask.range.near_m);
      const far  = Number(document.getElementById('mr-far')?.value  ?? modalWorking.mask.range.far_m);
      m.range = { near_m: near, far_m: far };
    }
    patch.mask = m;
  }
  return patch;
}

// サーバ応答で閉じる（ok/sensor.updated のどちらでも）
function maybeCloseModalOnAck(m){
  if (pendingSaveForId == null) return;
  if (m.type === 'ok' && m.ref === 'sensor.update' && m.sensor && m.sensor.id === pendingSaveForId) {
    closeSensorModal();
  } else if (m.type === 'sensor.updated' && m.sensor && m.sensor.id === pendingSaveForId) {
    closeSensorModal();
  }
}

// サーバから更新を受けたらモーダルの内容も最新へ
function tryRefreshModalFromState(){
  if (modalSensorId == null) return;
  const s = sensors.get(modalSensorId);
  if (!s) return;
  modalWorking = buildEditableSnapshot(s);
  renderSensorModal(modalWorking, s);
}

// Delete sensor function
function deleteSensor(slotIndex) {
  console.log('Attempting to delete sensor with slot index:', slotIndex);
  
  // Get the actual sensor config ID from the sensor data
  const sensor = sensors.get(slotIndex);
  if (!sensor || !sensor.id) {
    setPanelMsg('Sensor not found or missing config ID', false);
    return;
  }
  
  const sensorConfigId = sensor.id;
  console.log('Deleting sensor with config ID:', sensorConfigId);
  
  if (!confirm(`Are you sure you want to delete sensor ${sensorConfigId}?`)) {
    return;
  }
  
  try {
    // Use the string config ID for deletion, not the slot index
    fetch(`/api/v1/sensors/${encodeURIComponent(sensorConfigId)}`, {
      method: 'DELETE',
      headers: {
        'Content-Type': 'application/json'
      }
    }).then(response => {
      console.log('Delete sensor response status:', response.status);
      if (response.ok) {
        setPanelMsg(`Sensor ${sensorConfigId} deleted successfully`, true);
        // Refresh sensor list
        ws.send(JSON.stringify({ type: 'sensor.requestSnapshot' }));
      } else {
        response.json().then(error => {
          console.error('Delete sensor error:', error);
          setPanelMsg(`Failed to delete sensor: ${error.message}`, false);
        }).catch(() => {
          setPanelMsg(`Failed to delete sensor: HTTP ${response.status}`, false);
        });
      }
    }).catch(e => {
      console.error('Delete sensor network error:', e);
      setPanelMsg(`Failed to delete sensor: ${e.message}`, false);
    });
  } catch (e) {
    console.error('Delete sensor exception:', e);
    setPanelMsg(`Failed to delete sensor: ${e.message}`, false);
  }
}

// ======== Filter Configuration Management ========
const filterElements = {
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
  resetFiltersBtn: document.getElementById('btn-reset-filters'),
  filterMsg: document.getElementById('filter-msg')
};

// Filter configuration state
let currentFilterConfig = {
  prefilter: {
    enabled: true,
    neighborhood: { enabled: true, k: 5, r_base: 0.05, r_scale: 1.0 },
    spike_removal: { enabled: true, dr_threshold: 0.3, window_size: 3 },
    outlier_removal: { enabled: true, median_window: 5, outlier_threshold: 2.0 },
    intensity_filter: { enabled: false, min_intensity: 0, min_reliability: 0 },
    isolation_removal: { enabled: true, min_cluster_size: 3, isolation_radius: 0.1 }
  },
  postfilter: {
    enabled: true,
    isolation_removal: { enabled: true, min_points_size: 3, isolation_radius: 0.2, required_neighbors: 2 }
  }
};

function setFilterMessage(text, isError = false) {
  if (filterElements.filterMsg) {
    filterElements.filterMsg.textContent = text || '';
    filterElements.filterMsg.className = isError ? 'error' : '';
    if (text) {
      setTimeout(() => { filterElements.filterMsg.textContent = ''; }, 3000);
    }
  }
}

function updateFilterUIState() {
  // Update prefilter section state
  const prefilterSection = document.querySelector('.filter-section:first-child');
  if (prefilterSection) {
    prefilterSection.classList.toggle('disabled', !currentFilterConfig.prefilter.enabled);
  }
  
  // Update postfilter section state
  const postfilterSection = document.querySelector('.filter-section:last-child');
  if (postfilterSection) {
    postfilterSection.classList.toggle('disabled', !currentFilterConfig.postfilter.enabled);
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

function applyFilterConfigToUI(config) {
  // Prefilter settings
  if (filterElements.prefilterEnabled) filterElements.prefilterEnabled.checked = config.prefilter.enabled;
  
  if (filterElements.prefilterNeighborhoodEnabled) filterElements.prefilterNeighborhoodEnabled.checked = config.prefilter.neighborhood.enabled;
  if (filterElements.prefilterNeighborhoodK) filterElements.prefilterNeighborhoodK.value = config.prefilter.neighborhood.k;
  if (filterElements.prefilterNeighborhoodRBase) filterElements.prefilterNeighborhoodRBase.value = config.prefilter.neighborhood.r_base;
  if (filterElements.prefilterNeighborhoodRScale) filterElements.prefilterNeighborhoodRScale.value = config.prefilter.neighborhood.r_scale;
  
  if (filterElements.prefilterSpikeEnabled) filterElements.prefilterSpikeEnabled.checked = config.prefilter.spike_removal.enabled;
  if (filterElements.prefilterSpikeDrThreshold) filterElements.prefilterSpikeDrThreshold.value = config.prefilter.spike_removal.dr_threshold;
  if (filterElements.prefilterSpikeWindowSize) filterElements.prefilterSpikeWindowSize.value = config.prefilter.spike_removal.window_size;
  
  if (filterElements.prefilterOutlierEnabled) filterElements.prefilterOutlierEnabled.checked = config.prefilter.outlier_removal.enabled;
  if (filterElements.prefilterOutlierMedianWindow) filterElements.prefilterOutlierMedianWindow.value = config.prefilter.outlier_removal.median_window;
  if (filterElements.prefilterOutlierThreshold) filterElements.prefilterOutlierThreshold.value = config.prefilter.outlier_removal.outlier_threshold;
  
  if (filterElements.prefilterIntensityEnabled) filterElements.prefilterIntensityEnabled.checked = config.prefilter.intensity_filter.enabled;
  if (filterElements.prefilterIntensityMin) filterElements.prefilterIntensityMin.value = config.prefilter.intensity_filter.min_intensity;
  if (filterElements.prefilterIntensityReliability) filterElements.prefilterIntensityReliability.value = config.prefilter.intensity_filter.min_reliability;
  
  if (filterElements.prefilterIsolationEnabled) filterElements.prefilterIsolationEnabled.checked = config.prefilter.isolation_removal.enabled;
  if (filterElements.prefilterIsolationMinSize) filterElements.prefilterIsolationMinSize.value = config.prefilter.isolation_removal.min_cluster_size;
  if (filterElements.prefilterIsolationRadius) filterElements.prefilterIsolationRadius.value = config.prefilter.isolation_removal.isolation_radius;
  
  // Postfilter settings
  if (filterElements.postfilterEnabled) filterElements.postfilterEnabled.checked = config.postfilter.enabled;
  if (filterElements.postfilterIsolationEnabled) filterElements.postfilterIsolationEnabled.checked = config.postfilter.isolation_removal.enabled;
  if (filterElements.postfilterIsolationMinSize) filterElements.postfilterIsolationMinSize.value = config.postfilter.isolation_removal.min_points_size;
  if (filterElements.postfilterIsolationRadius) filterElements.postfilterIsolationRadius.value = config.postfilter.isolation_removal.isolation_radius;
  if (filterElements.postfilterIsolationRequiredNeighbors) filterElements.postfilterIsolationRequiredNeighbors.value = config.postfilter.isolation_removal.required_neighbors;

  updateFilterUIState();
}

function sendFilterConfig() {
  const config = loadFilterConfigFromUI();
  currentFilterConfig = config;
  
  try {
    ws.send(JSON.stringify({
      type: 'filter.update',
      config: config
    }));
    setFilterMessage('Filter configuration updated');
  } catch (e) {
    setFilterMessage('Failed to send filter configuration', true);
  }
}

function resetFilterConfig() {
  const defaultConfig = {
    prefilter: {
      enabled: true,
      neighborhood: { enabled: true, k: 5, r_base: 0.05, r_scale: 1.0 },
      spike_removal: { enabled: true, dr_threshold: 0.3, window_size: 3 },
      outlier_removal: { enabled: true, median_window: 5, outlier_threshold: 2.0 },
      intensity_filter: { enabled: false, min_intensity: 0, min_reliability: 0 },
      isolation_removal: { enabled: true, min_cluster_size: 3, isolation_radius: 0.1 }
    },
    postfilter: {
      enabled: true,
      isolation_removal: { enabled: true, min_points_size: 3, isolation_radius: 0.2, required_neighbors: 2 }
    }
  };
  
  applyFilterConfigToUI(defaultConfig);
  sendFilterConfig();
}

// Event listeners for filter controls
function setupFilterEventListeners() {
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
      input.addEventListener('change', sendFilterConfig);
      input.addEventListener('blur', sendFilterConfig);
    }
  });
  
  // Reset button
  if (filterElements.resetFiltersBtn) {
    filterElements.resetFiltersBtn.addEventListener('click', resetFilterConfig);
  }
}

// Initialize filter controls
document.addEventListener('DOMContentLoaded', () => {
  setupFilterEventListeners();
  updateFilterUIState();
});

// Mouse and touch interaction handlers
function getMousePos(e) {
  const rect = cv.getBoundingClientRect();
  // Account for any CSS scaling or transformations
  const scaleX = cv.width / rect.width;
  const scaleY = cv.height / rect.height;
  
  return {
    x: (e.clientX - rect.left) * scaleX,
    y: (e.clientY - rect.top) * scaleY
  };
}

function findSensorAt(screenX, screenY) {
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
  // TODO: 候補のpolygonの中で一番小さなものを返す
  
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

function isPointInPolygon(point, polygon) {
  let inside = false;
  for (let i = 0, j = polygon.length - 1; i < polygon.length; j = i++) {
    const xi = polygon[i][0], yi = polygon[i][1];
    const xj = polygon[j][0], yj = polygon[j][1];
    
    if (((yi > point.y) !== (yj > point.y)) &&
        (point.x < (xj - xi) * (point.y - yi) / (yj - yi) + xi)) {
      inside = !inside;
    }
  }
  return inside;
}

function deleteSelectedROI() {
  if (selectedROI !== null && selectedROIType) {
    if (selectedROIType === 'include') {
      worldMask.include.splice(selectedROI, 1);
    } else {
      worldMask.exclude.splice(selectedROI, 1);
    }
    
    selectedROI = null;
    selectedROIType = null;
    selectedVertex = null;
    
    sendWorldMaskUpdate();
    redrawCanvas();
  }
}

function deleteVertexFromROI(roiType, roiIndex, vertexIndex) {
  const polygon = roiType === 'include' ? worldMask.include[roiIndex] : worldMask.exclude[roiIndex];
  
  if (polygon.length > 3) { // Keep at least 3 vertices for a valid polygon
    polygon.splice(vertexIndex, 1);
    sendWorldMaskUpdate();
    redrawCanvas();
  }
}

function addVertexToROI(roiType, roiIndex, vertexIndex, worldPos) {
  const polygon = roiType === 'include' ? worldMask.include[roiIndex] : worldMask.exclude[roiIndex];
  polygon.splice(vertexIndex + 1, 0, [worldPos.x, worldPos.y]);
  sendWorldMaskUpdate();
  redrawCanvas();
}

// Canvas mouse event handlers
cv.addEventListener('mousedown', (e) => {
  const pos = getMousePos(e);
  dragStart = { x: pos.x, y: pos.y };
  isDragging = true;
  
  if (roiEditMode.startsWith('create_')) {
    // Add point to ROI
    const world = screenToWorld(pos.x, pos.y);
    roiPoints.push([world.x, world.y]);
    redrawCanvas();
    return;
  }
  
  // Check if clicking on a sensor
  const sensorId = findSensorAt(pos.x, pos.y);
  if (sensorId !== null) {
    selectedSensor = sensorId;
    selectedROI = null;
    selectedROIType = null;
    selectedVertex = null;
    dragMode = 'sensor';
    redrawCanvas();
    return;
  }
  
  // Check if clicking on a vertex of selected ROI
  if (selectedROI !== null && selectedROIType) {
    const roi = {
      type: selectedROIType,
      index: selectedROI,
      polygon: selectedROIType === 'include' ? worldMask.include[selectedROI] : worldMask.exclude[selectedROI]
    };
    const vertex = findVertexAt(pos.x, pos.y, roi);
    if (vertex) {
      selectedVertex = vertex.index;
      dragMode = 'vertex';
      redrawCanvas();
      return;
    }
  }
  
  const roi = findROIAt(pos.x, pos.y);
  if (roi) {
    selectedROI = roi.index;
    selectedROIType = roi.type;
    selectedVertex = null;
    selectedSensor = null;
    dragMode = 'roi';
    redrawCanvas();
    return;
  }
  
  // Default to pan mode
  selectedSensor = null;
  selectedROI = null;
  selectedROIType = null;
  selectedVertex = null;
  dragMode = 'pan';
  redrawCanvas();
});

cv.addEventListener('mousemove', (e) => {
  if (!isDragging) return;
  
  const pos = getMousePos(e);
  const dx = pos.x - dragStart.x;
  const dy = pos.y - dragStart.y;
  
  if (dragMode === 'pan') {
    viewport.x += dx;
    viewport.y += dy;
    redrawCanvas();
  } else if (dragMode === 'sensor' && selectedSensor !== null) {
    // Move sensor
    const sensor = sensors.get(selectedSensor);
    if (sensor && sensor.pose) {
      const worldDelta = screenToWorld(dx, dy);
      const worldOrigin = screenToWorld(0, 0);
      
      const newTx = (sensor.pose.tx || 0) + (worldDelta.x - worldOrigin.x);
      const newTy = (sensor.pose.ty || 0) + (worldDelta.y - worldOrigin.y);
      
      // Update local state immediately for smooth dragging
      sensor.pose.tx = newTx;
      sensor.pose.ty = newTy;
      
      redrawCanvas();
    }
  } else if (dragMode === 'vertex' && selectedROI !== null && selectedROIType && selectedVertex !== null) {
    // Move vertex
    const polygon = selectedROIType === 'include' ? worldMask.include[selectedROI] : worldMask.exclude[selectedROI];
    if (polygon && selectedVertex < polygon.length) {
      const worldPos = screenToWorld(pos.x, pos.y);
      polygon[selectedVertex][0] = worldPos.x;
      polygon[selectedVertex][1] = worldPos.y;
      redrawCanvas();
    }
  } else if (dragMode === 'roi' && selectedROI !== null && selectedROIType) {
    // Move entire ROI
    const polygon = selectedROIType === 'include' ? worldMask.include[selectedROI] : worldMask.exclude[selectedROI];
    if (polygon) {
      const worldDelta = screenToWorld(dx, dy);
      const worldOrigin = screenToWorld(0, 0);
      const deltaX = worldDelta.x - worldOrigin.x;
      const deltaY = worldDelta.y - worldOrigin.y;
      
      for (let i = 0; i < polygon.length; i++) {
        polygon[i][0] += deltaX;
        polygon[i][1] += deltaY;
      }
      redrawCanvas();
    }
  }
  
  dragStart = { x: pos.x, y: pos.y };
});

cv.addEventListener('mouseup', (e) => {
  if (isDragging && dragMode === 'sensor' && selectedSensor !== null) {
    // Send sensor update to server
    const sensor = sensors.get(selectedSensor);
    if (sensor && sensor.pose) {
      const patch = {
        pose: {
          tx: sensor.pose.tx,
          ty: sensor.pose.ty,
          theta_deg: sensor.pose.theta_deg || 0
        }
      };
      
      try {
        ws.send(JSON.stringify({
          type: 'sensor.update',
          id: selectedSensor,
          patch: patch
        }));
      } catch (e) {
        console.error('Failed to send sensor update:', e);
      }
    }
  } else if (isDragging && (dragMode === 'vertex' || dragMode === 'roi') && selectedROI !== null) {
    // Send world mask update to server
    sendWorldMaskUpdate();
  }
  
  isDragging = false;
  dragMode = 'pan';
});

// Mouse wheel for zooming
cv.addEventListener('wheel', (e) => {
  e.preventDefault();
  
  const pos = getMousePos(e);
  const worldPos = screenToWorld(pos.x, pos.y);
  
  const zoomFactor = e.deltaY > 0 ? 0.9 : 1.1;
  const newScale = Math.max(10, Math.min(200, viewport.scale * zoomFactor));
  
  // Zoom towards mouse position
  const scaleDelta = newScale - viewport.scale;
  viewport.x -= worldPos.x * scaleDelta;
  viewport.y += worldPos.y * scaleDelta;
  viewport.scale = newScale;
  
  updateViewportInfo();
  redrawCanvas();
});

// Double-click to reset viewport
cv.addEventListener('dblclick', (e) => {
  viewport.x = 0;
  viewport.y = 0;
  viewport.scale = 60;
  redrawCanvas();
});

// Keyboard shortcuts
document.addEventListener('keydown', (e) => {
  if (e.target.tagName === 'INPUT' || e.target.tagName === 'TEXTAREA') return;
  
  // TODO: arrawキーは、選択しているROI,vertex,sensorがある場合はそれを移動する
  switch (e.key) {
    case 'ArrowLeft':
      viewport.x += 20;
      redrawCanvas();
      break;
    case 'ArrowRight':
      viewport.x -= 20;
      redrawCanvas();
      break;
    case 'ArrowUp':
      viewport.y += 20;
      redrawCanvas();
      break;
    case 'ArrowDown':
      viewport.y -= 20;
      redrawCanvas();
      break;
    case '+':
    case '=':
      viewport.scale = Math.min(200, viewport.scale * 1.1);
      updateViewportInfo();
      redrawCanvas();
      break;
    case '-':
      viewport.scale = Math.max(10, viewport.scale * 0.9);
      updateViewportInfo();
      redrawCanvas();
      break;
    case 'r':
    case 'R':
      if (selectedSensor !== null) {
        // TODO: rとRで回転方向を変える
        // Rotate selected sensor
        const sensor = sensors.get(selectedSensor);
        if (sensor && sensor.pose) {
          const newTheta = ((sensor.pose.theta_deg || 0) + 15) % 360;
          sensor.pose.theta_deg = newTheta;
          
          const patch = {
            pose: {
              tx: sensor.pose.tx || 0,
              ty: sensor.pose.ty || 0,
              theta_deg: newTheta
            }
          };
          
          try {
            ws.send(JSON.stringify({
              type: 'sensor.update',
              id: selectedSensor,
              patch: patch
            }));
          } catch (e) {
            console.error('Failed to send sensor rotation:', e);
          }
          
          redrawCanvas();
        }
      }
      break;
    case 'Enter':
      if (roiEditMode.startsWith('create_') && roiPoints.length >= 3) {
        // Finish ROI creation
        const regionType = roiEditMode === 'create_include' ? 'include' : 'exclude';
        worldMask[regionType].push([...roiPoints]);
        
        // Send world mask update
        sendWorldMaskUpdate();
        
        roiPoints = [];
        roiEditMode = 'none';
        redrawCanvas();
      }
      break;
    case 'Escape':
      // Cancel ROI creation or clear selection
      roiPoints = [];
      roiEditMode = 'none';
      selectedSensor = null;
      selectedROI = null;
      selectedROIType = null;
      selectedVertex = null;
      redrawCanvas();
      break;
    case 'p':
    case 'P':
      togglePerformanceMode();
      break;
    case 'Delete':
    case 'Backspace':
      if (selectedVertex !== null && selectedROI !== null && selectedROIType) {
        // Delete selected vertex
        const polygon = selectedROIType === 'include' ? worldMask.include[selectedROI] : worldMask.exclude[selectedROI];
        if (polygon.length > 3) { // Keep at least 3 vertices
          deleteVertexFromROI(selectedROIType, selectedROI, selectedVertex);
          selectedVertex = null;
        }
      } else if (selectedROI !== null && selectedROIType) {
        // Delete selected ROI
        deleteSelectedROI();
      }
      break;
    case 'Insert':
      if (selectedVertex !== null && selectedROI !== null && selectedROIType) {
        // Add vertex after selected vertex
        const polygon = selectedROIType === 'include' ? worldMask.include[selectedROI] : worldMask.exclude[selectedROI];
        const currentVertex = polygon[selectedVertex];
        const nextVertex = polygon[(selectedVertex + 1) % polygon.length];
        const midPoint = {
          x: (currentVertex[0] + nextVertex[0]) / 2,
          y: (currentVertex[1] + nextVertex[1]) / 2
        };
        addVertexToROI(selectedROIType, selectedROI, selectedVertex, midPoint);
        selectedVertex = selectedVertex + 1;
        redrawCanvas();
      }
      break;
  }
});

// ROI management functions
function sendWorldMaskUpdate() {
  try {
    ws.send(JSON.stringify({
      type: 'world.update',
      patch: {
        world_mask: {
          includes: worldMask.include,
          excludes: worldMask.exclude
        }
      }
    }));
  } catch (e) {
    console.error('Failed to send world mask update:', e);
  }
}

// Load world mask from snapshot
function loadWorldMaskFromSnapshot(snapshot) {
  if (snapshot && snapshot.world_mask) {
    worldMask.include = snapshot.world_mask.includes || [];
    worldMask.exclude = snapshot.world_mask.excludes || [];
    redrawCanvas();
  }
}

// UI Control Event Handlers
document.addEventListener('DOMContentLoaded', () => {
  // Viewport controls
  const btnResetViewport = document.getElementById('btn-reset-viewport');
  const btnCreateInclude = document.getElementById('btn-create-include');
  const btnCreateExclude = document.getElementById('btn-create-exclude');
  const btnClearROI = document.getElementById('btn-clear-roi');
  const btnSaveConfig = document.getElementById('btn-save-config');
  const viewportInfo = document.getElementById('viewport-info');
  const roiInstructions = document.getElementById('roi-instructions');

  // Reset viewport
  btnResetViewport?.addEventListener('click', () => {
    viewport.x = 0;
    viewport.y = 0;
    viewport.scale = 60;
    redrawCanvas();
    updateViewportInfo();
  });

  // ROI creation buttons
  btnCreateInclude?.addEventListener('click', () => {
    if (roiEditMode === 'create_include') {
      // Cancel current creation
      roiEditMode = 'none';
      roiPoints = [];
      btnCreateInclude.classList.remove('active', 'roi-creating');
    } else {
      // Start include region creation
      roiEditMode = 'create_include';
      roiPoints = [];
      btnCreateInclude.classList.add('active', 'roi-creating');
      btnCreateExclude.classList.remove('active', 'roi-creating');
      cv.className = 'canvas-roi';
    }
    redrawCanvas();
    updateROIInstructions();
  });

  btnCreateExclude?.addEventListener('click', () => {
    if (roiEditMode === 'create_exclude') {
      // Cancel current creation
      roiEditMode = 'none';
      roiPoints = [];
      btnCreateExclude.classList.remove('active', 'roi-creating');
    } else {
      // Start exclude region creation
      roiEditMode = 'create_exclude';
      roiPoints = [];
      btnCreateExclude.classList.add('active', 'roi-creating');
      btnCreateInclude.classList.remove('active', 'roi-creating');
      cv.className = 'canvas-roi';
    }
    redrawCanvas();
    updateROIInstructions();
  });

  // Clear all ROI
  btnClearROI?.addEventListener('click', () => {
    if (confirm('Clear all ROI regions?')) {
      worldMask.include = [];
      worldMask.exclude = [];
      sendWorldMaskUpdate();
      redrawCanvas();
    }
  });

  // Configuration buttons
  const btnSaveDefault = document.getElementById('btn-save-default');
  btnSaveDefault?.addEventListener('click', () => {
    saveDefaultConfiguration();
  });

  btnSaveConfig?.addEventListener('click', () => {
    saveCurrentConfiguration();
  });

  const btnLoadConfig = document.getElementById('btn-load-config');
  btnLoadConfig?.addEventListener('click', () => {
    loadConfiguration();
  });

  const btnExportConfig = document.getElementById('btn-export-config');
  btnExportConfig?.addEventListener('click', () => {
    exportConfiguration();
  });

  const btnImportConfig = document.getElementById('btn-import-config');
  btnImportConfig?.addEventListener('click', () => {
    importConfiguration();
  });

  // Update viewport info periodically
  setInterval(updateViewportInfo, 1000);
  
  // Initialize UI state
  updateViewportInfo();
  updateROIInstructions();
});

function updateViewportInfo() {
  const viewportInfo = document.getElementById('viewport-info');
  if (viewportInfo) {
    const scale = Math.round(viewport.scale);
    const metersPerPixel = (1 / viewport.scale).toFixed(3);
    viewportInfo.textContent = `Scale: ${scale}px/m (${metersPerPixel}m/px)`;
  }
}

function updateROIInstructions() {
  const roiInstructions = document.getElementById('roi-instructions');
  if (!roiInstructions) return;
  
  if (roiEditMode.startsWith('create_')) {
    const regionType = roiEditMode === 'create_include' ? 'Include' : 'Exclude';
    roiInstructions.textContent = `Creating ${regionType} Region: Click points, Enter to finish, Esc to cancel`;
    roiInstructions.style.color = roiEditMode === 'create_include' ? '#28a745' : '#dc3545';
  } else {
    roiInstructions.textContent = 'Pan: Drag | Zoom: Wheel | Sensors: Drag to move, R to rotate | ROI: Click points, Enter to finish';
    roiInstructions.style.color = '#6c757d';
  }
}

function updateCanvasCursor() {
  if (roiEditMode.startsWith('create_')) {
    cv.className = 'canvas-roi';
  } else if (selectedSensor !== null) {
    cv.className = 'canvas-sensor';
  } else {
    cv.className = 'canvas-pan';
  }
}

// Browser state synchronization functions
async function refreshBrowserStateFromServer() {
  try {
    // Request fresh sensor snapshot
    ws.send(JSON.stringify({ type: 'sensor.requestSnapshot' }));
    
    // Wait a bit for the snapshot to arrive and update UI
    await new Promise(resolve => setTimeout(resolve, 500));
    
    // Refresh filter configuration from server
    // Note: This would require a filter.requestSnapshot message type on the server
    // For now, we'll just redraw the canvas to reflect any changes
    redrawCanvas();
    updateViewportInfo();
    
  } catch (e) {
    console.error('Failed to refresh browser state:', e);
  }
}

// Configuration save/load with REST API
async function saveCurrentConfiguration() {
  let name = prompt('Enter configuration name:', 'webui_saved');
  if (!name) return;
  
  // Strip .yaml extension if user added it
  name = name.replace(/\.ya?ml$/i, '');
  
  try {
    // Now save the current server state with the specified name
    const response = await fetch('/api/v1/configs/save', {
      method: 'POST',
      headers: {
        'Content-Type': 'application/json',
        // Add auth header if needed
        // 'Authorization': 'Bearer ' + getAuthToken()
      },
      body: JSON.stringify({
        name: name
      })
    });
    
    if (response.ok) {
      const result = await response.json();
      setPanelMsg(`Configuration saved: ${result.name}`, true);
      showNotification(`Configuration saved successfully as ${result.name}`, 'success');
    } else {
      const error = await response.json();
      setPanelMsg(`Save failed: ${error.message}`, false);
      showNotification(`Save failed: ${error.message}`, 'error');
    }
  } catch (e) {
    setPanelMsg(`Save failed: ${e.message}`, false);
    showNotification(`Save failed: ${e.message}`, 'error');
  }
}

async function saveDefaultConfiguration() {
  try {
    // Save current server state to default.yaml
    const response = await fetch('/api/v1/configs/save', {
      method: 'POST',
      headers: {
        'Content-Type': 'application/json',
        // Add auth header if needed
        // 'Authorization': 'Bearer ' + getAuthToken()
      },
      body: JSON.stringify({
        name: 'default'
      })
    });
    
    if (response.ok) {
      const result = await response.json();
      setPanelMsg(`Configuration saved to default.yaml`, true);
      showNotification(`Configuration saved successfully to default.yaml`, 'success');
    } else {
      const error = await response.json();
      setPanelMsg(`Save failed: ${error.message}`, false);
      showNotification(`Save failed: ${error.message}`, 'error');
    }
  } catch (e) {
    setPanelMsg(`Save failed: ${e.message}`, false);
    showNotification(`Save failed: ${e.message}`, 'error');
  }
}

async function loadConfiguration() {
  try {
    // First, get the list of available configurations
    const listResponse = await fetch('/api/v1/configs/list', {
      method: 'GET',
      headers: {
        // Add auth header if needed
        // 'Authorization': 'Bearer ' + getAuthToken()
      }
    });
    
    if (!listResponse.ok) {
      const error = await listResponse.json();
      showNotification(`Failed to get config list: ${error.message}`, 'error');
      return;
    }
    
    const listResult = await listResponse.json();
    const files = listResult.files || [];
    
    if (files.length === 0) {
      showNotification('No configuration files found', 'warning');
      return;
    }
    
    // Create a simple selection dialog
    let options = files.map((name, index) => `${index + 1}. ${name}`).join('\n');
    const selection = prompt(`Select configuration to load:\n${options}\n\nEnter number or name:`);
    
    if (!selection) return;
    
    let selectedName;
    // Check if user entered a number
    const num = parseInt(selection);
    if (!isNaN(num) && num >= 1 && num <= files.length) {
      selectedName = files[num - 1];
    } else {
      // Check if the entered name exists in the list
      selectedName = files.find(name => name.toLowerCase() === selection.toLowerCase());
      if (!selectedName) {
        showNotification(`Configuration "${selection}" not found`, 'error');
        return;
      }
    }
    
    // Load the selected configuration
    const response = await fetch('/api/v1/configs/load', {
      method: 'POST',
      headers: {
        'Content-Type': 'application/json',
        // Add auth header if needed
        // 'Authorization': 'Bearer ' + getAuthToken()
      },
      body: JSON.stringify({
        name: selectedName
      })
    });
    
    if (response.ok) {
      const result = await response.json();
      setPanelMsg(`Configuration loaded: ${selectedName}`, true);
      showNotification(`Configuration loaded successfully: ${selectedName}`, 'success');
      
      // Refresh all browser state from server
      await refreshBrowserStateFromServer();
    } else {
      const error = await response.json();
      setPanelMsg(`Load failed: ${error.message}`, false);
      showNotification(`Load failed: ${error.message}`, 'error');
    }
  } catch (e) {
    setPanelMsg(`Load failed: ${e.message}`, false);
    showNotification(`Load failed: ${e.message}`, 'error');
  }
}

async function exportConfiguration() {
  try {
    const response = await fetch('/api/v1/configs/export', {
      method: 'GET',
      headers: {
        // Add auth header if needed
        // 'Authorization': 'Bearer ' + getAuthToken()
      }
    });
    
    if (response.ok) {
      const yamlContent = await response.text();
      
      // Create download link
      const blob = new Blob([yamlContent], { type: 'text/yaml' });
      const url = window.URL.createObjectURL(blob);
      const a = document.createElement('a');
      a.href = url;
      a.download = 'hokuyo_config.yaml';
      document.body.appendChild(a);
      a.click();
      document.body.removeChild(a);
      window.URL.revokeObjectURL(url);
      
      showNotification('Configuration exported successfully', 'success');
    } else {
      const error = await response.json();
      showNotification(`Export failed: ${error.message}`, 'error');
    }
  } catch (e) {
    showNotification(`Export failed: ${e.message}`, 'error');
  }
}

async function importConfiguration() {
  const fileInput = document.getElementById('config-file-input');
  fileInput.click();
}

// Handle file selection for import
document.addEventListener('DOMContentLoaded', () => {
  const fileInput = document.getElementById('config-file-input');
  if (fileInput) {
    fileInput.addEventListener('change', async (event) => {
      const file = event.target.files[0];
      if (!file) return;
      
      try {
        const yamlContent = await new Promise((resolve, reject) => {
          const reader = new FileReader();
          reader.onload = (e) => resolve(e.target.result);
          reader.onerror = (e) => reject(e);
          reader.readAsText(file);
        });
        
        const response = await fetch('/api/v1/configs/import', {
          method: 'POST',
          headers: {
            'Content-Type': 'text/plain',
            // Add auth header if needed
            // 'Authorization': 'Bearer ' + getAuthToken()
          },
          body: yamlContent
        });
        
        if (response.ok) {
          const result = await response.json();
          showNotification(`Configuration imported successfully from ${file.name}`, 'success');
          
          // Refresh all browser state from server
          await refreshBrowserStateFromServer();
        } else {
          const error = await response.json();
          showNotification(`Import failed: ${error.message}`, 'error');
        }
      } catch (e) {
        showNotification(`Import failed: ${e.message}`, 'error');
      }
      
      // Clear the file input
      event.target.value = '';
    });
  }
});

function showNotification(message, type = 'info') {
  // Create notification element
  const notification = document.createElement('div');
  notification.className = `notification notification-${type}`;
  notification.textContent = message;
  
  // Add to page
  document.body.appendChild(notification);
  
  // Auto-remove after 3 seconds
  setTimeout(() => {
    if (notification.parentNode) {
      notification.parentNode.removeChild(notification);
    }
  }, 3000);
}

async function loadConfigurationFromServer(path) {
  try {
    const response = await fetch('/api/v1/configs/load', {
      method: 'POST',
      headers: {
        'Content-Type': 'application/json',
        // Add auth header if needed
        // 'Authorization': 'Bearer ' + getAuthToken()
      },
      body: JSON.stringify({
        path: path
      })
    });
    
    if (response.ok) {
      const result = await response.json();
      setPanelMsg(`Configuration loaded: ${path}`, true);
      
      // Refresh sensor data
      ws.send(JSON.stringify({ type: 'sensor.requestSnapshot' }));
    } else {
      const error = await response.json();
      setPanelMsg(`Load failed: ${error.message}`, false);
    }
  } catch (e) {
    setPanelMsg(`Load failed: ${e.message}`, false);
  }
}

// Enhanced WebSocket message handling for world mask
const originalOnMessage = ws.onmessage;
ws.onmessage = (ev) => {
  // Call original handler first
  if (originalOnMessage) {
    originalOnMessage.call(ws, ev);
  }
  
  // Handle additional message types
  try {
    const m = JSON.parse(ev.data);
    
    if (m.type === 'sensor.snapshot' && m.world_mask) {
      loadWorldMaskFromSnapshot(m);
    } else if (m.type === 'world.updated' && m.world_mask) {
      worldMask.include = m.world_mask.includes || [];
      worldMask.exclude = m.world_mask.excludes || [];
      redrawCanvas();
    }
  } catch (e) {
    // Ignore parsing errors
  }
};

// Update canvas cursor based on mode
function updateCanvasState() {
  updateCanvasCursor();
  updateROIInstructions();
  
  // Update button states
  const btnCreateInclude = document.getElementById('btn-create-include');
  const btnCreateExclude = document.getElementById('btn-create-exclude');
  
  if (btnCreateInclude) {
    btnCreateInclude.classList.toggle('active', roiEditMode === 'create_include');
    btnCreateInclude.classList.toggle('roi-creating', roiEditMode === 'create_include');
  }
  
  if (btnCreateExclude) {
    btnCreateExclude.classList.toggle('active', roiEditMode === 'create_exclude');
    btnCreateExclude.classList.toggle('roi-creating', roiEditMode === 'create_exclude');
  }
}

// Override the keyboard handler to update UI state
const originalKeyHandler = document.onkeydown;
document.addEventListener('keydown', (e) => {
  // Call original handler logic here if needed
  
  // Update UI state after key handling
  setTimeout(() => {
    updateCanvasState();
    updateViewportInfo();
  }, 0);
});

// Publishers management
const publishersTbody = document.getElementById('publishers-tbody');
const publishersMsg = document.getElementById('publishers-msg');

function setPublishersMessage(text, isError = false) {
  if (publishersMsg) {
    publishersMsg.textContent = text || '';
    publishersMsg.className = isError ? 'panel-msg error' : 'panel-msg';
    if (text) {
      setTimeout(() => { publishersMsg.textContent = ''; }, 3000);
    }
  }
}

function renderPublishers(sinksArray) {
  if (!publishersTbody) return;
  
  publishersTbody.innerHTML = '';
  
  if (!Array.isArray(sinksArray) || sinksArray.length === 0) {
    const tr = document.createElement('tr');
    tr.innerHTML = '<td colspan="6" style="text-align: center; color: #666;">No publishers configured</td>';
    publishersTbody.appendChild(tr);
    return;
  }
  
  for (const sink of sinksArray) {
    const tr = document.createElement('tr');
    
    const type = sink.type || 'unknown';
    const enabled = sink.enabled ? 'Yes' : 'No';
    const url = sink.url || '-';
    const topic = sink.topic || '-';
    const encoding = sink.encoding || '-';
    const rateLimit = sink.rate_limit || 0;
    
    tr.innerHTML = `
      <td style="padding: 6px 8px; border-bottom: 1px solid #eee;">${type}</td>
      <td style="padding: 6px 8px; border-bottom: 1px solid #eee;">
        <span class="status-badge ${enabled === 'Yes' ? 'status-enabled' : 'status-disabled'}">${enabled}</span>
      </td>
      <td style="padding: 6px 8px; border-bottom: 1px solid #eee; font-family: monospace; font-size: 0.9em;">${url}</td>
      <td style="padding: 6px 8px; border-bottom: 1px solid #eee;">${topic}</td>
      <td style="padding: 6px 8px; border-bottom: 1px solid #eee;">${encoding}</td>
      <td style="padding: 6px 8px; border-bottom: 1px solid #eee; text-align: right;">${rateLimit}</td>
    `;
    
    publishersTbody.appendChild(tr);
  }
  
  setPublishersMessage(`Updated: ${sinksArray.length} publisher(s) configured`);
}

// Initialize canvas state
updateCanvasState();

// ======== DBSCAN Configuration Management ========
const dbscanElements = {
  epsNorm: document.getElementById('dbscan-eps-norm'),
  minPts: document.getElementById('dbscan-min-pts'),
  kScale: document.getElementById('dbscan-k-scale'),
  hMin: document.getElementById('dbscan-h-min'),
  hMax: document.getElementById('dbscan-h-max'),
  rMax: document.getElementById('dbscan-r-max'),
  mMax: document.getElementById('dbscan-m-max'),
  msg: document.getElementById('dbscan-msg')
};

let currentDbscanConfig = {
  eps_norm: 2.5,
  minPts: 5,
  k_scale: 1.0,
  h_min: 0.01,
  h_max: 0.20,
  R_max: 5,
  M_max: 600
};

let dbscanUpdateTimeout = null;

function setDbscanMessage(text, isError = false) {
  if (dbscanElements.msg) {
    dbscanElements.msg.textContent = text || '';
    dbscanElements.msg.className = isError ? 'panel-msg error' : 'panel-msg';
    if (text) {
      setTimeout(() => { dbscanElements.msg.textContent = ''; }, 3000);
    }
  }
}

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

function applyDbscanConfigToUI(config) {
  if (dbscanElements.epsNorm) dbscanElements.epsNorm.value = config.eps_norm;
  if (dbscanElements.minPts) dbscanElements.minPts.value = config.minPts;
  if (dbscanElements.kScale) dbscanElements.kScale.value = config.k_scale;
  if (dbscanElements.hMin) dbscanElements.hMin.value = config.h_min;
  if (dbscanElements.hMax) dbscanElements.hMax.value = config.h_max;
  if (dbscanElements.rMax) dbscanElements.rMax.value = config.R_max;
  if (dbscanElements.mMax) dbscanElements.mMax.value = config.M_max;
}

function sendDbscanConfig() {
  const config = loadDbscanConfigFromUI();
  currentDbscanConfig = config;
  
  try {
    ws.send(JSON.stringify({
      type: 'dbscan.update',
      config: config
    }));
    setDbscanMessage('DBSCAN configuration updated');
  } catch (e) {
    setDbscanMessage('Failed to send DBSCAN configuration', true);
  }
}

function setupDbscanEventListeners() {
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
      input.addEventListener('change', () => {
        // Debounce updates
        if (dbscanUpdateTimeout) {
          clearTimeout(dbscanUpdateTimeout);
        }
        dbscanUpdateTimeout = setTimeout(sendDbscanConfig, 500);
      });
      input.addEventListener('blur', () => {
        // Immediate update on blur
        if (dbscanUpdateTimeout) {
          clearTimeout(dbscanUpdateTimeout);
        }
        sendDbscanConfig();
      });
    }
  });
}

// ======== Sensor Addition Management ========
const btnAddSensor = document.getElementById('btn-add-sensor');
let sensorAddModal = null;

function createSensorAddModal() {
  // Create modal HTML
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
              <option value="hokuyo_urg_eth">Hokuyo URG Ethernet</option>
              <option value="unknown">Unknown</option>
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
              <option value="ME">ME (Distance + Intensity)</option>
              <option value="MD">MD (Distance Only)</option>
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
  
  // Add to document
  document.body.insertAdjacentHTML('beforeend', modalHtml);
  
  // Get references
  const backdrop = document.getElementById('sensor-add-modal-backdrop');
  const modal = document.getElementById('sensor-add-modal');
  const closeBtn = document.getElementById('sensor-add-modal-close');
  const cancelBtn = document.getElementById('sensor-add-cancel');
  const saveBtn = document.getElementById('sensor-add-save');
  
  // Event listeners
  const closeModal = () => {
    backdrop.remove();
    sensorAddModal = null;
  };
  
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
      const response = await fetch('/api/v1/sensors', {
        method: 'POST',
        headers: {
          'Content-Type': 'application/json'
        },
        body: JSON.stringify(sensorData)
      });
      
      if (response.ok) {
        const result = await response.json();
        setPanelMsg(`Sensor added: ${result.id}`, true);
        closeModal();
        // Refresh sensor list
        ws.send(JSON.stringify({ type: 'sensor.requestSnapshot' }));
      } else {
        const error = await response.json();
        setPanelMsg(`Failed to add sensor: ${error.message}`, false);
      }
    } catch (e) {
      setPanelMsg(`Failed to add sensor: ${e.message}`, false);
    }
  });
  
  return { backdrop, modal };
}

// ======== Sink Management ========
const btnAddSink = document.getElementById('btn-add-sink');
const sinksTbody = document.getElementById('sinks-tbody');
const sinksMsg = document.getElementById('sinks-msg');

function setSinksMessage(text, isError = false) {
  if (sinksMsg) {
    sinksMsg.textContent = text || '';
    sinksMsg.className = isError ? 'panel-msg error' : 'panel-msg';
    if (text) {
      setTimeout(() => { sinksMsg.textContent = ''; }, 3000);
    }
  }
}

function renderSinks(sinksArray) {
  if (!sinksTbody) return;
  
  sinksTbody.innerHTML = '';
  
  if (!Array.isArray(sinksArray) || sinksArray.length === 0) {
    const tr = document.createElement('tr');
    tr.innerHTML = '<td colspan="7" style="text-align: center; color: #666;">No sinks configured</td>';
    sinksTbody.appendChild(tr);
    return;
  }
  
  for (let i = 0; i < sinksArray.length; i++) {
    const sink = sinksArray[i];
    const tr = document.createElement('tr');
    
    const type = sink.type || 'unknown';
    const enabled = sink.enabled ? 'Yes' : 'No';
    const url = sink.url || '-';
    const topic = sink.topic || '-';
    const encoding = sink.encoding || '-';
    const rateLimit = sink.rate_limit || 0;
    
    tr.innerHTML = `
      <td>${type}</td>
      <td><span class="status-badge ${enabled === 'Yes' ? 'status-enabled' : 'status-disabled'}">${enabled}</span></td>
      <td style="font-family: monospace; font-size: 0.9em;">${url}</td>
      <td>${topic}</td>
      <td>${encoding}</td>
      <td style="text-align: right;">${rateLimit}</td>
      <td style="text-align: right;">
        <button class="btn" data-edit-sink="${i}">Edit</button>
        <button class="btn" data-delete-sink="${i}" style="margin-left: 4px; background-color: #dc3545; color: white;">Delete</button>
      </td>
    `;
    
    // Add event listeners
    const editBtn = tr.querySelector('[data-edit-sink]');
    const deleteBtn = tr.querySelector('[data-delete-sink]');
    
    editBtn.addEventListener('click', () => editSink(i));
    deleteBtn.addEventListener('click', () => deleteSink(i));
    
    sinksTbody.appendChild(tr);
  }
  
  setSinksMessage(`${sinksArray.length} sink(s) configured`);
}

// Create sink add modal
function createSinkAddModal() {
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
              <option value="nng">NNG</option>
              <option value="osc">OSC</option>
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
              <option value="msgpack">MessagePack</option>
              <option value="json">JSON</option>
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
  const modal = document.getElementById('sink-add-modal');
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
    
    if (type === 'nng') {
      encodingRow.style.display = 'flex';
      bundleRow.style.display = 'none';
      fragmentRow.style.display = 'none';
    } else if (type === 'osc') {
      encodingRow.style.display = 'none';
      bundleRow.style.display = 'flex';
      fragmentRow.style.display = 'flex';
    }
  };
  
  typeSelect.addEventListener('change', updateFieldVisibility);
  updateFieldVisibility();
  
  const closeModal = () => {
    backdrop.remove();
  };
  
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
    
    if (type === 'nng') {
      sinkData.encoding = document.getElementById('sink-encoding-select').value;
    } else if (type === 'osc') {
      sinkData.in_bundle = document.getElementById('sink-in-bundle-input').checked;
      sinkData.bundle_fragment_size = parseInt(document.getElementById('sink-fragment-size-input').value) || 1024;
    }
    
    try {
      const response = await fetch('/api/v1/sinks', {
        method: 'POST',
        headers: {
          'Content-Type': 'application/json'
        },
        body: JSON.stringify(sinkData)
      });
      
      if (response.ok) {
        const result = await response.json();
        setSinksMessage(`Sink added successfully`, false);
        closeModal();
        // Refresh sink list
        ws.send(JSON.stringify({ type: 'sensor.requestSnapshot' }));
      } else {
        const error = await response.json();
        setSinksMessage(`Failed to add sink: ${error.message}`, true);
      }
    } catch (e) {
      setSinksMessage(`Failed to add sink: ${e.message}`, true);
    }
  });
}

// Edit sink function
function editSink(index) {
  // Get current sink data from the rendered table or from a global sinks array
  // For now, we'll create a simple edit modal similar to add modal
  createSinkEditModal(index);
}

// Create sink edit modal
function createSinkEditModal(index) {
  // First, we need to get the current sink data
  // Since we don't have it readily available, we'll fetch it from the server
  fetch('/api/v1/sinks')
    .then(response => response.json())
    .then(sinks => {
      if (index >= sinks.length) {
        setSinksMessage(`Sink index ${index} not found`, true);
        return;
      }
      
      const sink = sinks[index];
      
      const modalHtml = `
        <div id="sink-edit-modal-backdrop" class="modal-backdrop">
          <div id="sink-edit-modal" class="modal">
            <div class="modal__header">
              <h3>Edit Sink ${index}</h3>
              <button id="sink-edit-modal-close" class="btn btn-ghost">×</button>
            </div>
            <div class="modal__body">
              <div class="modal__row">
                <label>Sink Type:</label>
                <select id="sink-edit-type-select" disabled>
                  <option value="nng" ${sink.type === 'nng' ? 'selected' : ''}>NNG</option>
                  <option value="osc" ${sink.type === 'osc' ? 'selected' : ''}>OSC</option>
                </select>
              </div>
              <div class="modal__row">
                <label>URL:</label>
                <input type="text" id="sink-edit-url-input" value="${sink.url || ''}">
              </div>
              <div class="modal__row">
                <label>Topic:</label>
                <input type="text" id="sink-edit-topic-input" value="${sink.topic || ''}">
              </div>
              <div class="modal__row">
                <label>Rate Limit (Hz):</label>
                <input type="number" id="sink-edit-rate-limit-input" min="0" value="${sink.rate_limit || 0}">
              </div>
              <div class="modal__row" id="sink-edit-encoding-row" style="display: ${sink.type === 'nng' ? 'flex' : 'none'};">
                <label>Encoding:</label>
                <select id="sink-edit-encoding-select">
                  <option value="msgpack" ${sink.encoding === 'msgpack' ? 'selected' : ''}>MessagePack</option>
                  <option value="json" ${sink.encoding === 'json' ? 'selected' : ''}>JSON</option>
                </select>
              </div>
              <div class="modal__row" id="sink-edit-bundle-row" style="display: ${sink.type === 'osc' ? 'flex' : 'none'};">
                <label>In Bundle:</label>
                <input type="checkbox" id="sink-edit-in-bundle-input" ${sink.in_bundle ? 'checked' : ''}>
              </div>
              <div class="modal__row" id="sink-edit-fragment-row" style="display: ${sink.type === 'osc' ? 'flex' : 'none'};">
                <label>Bundle Fragment Size:</label>
                <input type="number" id="sink-edit-fragment-size-input" min="0" value="${sink.bundle_fragment_size || 1024}">
              </div>
            </div>
            <div class="modal__footer">
              <button id="sink-edit-cancel" class="btn">Cancel</button>
              <button id="sink-edit-save" class="btn btn-primary">Save Changes</button>
            </div>
          </div>
        </div>
      `;
      
      document.body.insertAdjacentHTML('beforeend', modalHtml);
      
      const backdrop = document.getElementById('sink-edit-modal-backdrop');
      const modal = document.getElementById('sink-edit-modal');
      const closeBtn = document.getElementById('sink-edit-modal-close');
      const cancelBtn = document.getElementById('sink-edit-cancel');
      const saveBtn = document.getElementById('sink-edit-save');
      
      const closeModal = () => {
        backdrop.remove();
      };
      
      closeBtn.addEventListener('click', closeModal);
      cancelBtn.addEventListener('click', closeModal);
      backdrop.addEventListener('click', (e) => {
        if (e.target === backdrop) closeModal();
      });
      
      saveBtn.addEventListener('click', async () => {
        const type = sink.type; // Type is immutable
        const patchData = {
          url: document.getElementById('sink-edit-url-input').value,
          topic: document.getElementById('sink-edit-topic-input').value,
          rate_limit: parseInt(document.getElementById('sink-edit-rate-limit-input').value) || 0
        };
        
        if (type === 'nng') {
          patchData.encoding = document.getElementById('sink-edit-encoding-select').value;
        } else if (type === 'osc') {
          patchData.in_bundle = document.getElementById('sink-edit-in-bundle-input').checked;
          patchData.bundle_fragment_size = parseInt(document.getElementById('sink-edit-fragment-size-input').value) || 1024;
        }
        
        try {
          const response = await fetch(`/api/v1/sinks/${index}`, {
            method: 'PATCH',
            headers: {
              'Content-Type': 'application/json'
            },
            body: JSON.stringify(patchData)
          });
          
          if (response.ok) {
            const result = await response.json();
            setSinksMessage(`Sink ${index} updated successfully`, false);
            closeModal();
            // Refresh sink list
            ws.send(JSON.stringify({ type: 'sensor.requestSnapshot' }));
          } else {
            const error = await response.json();
            setSinksMessage(`Failed to update sink: ${error.message}`, true);
          }
        } catch (e) {
          setSinksMessage(`Failed to update sink: ${e.message}`, true);
        }
      });
    })
    .catch(e => {
      setSinksMessage(`Failed to fetch sink data: ${e.message}`, true);
    });
}

// Delete sink function
function deleteSink(index) {
  if (!confirm(`Are you sure you want to delete sink ${index}?`)) {
    return;
  }
  
  try {
    fetch(`/api/v1/sinks/${index}`, {
      method: 'DELETE',
      headers: {
        'Content-Type': 'application/json'
      }
    }).then(response => {
      if (response.ok) {
        setSinksMessage(`Sink ${index} deleted successfully`, false);
        // Refresh sink list
        ws.send(JSON.stringify({ type: 'sensor.requestSnapshot' }));
      } else {
        response.json().then(error => {
          setSinksMessage(`Failed to delete sink: ${error.message}`, true);
        });
      }
    }).catch(e => {
      setSinksMessage(`Failed to delete sink: ${e.message}`, true);
    });
  } catch (e) {
    setSinksMessage(`Failed to delete sink: ${e.message}`, true);
  }
}

// Initialize DBSCAN and other UI components
document.addEventListener('DOMContentLoaded', () => {
  setupDbscanEventListeners();
  
  // Add sensor button
  if (btnAddSensor) {
    btnAddSensor.addEventListener('click', () => {
      if (!sensorAddModal) {
        sensorAddModal = createSensorAddModal();
      }
    });
  }
  
  // Add sink button
  if (btnAddSink) {
    btnAddSink.addEventListener('click', () => {
      createSinkAddModal();
    });
  }
  
  // Request initial DBSCAN config
  if (ws.readyState === WebSocket.OPEN) {
    ws.send(JSON.stringify({ type: 'dbscan.requestConfig' }));
  } else {
    ws.addEventListener('open', () => {
      ws.send(JSON.stringify({ type: 'dbscan.requestConfig' }));
    });
  }
});

// Handle DBSCAN WebSocket messages
const originalWsOnMessage = ws.onmessage;
ws.onmessage = (ev) => {
  // Call original handler first
  if (originalWsOnMessage) {
    originalWsOnMessage.call(ws, ev);
  }
  
  // Handle DBSCAN messages
  try {
    const m = JSON.parse(ev.data);
    
    if (m.type === 'dbscan.config' && m.config) {
      currentDbscanConfig = m.config;
      applyDbscanConfigToUI(m.config);
      setDbscanMessage('DBSCAN configuration loaded from server');
    } else if (m.type === 'dbscan.updated' && m.config) {
      currentDbscanConfig = m.config;
      applyDbscanConfigToUI(m.config);
      setDbscanMessage('DBSCAN configuration updated');
    }
    
    // Handle sinks in snapshot
    if (m.publishers && m.publishers.sinks) {
      renderSinks(m.publishers.sinks);
    }
  } catch (e) {
    // Ignore parsing errors
  }
};
