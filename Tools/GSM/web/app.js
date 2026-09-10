'use strict';
const el = id => document.getElementById(id);
let snapshot = null, selected = null, busy = false;
const text = (id, value) => { el(id).textContent = value; };
const number = value => value == null ? '—' : value.toLocaleString();
const memory = bytes => bytes == null ? '—' : `${(bytes / 1048576).toFixed(1)} MB`;
const duration = seconds => `${Math.floor(seconds / 3600)}h ${Math.floor(seconds % 3600 / 60)}m ${Math.floor(seconds % 60)}s`;
function inspect() {
  const level = snapshot?.levels.find(row => row.id === selected);
  if (!level) return;
  text('detail-title', level.name); text('detail-subtitle', level.id);
  const facts = {'State':level.state, 'PID (last launch)':number(level.pid), 'Game / query port':`${level.port} / ${level.queryPort || '—'}`, 'Run duration':duration(level.uptimeSeconds), 'Working set':memory(level.workingSetBytes), 'Private memory':memory(level.privateBytes), 'CPU time (total)':level.cpuSeconds == null ? '—' : `${level.cpuSeconds.toFixed(2)} s`, 'Launch count':level.launchCount, 'Exit code':number(level.exitCode)};
  el('facts').replaceChildren();
  for (const [key,value] of Object.entries(facts)) { const dt=document.createElement('dt'),dd=document.createElement('dd');dt.textContent=key;dd.textContent=value;el('facts').append(dt,dd); }
  text('executable',level.executable || 'Resolved when a client requests this level');
  text('args-label',level.arguments.length ? '(last launch)' : '(configured)');
  text('arguments',(level.arguments.length ? level.arguments.slice(1) : level.configuredArguments).join('\n') || '—');
  text('log',level.logPath || 'Available after first launch');
  el('error').hidden=!level.error;text('error',level.error);
}
function render() {
  if (!snapshot) return;
  for (const key of ['running','ready','starting','failed']) text(key,snapshot.counts[key]);
  text('configured',`${snapshot.counts.configured} configured`);
  text('manager',`GSM ${snapshot.version} · PID ${snapshot.managerPid} · TCP ${snapshot.tcpEndpoint} · up ${duration(snapshot.uptimeSeconds)} · ${snapshot.requestCount} requests`);
  if (!snapshot.levels.some(row=>row.id===selected)) selected=snapshot.levels[0]?.id;
  const query=el('search').value.trim().toLowerCase();
  const rows=snapshot.levels.filter(row=>`${row.id} ${row.name} ${row.map} ${row.port} ${row.pid ?? ''}`.toLowerCase().includes(query));
  const focusedLevel=document.activeElement?.dataset.level;
  if(rows.length && !rows.some(row=>row.id===selected))selected=rows[0].id;
  el('rows').replaceChildren(); el('empty').hidden=rows.length>0;
  for (const row of rows) {
    const tr=document.createElement('tr');tr.classList.toggle('selected',row.id===selected);
    const name=document.createElement('td'),button=document.createElement('button'),map=document.createElement('small');
    button.textContent=row.name;button.dataset.level=row.id;button.setAttribute('aria-pressed',String(row.id===selected));button.onclick=()=>{selected=row.id;render();};
    map.textContent=row.map;map.title=row.map;name.append(button,map);tr.append(name);
    const state=document.createElement('td'),badge=document.createElement('span');badge.className=`badge ${row.state}`;badge.textContent=row.state;state.append(badge);tr.append(state);
    for(const value of [row.running ? number(row.pid) : '—',row.port,memory(row.workingSetBytes)]) {const td=document.createElement('td');td.textContent=value;tr.append(td);}
    el('rows').append(tr);
    if(focusedLevel===row.id)button.focus({preventScroll:true});
  }
  inspect();
}
async function refresh() {
  if(busy)return;busy=true;el('refresh').disabled=true;
  const controller=new AbortController(),timeout=setTimeout(()=>controller.abort(),4000);
  try {
    const response=await fetch('/api/status',{cache:'no-store',signal:controller.signal});
    if(!response.ok)throw Error(`HTTP ${response.status}`);
    const next=await response.json();if(!Array.isArray(next.levels)||!next.counts)throw Error('Invalid snapshot');
    snapshot=next;render();text('updated',`Updated ${new Date().toLocaleTimeString()}`);
    text('connection','● Backend connected');el('connection').className='online';el('alert').hidden=true;
  } catch(error) {
    text('connection','○ Backend offline');el('connection').className='';el('alert').hidden=false;
    text('alert',`Cannot reach GSM. ${snapshot?'Values below are stale; the last successful snapshot is retained.':'No process data has been received.'} Retrying automatically.`);
  } finally {clearTimeout(timeout);busy=false;el('refresh').disabled=false;}
}
el('search').addEventListener('input',render);el('refresh').addEventListener('click',refresh);
refresh();setInterval(refresh,2000);
