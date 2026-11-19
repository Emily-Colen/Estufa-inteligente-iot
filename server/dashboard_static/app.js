'use strict';
// A dashboard never embeds Supabase database credentials, service keys or DEVICE_KEY.
const $ = (id) => document.getElementById(id);
const API = { latest: '/api/telemetry/latest?device_id=estufa-01', history: '/api/telemetry/history?device_id=estufa-01&limit=' };
let adminKey = '';  // in memory only; closing/reloading page clears it
let records = [];
let busy = false;
let latestKnownId = null;
let lastSuccess = null;
let latestBusy = false;
let historyBusy = false;
let lastHistoryFetch = 0;
const POLL_MS = 5000;
const HISTORY_MS = 30000;
function fmt(value, suffix = '', decimals = 1) {
  return (typeof value === 'number' && Number.isFinite(value))
    ? value.toLocaleString('pt-BR', {minimumFractionDigits: decimals, maximumFractionDigits: decimals}) + suffix
    : '—';
}
function safeDate(s) { const d = new Date(s); return Number.isFinite(d.getTime()) ? d : null; }
function when(s) { const d = safeDate(s); return d ? d.toLocaleString('pt-BR') : 'Horário desconhecido'; }
function badge(text, state) { $('connectionBadge').textContent = text; $('connectionBadge').className = 'status-pill ' + state; }
function warn(message) { const box = $('globalError'); box.textContent = message; box.hidden = !message; }
function setBoolean(id, value, yes = 'Ligada', no = 'Desligada') {
  const el = $(id); el.textContent = typeof value === 'boolean' ? (value ? yes : no) : '—';
  el.classList.toggle('on', value === true); el.classList.toggle('bad', id === 'reservoirStatus' && value === false);
}
async function getJson(url) {
  // Evita deixar uma requisicao travada indefinidamente.
  const controller = new AbortController();
  const timer = setTimeout(() => controller.abort(), 12000);
  try {
    const result = await fetch(url, {
      headers: {'X-Admin-Key': adminKey}, cache: 'no-store', signal: controller.signal
    });
    if (!result.ok) throw new Error(result.status === 401 || result.status === 422 ? 'ADMIN_KEY incorreta ou não autorizada.' : `Falha na consulta HTTP ${result.status}.`);
    return await result.json();
  } finally {
    clearTimeout(timer);
  }
}
// Estimativa do acionamento da luz a partir de luminosity (0 a 100%).
// Reconstitui a histerese percorrendo o histórico em ordem cronológica.
// Não é a confirmação real do LED, pois grow_light_on não vem da API.
function estimateLighting(readings, latest) {
  const timeline = [...(Array.isArray(readings) ? readings : [])];
  if (latest && !timeline.some(t => t.id != null && t.id === latest.id)) timeline.push(latest);
  timeline.sort((a, b) => {
    const at = safeDate(a.created_at)?.getTime() ?? 0;
    const bt = safeDate(b.created_at)?.getTime() ?? 0;
    return at - bt || (Number(a.id) || 0) - (Number(b.id) || 0);
  });
  let isOn = null;
  for (const reading of timeline) {
    const light = Number(reading.luminosity);
    if (reading.luminosity == null || !Number.isFinite(light)) continue;
    if (light < 30) isOn = true;
    else if (light > 50) isOn = false;
    // De 30% a 50% o ESP32 mantém seu estado anterior.
  }
  return isOn;
}
function renderLatest(t) {
  if (!t) { badge('Sem medições', 'waiting'); $('signalTitle').textContent = 'Nenhuma leitura registrada'; $('signalDescription').textContent = 'Inicie o ESP32 no Wokwi e verifique o envio de telemetria.'; return; }
  latestKnownId = t.id;
  $('temperature').textContent = fmt(t.temperature, ' °C');
  $('airHumidity').textContent = fmt(t.air_humidity, '%');
  $('soilMoisture').textContent = fmt(t.soil_moisture, '%');
  $('luminosity').textContent = fmt(t.luminosity, '%');
  $('measurementTime').textContent = 'Última medição: ' + when(t.created_at);
  $('tempHint').textContent = Number.isFinite(t.temperature) ? (t.temperature >= 29 ? 'Acima de 29 °C' : 'Registrada') : 'Sem leitura';
  $('soilHint').textContent = Number.isFinite(t.soil_moisture) ? (t.soil_moisture <= 40 ? 'Solo seco' : 'Registrada') : 'Sem leitura';
  setBoolean('reservoirStatus', t.reservoir_ok, 'Com água', 'Sem água');
  setBoolean('ventStatus', t.fan_on);
  setBoolean('irrigationStatus', t.pump_on);
  $('fanCaption').textContent = t.fan_on ? 'Ligada' : 'Desligada';
  $('pumpCaption').textContent = t.pump_on ? 'Ligada' : 'Desligada';
  const estimatedLight = estimateLighting(records, t);
  $('lightCaption').textContent = estimatedLight === null
    ? 'Indeterminado (histerese)'
    : (estimatedLight ? 'Ligada (estimada)' : 'Desligada (estimada)');
  $('lightDot').classList.toggle('is-on', estimatedLight === true);
  $('fanDot').classList.toggle('is-on', t.fan_on === true);
  $('pumpDot').classList.toggle('is-on', t.pump_on === true);
  const last = safeDate(t.created_at);
  const age = last ? Date.now() - last.getTime() : Infinity;
  const recent = age >= -60000 && age <= 90000; // allow small clock skew
  badge(recent ? 'Dispositivo online' : 'Sem leituras recentes', recent ? 'online' : 'offline');
  $('greenhouseContainer').classList.toggle('fanning', recent && t.fan_on === true);
  $('greenhouseContainer').classList.toggle('pumping', recent && t.pump_on === true);
  $('greenhouseContainer').classList.toggle('lighting', recent && estimatedLight === true);
  $('signalTitle').textContent = !recent ? 'Dados desatualizados' : !t.reservoir_ok ? 'Verificar reservatório' : t.temperature >= 29 ? 'Temperatura elevada' : 'Monitoramento ativo';
  $('signalDescription').textContent = !recent ? 'O ESP32 não enviou uma leitura recente. Os valores exibidos são o último registro salvo.' : !t.reservoir_ok ? 'A bomba deve permanecer bloqueada quando faltar água.' : 'A automação é executada localmente pelo ESP32.';
}
function populateTable(items) {
 const body = $('readingsBody'); body.replaceChildren();
 if (!items.length) { const tr = document.createElement('tr'); const td = document.createElement('td'); td.colSpan = 4; td.className = 'table-empty'; td.textContent = 'Nenhum registro encontrado'; tr.append(td); body.append(tr); return; }
 for (const t of items.slice(-5).reverse()) {
   const tr = document.createElement('tr');
   for (const value of [when(t.created_at), fmt(t.temperature,'°',1), fmt(t.air_humidity,'%',0), fmt(t.soil_moisture,'%',0)]) {
     const td = document.createElement('td'); td.textContent = value; tr.append(td);
   }
   body.append(tr);
 }
}
function draw() {
 const canvas = $('historyChart'); if (!canvas) return;
 const rect = canvas.getBoundingClientRect();
 const w = Math.max(200, Math.floor(rect.width)), h = Math.max(160, Math.floor(rect.height));
 const dpr = Math.min(2, window.devicePixelRatio || 1);
 canvas.width = Math.round(w*dpr); canvas.height = Math.round(h*dpr);
 const ctx = canvas.getContext('2d'); ctx.setTransform(dpr,0,0,dpr,0,0); ctx.clearRect(0,0,w,h);
 const plot = {left:38,right:w-16,top:14,bottom:h-29};
 ctx.font='11px system-ui';ctx.textBaseline='middle';ctx.lineWidth=1;
 for(let i=0;i<=4;i++) {const y=plot.top+(plot.bottom-plot.top)*i/4;ctx.strokeStyle='#e9efe9';ctx.beginPath();ctx.moveTo(plot.left,y);ctx.lineTo(plot.right,y);ctx.stroke();ctx.fillStyle='#9aab9e';ctx.fillText(String(100-i*25),4,y);}
 if(!records.length){ctx.textAlign='center';ctx.fillStyle='#9dac9d';ctx.fillText('Aguardando medições para construir o gráfico',w/2,h/2);ctx.textAlign='left';return;}
 const x = (i) => records.length===1 ? (plot.left+plot.right)/2 : plot.left + (plot.right-plot.left)*i/(records.length-1);
 const series=[{key:'temperature',color:'#d4866d',scale:60},{key:'air_humidity',color:'#6aabcc',scale:100},{key:'soil_moisture',color:'#69a97d',scale:100}];
 for(const s of series) {
  ctx.strokeStyle=s.color;ctx.fillStyle=s.color;ctx.lineWidth=2.4;ctx.lineJoin='round';ctx.beginPath();let started=false;
  records.forEach((item,i)=>{const value=item[s.key];if(!Number.isFinite(value)){started=false;return;}const yy=plot.bottom-Math.max(0,Math.min(1,value/s.scale))*(plot.bottom-plot.top);if(!started){ctx.moveTo(x(i),yy);started=true;}else ctx.lineTo(x(i),yy);});ctx.stroke();
  if(records.length===1){const v=records[0][s.key];if(Number.isFinite(v)){ctx.beginPath();ctx.arc(x(0),plot.bottom-Math.max(0,Math.min(1,v/s.scale))*(plot.bottom-plot.top),4,0,Math.PI*2);ctx.fill();}}
 }
 ctx.fillStyle='#9aab9e';ctx.font='10px system-ui';
 const first=safeDate(records[0].created_at),last=safeDate(records[records.length-1].created_at);
 if(first){ctx.fillText(first.toLocaleTimeString('pt-BR',{hour:'2-digit',minute:'2-digit'}),plot.left, h-12)}
 if(last){const label=last.toLocaleTimeString('pt-BR',{hour:'2-digit',minute:'2-digit'});ctx.fillText(label,plot.right-40,h-12)}
}
async function refreshLatest() {
 if (!adminKey || latestBusy) return;
 latestBusy = true;
 const t0 = performance.now();
 try {
  const latest = await getJson(API.latest);
  renderLatest(latest);
  warn(''); lastSuccess = Date.now();
  const elapsed = ((performance.now() - t0) / 1000).toFixed(1);
  $('lastRefresh').textContent = 'Consulta: ' + elapsed + ' s · Atualizado às ' + new Date().toLocaleTimeString('pt-BR',{hour:'2-digit',minute:'2-digit',second:'2-digit'});
 } catch(err) {
  const message = err.name === 'AbortError' ? 'A API demorou mais de 12 segundos para responder.' : err.message;
  badge('Conexão indisponível','offline');
  warn(message + ' Verifique sua conexão e os logs do Render.');
  if (!lastSuccess) $('signalDescription').textContent='Não foi possível acessar os dados.';
 } finally { latestBusy=false; }
}
async function refreshHistory(force=false) {
 if (!adminKey || historyBusy || (!force && Date.now()-lastHistoryFetch < HISTORY_MS)) return;
 historyBusy = true;
 try {
  const history = await getJson(API.history+$('historyLimit').value);
  records = Array.isArray(history.data) ? history.data : [];
  lastHistoryFetch = Date.now();
  populateTable(records); draw();
  $('historyCaption').textContent = `${records.length} medições exibidas · Temperatura usa escala 0–60 °C; umidades usam escala 0–100%.`;
  // A estimativa da luz utiliza o historico; recalcular apos chegada dos dados.
  if (records.length) {
   const newest = [...records].sort((a,b)=>(safeDate(a.created_at)?.getTime()??0)-(safeDate(b.created_at)?.getTime()??0)).at(-1);
   if (newest && latestKnownId === newest.id) renderLatest(newest);
  }
 } catch(err) {
  // Falha do historico nao deve bloquear a leitura mais recente.
  console.warn('Histórico indisponível:', err);
 } finally { historyBusy=false; }
}
function refresh() {
 refreshLatest();
 refreshHistory();
}
const overlay = $('authOverlay');
function showAuth() {overlay.hidden=false;$('adminKey').value='';$('adminKey').focus();}
$('openAuth').addEventListener('click', showAuth);
$('cancelAuth').addEventListener('click', ()=>{overlay.hidden=true;});
$('authForm').addEventListener('submit',(e)=>{e.preventDefault();adminKey=$('adminKey').value.trim();overlay.hidden=true;lastSuccess=null;refresh();});
$('historyLimit').addEventListener('change',()=>refreshHistory(true));
window.addEventListener('resize',()=>draw());
window.setInterval(refresh,POLL_MS);
window.addEventListener('focus',()=>refresh());
draw();showAuth();
