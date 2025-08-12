const stats = document.getElementById('stats');
const cv = document.getElementById('cv');
const ctx = cv.getContext('2d');

function resize(){ cv.width = window.innerWidth; cv.height = Math.floor(window.innerHeight*0.6); }
window.addEventListener('resize', resize); resize();

const wsProto = location.protocol === 'https:' ? 'wss' : 'ws';
const ws = new WebSocket(`${wsProto}://${location.host}/ws/live`);
let lastSeq = 0;

ws.onopen = () => stats.textContent = 'connected';
ws.onclose = () => stats.textContent = 'disconnected';
ws.onerror = () => stats.textContent = 'error';

ws.onmessage = (ev) => {
  try{
    const m = JSON.parse(ev.data);
    if(m.type === 'clusters-lite'){
      lastSeq = m.seq;
      drawClusters(m.items);
      stats.textContent = `seq=${m.seq} items=${m.items.length}`;
    }
  }catch(e){ /* ignore */ }
};

function drawClusters(items){
  ctx.clearRect(0,0,cv.width,cv.height);
  // 簡易座標変換（メートル -> 画面）
  const scale = 60; // 1m = 60px（適当）
  const cx = cv.width/2, cy = cv.height*0.8;
  ctx.strokeStyle = '#33c9'; ctx.fillStyle = '#6cf9'; ctx.lineWidth = 2;
  for(const c of items){
    const x = cx + c.cx*scale;
    const y = cy - c.cy*scale;
    ctx.beginPath(); ctx.arc(x,y,4,0,Math.PI*2); ctx.fill();
    const minx = cx + c.minx*scale, miny = cy - c.miny*scale;
    const maxx = cx + c.maxx*scale, maxy = cy - c.maxy*scale;
    ctx.strokeRect(minx, maxy, (maxx-minx), (miny-maxy));
  }
}
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
        <input class="pose-tx" type="number" step="0.01"
               value="${Number(s.pose?.tx ?? s.tx ?? 0)}" data-sid="${s.id}" style="width:100px" />
      </td>
      <td style="padding:6px 8px; border-bottom:1px solid #eee;">
        <input class="pose-ty" type="number" step="0.01"
               value="${Number(s.pose?.ty ?? s.ty ?? 0)}" data-sid="${s.id}" style="width:100px" />
      </td>
      <td style="padding:6px 8px; border-bottom:1px solid #eee;">
        <input class="pose-theta" type="number" step="0.1"
               value="${Number(s.pose?.theta_deg ?? s.theta_deg ?? 0)}" data-sid="${s.id}" style="width:100px" />
      </td>
      <td style="padding:6px 8px; border-bottom:1px solid #eee; text-align:right;">
        <button class="btn" data-open-modal="${s.id}">Details…</button>
      </td>
    `;

    // Enabled -> sensor.enable
    const chk = tr.querySelector('input[type=checkbox]');
    const lbl = tr.querySelector('span');
    chk.addEventListener('change', () => {
      lbl.textContent = chk.checked ? 'ON' : 'OFF';
      const msg = { type:'sensor.enable', id: Number(chk.dataset.sid), enabled: chk.checked };
      try { ws.send(JSON.stringify(msg)); setPanelMsg(`apply: sensor ${s.id} ${chk.checked?'ON':'OFF'}`); }
      catch(e){ setPanelMsg('send failed', false); }
    });

    // tx/ty/theta -> blurで patch {pose:{...}}
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
      tryRefreshModalFromState();
    } else if (m.type === 'sensor.updated' && m.sensor && typeof m.sensor.id === 'number') {
      sensors.set(m.sensor.id, m.sensor);
      renderSensors();
      tryRefreshModalFromState();
    } else if (m.type === 'ok') {
      // 任意：応答OKのとき軽く表示
      setPanelMsg(`ok: ${m.ref ?? ''}`);
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


// modal refs
const mBackdrop = document.getElementById('sensor-modal-backdrop');
const mRoot = document.getElementById('sensor-modal');
const mTitle = document.getElementById('sensor-modal-title');
const mBody  = document.getElementById('sensor-modal-body');
const mClose = document.getElementById('sensor-modal-close');
const mCancel= document.getElementById('sensor-modal-cancel');
const mSave  = document.getElementById('sensor-modal-save');

let modalSensorId = null;
let modalWorking  = null; // 編集中の一時オブジェクト（存在する項目だけ）

function openSensorModal(id){
  const s = sensors.get(id);
  if(!s) return;
  modalSensorId = id;
  modalWorking = buildEditableSnapshot(s); // 存在する項目だけ抽出
  renderSensorModal(modalWorking, s);
  mTitle.textContent = `Sensor #${id} Details`;
  mBackdrop.hidden = false; mRoot.hidden = false;
}

