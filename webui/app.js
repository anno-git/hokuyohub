// UI elements
const stats = document.getElementById('stats');
const connectionStatus = document.getElementById('connection-status');
const seqInfo = document.getElementById('seq-info');
const lastSeqAgeEl = document.getElementById('last-seq-age');
const fpsInfo = document.getElementById('fps-info');
const cv = document.getElementById('cv');
const ctx = cv.getContext('2d');
const showRawCheckbox = document.getElementById('show-raw-points');
const perSensorColorsCheckbox = document.getElementById('per-sensor-colors');
const legend = document.getElementById('legend');
const legendItems = document.getElementById('legend-items');

// Display state
let showRaw = false;
let perSensorColor = false;

// Connection stats
let connectionCount = 0;
let disconnectionCount = 0;
let errorCount = 0;

// Frame data
let lastSeq = 0;
let lastFrameTime = 0; // t_ns from message
let lastReceiveTime = 0; // local timestamp
let rawPoints = { xy: [], sid: [] };
let clusterItems = [];

// FPS tracking
let frameTimestamps = [];
let lastFPS = 0;

// Color palette for sensors (stable mapping)
const sensorColors = new Map();
const colorPalette = [
  '#e74c3c', '#3498db', '#2ecc71', '#f39c12', '#9b59b6',
  '#1abc9c', '#e67e22', '#34495e', '#f1c40f', '#95a5a6',
  '#c0392b', '#2980b9', '#27ae60', '#d35400', '#8e44ad'
];

function resize(){ cv.width = window.innerWidth; cv.height = Math.floor(window.innerHeight*0.6); }
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
  
  seqInfo.textContent = `seq=${lastSeq} clusters=${clusterItems.length} points=${rawPoints.xy.length/2}`;
  
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
    }
  }catch(e){ /* ignore */ }
};

function redrawCanvas(){
  ctx.clearRect(0,0,cv.width,cv.height);
  
  // Draw raw points if enabled
  if (showRaw && rawPoints.xy.length > 0) {
    drawRawPoints();
  }
  
  // Draw clusters
  if (clusterItems.length > 0) {
    drawClusters();
  }
}

function drawRawPoints(){
  const scale = 60; // 1m = 60px
  const cx = cv.width/2, cy = cv.height*0.8;
  
  // Batch drawing for performance
  ctx.save();
  
  if (perSensorColor && rawPoints.sid.length > 0) {
    // Group points by sensor ID for colored drawing
    const pointsBySensor = new Map();
    for (let i = 0; i < rawPoints.xy.length; i += 2) {
      const sidIndex = Math.floor(i / 2);
      const sid = rawPoints.sid[sidIndex] || 0;
      if (!pointsBySensor.has(sid)) {
        pointsBySensor.set(sid, []);
      }
      pointsBySensor.get(sid).push(rawPoints.xy[i], rawPoints.xy[i + 1]);
    }
    
    // Draw each sensor's points in its color
    for (const [sid, points] of pointsBySensor) {
      ctx.fillStyle = getSensorColor(sid);
      ctx.beginPath();
      for (let i = 0; i < points.length; i += 2) {
        const x = cx + points[i] * scale;
        const y = cy - points[i + 1] * scale;
        ctx.rect(x - 1, y - 1, 2, 2);
      }
      ctx.fill();
    }
  } else {
    // Single color for all points
    ctx.fillStyle = '#4fc3f7';
    ctx.beginPath();
    for (let i = 0; i < rawPoints.xy.length; i += 2) {
      const x = cx + rawPoints.xy[i] * scale;
      const y = cy - rawPoints.xy[i + 1] * scale;
      ctx.rect(x - 1, y - 1, 2, 2);
    }
    ctx.fill();
  }
  
  ctx.restore();
}

