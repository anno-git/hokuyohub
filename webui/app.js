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