function closeSensorModal(){
  modalSensorId = null; modalWorking = null;
  mBackdrop.hidden = true; mRoot.hidden = true;
}

mClose.addEventListener('click', closeSensorModal);
mCancel.addEventListener('click', closeSensorModal);
mBackdrop.addEventListener('click', (e)=>{ if(e.target===mBackdrop) closeSensorModal(); });

function buildEditableSnapshot(s){
  const out = {};

  // EndPoint -> host/port に分割して保持
  if (s.endpoint != null) {
    if (typeof s.endpoint === 'string') {
      const [host='', portStr=''] = s.endpoint.split(':');
      out.endpoint = { host, port: Number(portStr || 0) || '' };
    } else if (typeof s.endpoint === 'object') {
      out.endpoint = { host: s.endpoint.host ?? '', port: s.endpoint.port ?? '' };
    }
  }

  // Mode（存在時のみ）
  if (s.mode != null) out.mode = String(s.mode);

  if (typeof s.ignore_checksum_error !== 'undefined') {
    out.ignore_checksum_error = Number(s.ignore_checksum_error) ? 1 : 0;
  }
  if (typeof s.skip_step !== 'undefined') {
    out.skip_step = Number(s.skip_step) || 1;
  }

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

  // EndPoint (host/port)
  if (edit.endpoint) {
    const group = document.createElement('div'); group.className='modal__group';
    group.innerHTML = `<h4>EndPoint</h4>
      <div class="modal__row"><label>Host</label><input id="ep-host" type="text" value="${edit.endpoint.host}"></div>
      <div class="modal__row"><label>Port</label><input id="ep-port" type="number" min="0" value="${edit.endpoint.port}"></div>`;
    mBody.appendChild(group);
  }

  // Mode
  if (Object.prototype.hasOwnProperty.call(edit, 'mode')) {
    const row = document.createElement('div'); row.className='modal__row';
    row.innerHTML = `<label>Mode</label><input id="mode" type="text" value="${edit.mode}">`;
    mBody.appendChild(row);
  }

  // ignore_checksum_error
  if (Object.prototype.hasOwnProperty.call(edit, 'ignore_checksum_error')) {
    const row = document.createElement('div'); row.className='modal__row';
    row.innerHTML = `<label>ignore_checksum_error</label>
      <input id="ice" type="checkbox" ${edit.ignore_checksum_error ? 'checked':''}>`;
    mBody.appendChild(row);
  }

  // skip_step
  if (Object.prototype.hasOwnProperty.call(edit, 'skip_step')) {
    const row = document.createElement('div'); row.className='modal__row';
    row.innerHTML = `<label>skip_step (>=1)</label>
      <input id="skip" type="number" min="1" value="${edit.skip_step}">`;
    mBody.appendChild(row);
  }

  // mask（angle/range が存在するときのみ描画）
  if (edit.mask) {
    const group = document.createElement('div'); group.className='modal__group';
    group.innerHTML = `<h4>Mask</h4>`;
    if (edit.mask.angle) {
      group.innerHTML += `
        <div class="modal__row"><label>mask_angle (deg)</label>
          <div>
            <input id="ma-min" type="number" step="0.1" style="width:110px" value="${edit.mask.angle.min_deg}">
            <input id="ma-max" type="number" step="0.1" style="width:110px; margin-left:8px" value="${edit.mask.angle.max_deg}">
          </div>
        </div>`;
    }
    if (edit.mask.range) {
      group.innerHTML += `
        <div class="modal__row"><label>mask_range (m)</label>
          <div>
            <input id="mr-near" type="number" step="0.01" style="width:110px" value="${edit.mask.range.near_m}">
            <input id="mr-far"  type="number" step="0.01" style="width:110px; margin-left:8px" value="${edit.mask.range.far_m}">
          </div>
        </div>`;
    }
    mBody.appendChild(group);
  }

  // Save -> sensor.update で送信
  mSave.onclick = () => {
    if (modalSensorId == null) return;
    const patch = buildPatchFromModal();
    if (!patch) return;
    ws.send(JSON.stringify({ type:'sensor.update', id: modalSensorId, patch }));
    closeSensorModal();
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
    const v = Math.max(1, Number(document.getElementById('skip')?.value ?? 1));
    patch.skip_step = v;
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

// サーバから更新を受けたらモーダルの内容も最新へ
function tryRefreshModalFromState(){
  if (modalSensorId == null) return;
  const s = sensors.get(modalSensorId);
  if (!s) return;
  modalWorking = buildEditableSnapshot(s);
  renderSensorModal(modalWorking, s);
}