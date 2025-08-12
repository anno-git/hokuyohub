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
  // tbodyを都度再描画（件数が少ない前提のシンプル実装）
  tbody.innerHTML = '';
  // id昇順で安定表示
  const rows = Array.from(sensors.values()).sort((a,b)=>a.id-b.id);
  for(const s of rows){
    const tr = document.createElement('tr');
    tr.innerHTML = `
      <td style="padding:6px 8px; border-bottom:1px solid #eee;">${s.id}</td>
      <td style="padding:6px 8px; border-bottom:1px solid #eee;">${s.name ?? ''}</td>
      <td style="padding:6px 8px; border-bottom:1px solid #eee;"><code>${s.endpoint ?? ''}</code></td>
      <td style="padding:6px 8px; border-bottom:1px solid #eee;">${s.mode ?? ''}</td>
      <td style="padding:6px 8px; border-bottom:1px solid #eee;">
        <label style="display:inline-flex; align-items:center; gap:6px; cursor:pointer;">
          <input type="checkbox" ${s.enabled ? 'checked' : ''} data-sid="${s.id}" />
          <span>${s.enabled ? 'ON' : 'OFF'}</span>
        </label>
      </td>
    `;
    // ON/OFFラベルの即時更新
    const chk = tr.querySelector('input[type=checkbox]');
    const lbl = tr.querySelector('span');
    chk.addEventListener('change', () => {
      lbl.textContent = chk.checked ? 'ON' : 'OFF';
      // WSへ操作コマンド送信
      const msg = { type:'sensor.enable', id: s.id, enabled: chk.checked };
      try { ws.send(JSON.stringify(msg)); setPanelMsg(`apply: sensor ${s.id} ${chk.checked?'ON':'OFF'}`); }
      catch(e){ setPanelMsg('send failed', false); }
    });

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
    } else if (m.type === 'sensor.updated' && m.sensor && typeof m.sensor.id === 'number') {
      sensors.set(m.sensor.id, m.sensor);
      renderSensors();
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