function drawClusters(){
  const scale = 60; // 1m = 60px
  const cx = cv.width/2, cy = cv.height*0.8;
  
  ctx.save();
  ctx.strokeStyle = '#33c9';
  ctx.fillStyle = '#6cf9';
  ctx.lineWidth = 2;
  
  for(const c of clusterItems){
    const x = cx + c.cx*scale;
    const y = cy - c.cy*scale;
    ctx.beginPath();
    ctx.arc(x,y,4,0,Math.PI*2);
    ctx.fill();
    
    const minx = cx + c.minx*scale, miny = cy - c.miny*scale;
    const maxx = cx + c.maxx*scale, maxy = cy - c.maxy*scale;
    ctx.strokeRect(minx, maxy, (maxx-minx), (miny-maxy));
  }
  
  ctx.restore();
}
// Legend management
function updateLegend() {
  if (!perSensorColor || rawPoints.sid.length === 0) {
    legend.hidden = true;
    return;
  }
  
  // Get unique sensor IDs from current raw points
  const uniqueSids = [...new Set(rawPoints.sid)].sort((a, b) => a - b);
  
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

// ローカル状態（idをキーに保持）
const sensors = new Map();

function setPanelMsg(text, ok=true){
  panelMsg.textContent = text || '';
  panelMsg.style.color = ok ? '' : 'crimson';
  if (text) setTimeout(()=>{ panelMsg.textContent=''; }, 2000);
}

function renderSensors(){
  tbody.innerHTML = '';
  const rows = Array.from(sensors.values()).sort((a,b)=>a.id-b.id);
  for(const s of rows){
    const tr = document.createElement('tr');
    tr.innerHTML = `
      <td style="padding:6px 8px; border-bottom:1px solid #eee;">${s.id}</td>
      <td style="padding:6px 8px; border-bottom:1px solid #eee;">
        <label class="toggle">
          <input type="checkbox" ${s.enabled ? 'checked' : ''} data-sid="${s.id}" />
          <span>${s.enabled ? 'ON' : 'OFF'}</span>
        </label>
      </td>
      <td style="padding:6px 8px; border-bottom:1px solid #eee;">
        <input class="pose-tx" type="number" step="0.01" value="${Number(s.pose?.tx ?? s.tx ?? 0)}" data-sid="${s.id}" style="width:100px" />
      </td>
      <td style="padding:6px 8px; border-bottom:1px solid #eee;">
        <input class="pose-ty" type="number" step="0.01" value="${Number(s.pose?.ty ?? s.ty ?? 0)}" data-sid="${s.id}" style="width:100px" />
      </td>
      <td style="padding:6px 8px; border-bottom:1px solid #eee;">
        <input class="pose-theta" type="number" step="0.1" value="${Number(s.pose?.theta_deg ?? s.theta_deg ?? 0)}" data-sid="${s.id}" style="width:100px" />
      </td>
      <td style="padding:6px 8px; border-bottom:1px solid #eee; text-align:right;">
        <button class="btn" data-open-modal="${s.id}">Details…</button>
      </td>
    `;

    // Enabled → sensor.enable
    const chk = tr.querySelector('input[type=checkbox]');
    const lbl = tr.querySelector('span');
    chk.addEventListener('change', () => {
      lbl.textContent = chk.checked ? 'ON' : 'OFF';
      try { ws.send(JSON.stringify({ type:'sensor.enable', id: Number(chk.dataset.sid), enabled: chk.checked })); }
      catch(e){ setPanelMsg('send failed', false); }
    });

    // tx/ty/theta → blurで {pose:{...}} patch
    const onPoseBlur = (cls, key) => {
      const input = tr.querySelector(cls);
      input.addEventListener('blur', () => {
        const id = Number(input.dataset.sid);
        const v = Number(input.value || 0);
        const patch = { pose: {} };
        patch.pose[key] = v;
        ws.send(JSON.stringify({ type:'sensor.update', id, patch }));
      });
    };
    onPoseBlur('.pose-tx', 'tx');
    onPoseBlur('.pose-ty', 'ty');
    onPoseBlur('.pose-theta', 'theta_deg');

    // モーダル起動
    tr.querySelector('[data-open-modal]').addEventListener('click', () => openSensorModal(s.id));

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
    } else if (m.type === 'sensor.updated' && m.sensor && typeof m.sensor.id === 'number') {
      sensors.set(m.sensor.id, m.sensor);
      renderSensors();
      maybeCloseModalOnAck(m);
    } else if (m.type === 'ok') {
      // 任意：応答OKのとき軽く表示
      setPanelMsg(`ok: ${m.ref ?? ''}`);
      maybeCloseModalOnAck(m);
    } else if (m.type === 'error') {
      setPanelMsg(m.message || 'error', false);
    }
  }catch(e){ /* ignore */ }
};

// 接続時に snapshot 要求（サーバが自動送信しない環境でも取得できるように）
btnRefresh?.addEventListener('click', ()=>{
  try { ws.send(JSON.stringify({ type:'sensor.requestSnapshot' })); }
  catch(e){ setPanelMsg('send failed', false); }
});
ws.addEventListener('open', ()=>{
  try { ws.send(JSON.stringify({ type:'sensor.requestSnapshot' })); } catch(_e){}
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