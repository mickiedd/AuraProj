// Local-only fixture for the actual shipped HUD HTML (no copied render logic).
// node Scripts/firearm_hud_fixture.mjs [--check] [port]
// Open http://127.0.0.1:8766 and click Run regression tests.
import { createServer } from 'node:http';
import { readFileSync } from 'node:fs';
import { fileURLToPath } from 'node:url';
import { Script } from 'node:vm';

const webRoot = new URL('../Plugins/AuraWebUI/Content/WebUI/', import.meta.url);
const pages = ['hud-right-top.html', 'hud-bottom.html'];
// UE 5.5 CEFBrowserHandler::OnBeforeResourceLoad strips CR/LF from LoadString content.
// Exercise the actual transport transform, not just modern-browser file loading.
const ueLoadString = html => html.replace(/[\r\n]/g, '');
for (const page of pages) {
  const html = readFileSync(new URL(page, webRoot), 'utf8');
  for (const [, js] of html.matchAll(/<script[^>]*>([\s\S]*?)<\/script>/g)) new Script(js, { filename: page });
  for (const [, js] of ueLoadString(html).matchAll(/<script[^>]*>([\s\S]*?)<\/script>/g)) new Script(js, { filename: page + ':UE-LoadString' });
}

const shim = `<script>
// Substitute transport only; the production event handlers and DOM remain unchanged.
window.WebSocket=class {
  static OPEN=1;
  constructor(){this.readyState=1;window.fixtureSocket=this;setTimeout(()=>this.onopen?.(),0)}
  send(data){parent.postMessage({fixtureCommand:JSON.parse(data),panel:location.pathname},location.origin)}
  close(){this.readyState=3;this.onclose?.()}
};
addEventListener('message',e=>{
  if(e.origin!==location.origin||e.source!==parent)return;
  if(e.data.fixtureEvent)window.fixtureSocket?.onmessage?.({data:JSON.stringify({type:'event',event:e.data.fixtureEvent,payload:e.data.payload})});
  if(e.data.disconnect)window.fixtureSocket?.close();
});
</script>`;
const fixture = String.raw`<!doctype html><meta charset="utf-8"><title>FireGun HUD regression fixture</title>
<style>
body{background:#202d40;color:#eff5ff;font:15px system-ui;margin:24px}h1{font-size:23px}button{padding:8px 14px;margin:4px;background:#344e6c;border:1px solid #84adce;border-radius:6px;color:white;cursor:pointer}iframe{border:1px dashed #789;background:linear-gradient(130deg,#4c5664,#253447);display:block;margin:12px 0}pre{white-space:pre-wrap;background:#131d2c;padding:16px;max-width:1216px}p{color:#bacbdb}
</style><h1>BungeeMan · FireGun ammunition / manual reload</h1>
<p>Actual shipped HUD HTML after Unreal LoadString CR/LF removal, mock WebSocket transport. Panels: 366 × 210 and 1250 × 116. No gameplay server connected.</p>
<div id="controls"></div><button id="run">Run regression tests</button>
<iframe id="right" title="Right-top HUD" src="/hud-right-top.html" width="366" height="210"></iframe>
<iframe id="bottom" title="Bottom skill HUD" src="/hud-bottom.html" width="1250" height="116"></iframe>
<pre id="results">Waiting for panels…</pre><pre id="commands">Reload commands: 0</pre>
<script>
const frames=[document.querySelector('#right'),document.querySelector('#bottom')],commands=[];
const base={applicable:true,roleId:'BungeeMan',abilityTag:'Abilities.Gun.Fire',inputTag:'InputTag.LMB',state:'Ready',magazineRounds:12,magazineCapacity:12,reserveRounds:48,reserveCapacity:48,reloadDuration:1.25,canReload:false,canFire:true};
const cases={
  Ready:{},Partial:{magazineRounds:7,canReload:true},
  Empty:{state:'EmptyMagazine',magazineRounds:0,canReload:true,canFire:false},
  Reloading:{state:'Reloading',magazineRounds:0,canFire:false},
  Complete:{magazineRounds:12,reserveRounds:36},
  'No reserve':{state:'OutOfAmmo',magazineRounds:0,reserveRounds:0,canFire:false},
  Unavailable:{state:'Unavailable',magazineRounds:0,canFire:false},
  Aura:{applicable:false,roleId:'Aura',abilityTag:'',inputTag:'',state:'NotApplicable',canFire:false}
};
let current='Ready',loaded=0;
const tick=()=>new Promise(r=>setTimeout(r,50));
const doc=i=>frames[i].contentDocument;
function event(name,payload){frames.forEach(f=>f.contentWindow.postMessage({fixtureEvent:name,payload},location.origin))}
async function show(name){current=name;event('skill_panel_ability',{inputTag:'InputTag.LMB',abilityTag:name==='Aura'?'Abilities.Fire.FireBolt':'Abilities.Gun.Fire',abilityType:'Abilities.Type.Offensive',statusTag:'Abilities.Status.Equipped'});event('hud_firearm',{...base,...cases[name]});await tick()}
Object.keys(cases).forEach(name=>{const b=document.createElement('button');b.textContent=name;b.onclick=()=>show(name);document.querySelector('#controls').append(b)});
addEventListener('message',e=>{
  if(e.origin!==location.origin||!frames.some(f=>f.contentWindow===e.source))return;
  if(e.data.fixtureCommand?.command==='hud_reload'){commands.push(e.data);document.querySelector('#commands').textContent='Reload commands: '+commands.length+' (requests only; awaiting server event)'}
  if(e.data.fixtureCommand?.command==='hud_ready'&&++loaded===2){show('Ready');document.querySelector('#results').textContent='Panels ready. Choose a state or run tests.'}
});
const failures=[],passes=[];
function check(name,ok){(ok?passes:failures).push(name)}
function key(i,overrides={}){doc(i).defaultView.dispatchEvent(new KeyboardEvent('keydown',{code:'KeyR',key:'r',bubbles:true,...overrides}))}
function fits(){const r=doc(0).querySelector('#firearmPanel').getBoundingClientRect();return r.bottom<=210&&r.right<=366&&r.left>=0}
document.querySelector('#run').onclick=async()=>{
  const run=document.querySelector('#run');run.disabled=true;passes.length=failures.length=commands.length=0;
  try{
    await show('Ready');check('Ready shows 12 / 12 and reserve 48',doc(0).querySelector('#firearmAmmo').textContent==='12 / 12'&&doc(0).querySelector('#firearmReserve').textContent==='Reserve 48');
    check('Full magazine cannot reload',doc(0).querySelector('#reloadFirearm').disabled);check('Ready card fits native host',fits());
    check('Only LMB has a badge',doc(1).querySelectorAll('.firearm-badge').length===1&&doc(1).querySelector('[data-input-tag="InputTag.LMB"] .firearm-badge').textContent==='12/12');
    const tile=doc(1).querySelector('[data-input-tag="InputTag.LMB"]');
    for(let rounds=11;rounds>=0;--rounds){event('hud_firearm',{...base,magazineRounds:rounds,state:rounds?'Ready':'EmptyMagazine',canReload:true,canFire:rounds>0});await tick()}
    check('Twelve shots preserve pointer-owning skill node',tile===doc(1).querySelector('[data-input-tag="InputTag.LMB"]'));
    check('Twelfth shot exposes manual R hint',doc(0).querySelector('#firearmMeta').textContent.includes('Press R')&&doc(1).querySelector('.firearm-badge').textContent==='R');
    check('Empty card fits native host',fits());
    doc(0).querySelector('#reloadFirearm').click();await tick();check('Reload button sends one request',commands.length===1);
    check('Click never optimistically refills or completes reload',doc(0).querySelector('#firearmAmmo').textContent==='0 / 12'&&doc(0).querySelector('#firearmState').textContent==='Magazine empty');
    key(0);key(1);await tick();check('R works while either panel has focus',commands.length===3);
    key(0,{repeat:true});key(1,{ctrlKey:true});await tick();check('Repeat and modified R do not request reload',commands.length===3);
    doc(0).querySelector('#attributes').click();key(0);await tick();check('R does not reload through open menu',commands.length===3);doc(0).querySelector('[data-close="attributeModal"]').click();
    await show('Reloading');check('Reloading is explicit on both panels',doc(0).querySelector('#firearmState').textContent==='Reloading…'&&doc(1).querySelector('.slot-name').textContent==='Reloading');
    check('Reloading disables repeat requests',doc(0).querySelector('#reloadFirearm').disabled);check('Reload duration shown',doc(0).querySelector('#reloadDuration').textContent==='1.25 s reload');
    key(0);key(1);await tick();check('Reloading R is suppressed',commands.length===3);
    await new Promise(r=>setTimeout(r,1400));check('No client timer fabricates completion',doc(0).querySelector('#firearmState').textContent==='Reloading…');
    await show('Complete');check('Server completion updates both counts and badge',doc(0).querySelector('#firearmReserve').textContent==='Reserve 36'&&doc(1).querySelector('.firearm-badge').textContent==='12/12');
    await show('No reserve');check('Out of ammo differs from reloadable empty',doc(0).querySelector('#firearmState').textContent==='Out of ammo'&&doc(0).querySelector('#reloadFirearm').disabled&&doc(1).querySelector('.slot-name').textContent==='No ammo');check('Out of ammo card fits',fits());
    await show('Unavailable');check('Recovering cannot reload',doc(0).querySelector('#reloadFirearm').disabled&&doc(0).querySelector('#firearmState').textContent==='Unavailable');
    await show('Aura');check('Aura has no ammo card or skill badge',doc(0).querySelector('#firearmPanel').hidden&&doc(1).querySelectorAll('.firearm-badge').length===0);key(0);key(1);await tick();check('Aura R never requests firearm reload',commands.length===3);
    // Malformed/mismatched scope must not attach gun UI to FireBolt or another slot.
    event('hud_firearm',base);await tick();check('FireBolt never receives a gun badge',doc(1).querySelectorAll('.firearm-badge').length===0);
    for(const bad of [{roleId:'Aura'},{abilityTag:'Abilities.Fire.FireBolt'},{inputTag:'InputTag.RMB'},{applicable:false}]){event('hud_firearm',{...base,...bad});await tick();check('Reject mismatched scope '+JSON.stringify(bad),doc(0).querySelector('#firearmPanel').hidden&&doc(1).querySelectorAll('.firearm-badge').length===0)}
    await show('Partial');frames.forEach(f=>f.contentWindow.postMessage({disconnect:true},location.origin));await tick();check('Disconnected panels clear stale ammunition',doc(0).querySelector('#firearmPanel').hidden&&doc(1).querySelectorAll('.firearm-badge').length===0);
    // Reconnect is deliberately allowed to settle, then resync via a real event.
    await new Promise(r=>setTimeout(r,1000));await show('Empty');check('Reconnect accepts fresh ammo event',!doc(0).querySelector('#firearmPanel').hidden&&!doc(0).querySelector('#reloadFirearm').disabled);
    frames[0].width=196;frames[0].height=112;await tick();
    for(const name of ['Ready','Empty','Reloading','No reserve']){
      await show(name);const panel=doc(0).querySelector('#firearmPanel').getBoundingClientRect(),button=doc(0).querySelector('#reloadFirearm').getBoundingClientRect(),ammo=doc(0).querySelector('#firearmReserve').getBoundingClientRect();
      check('DPI compact '+name+' card above menu row',panel.bottom<=52&&panel.left>=0&&panel.right<=196);
      check('DPI compact '+name+' reload button visible and separate from ammo',button.bottom<=112&&button.right<=196&&button.left>=ammo.right);
    }
    frames[0].width=366;frames[0].height=210;await show('Empty');
  }catch(e){failures.push(e.stack)}
  document.querySelector('#results').textContent=passes.length+' passed / '+failures.length+' failed\n'+passes.map(s=>'PASS '+s).join('\n')+'\n'+failures.map(s=>'FAIL '+s).join('\n');run.disabled=false;
};
</script>`;
for (const [, js] of (shim + fixture).matchAll(/<script[^>]*>([\s\S]*?)<\/script>/g)) new Script(js, { filename: 'fixture' });
if (process.argv.includes('--check')) {
  console.log('PASS: both shipped HUD scripts parse before and after UE LoadString CR/LF removal; fixture scripts parse');
  process.exit(0);
}
const port = Number(process.argv[2] || 8766);
createServer((req, res) => {
  const page = req.url?.slice(1);
  res.setHeader('Content-Type', 'text/html; charset=utf-8');
  res.setHeader('Cache-Control', 'no-store');
  if (pages.includes(page)) res.end(ueLoadString(readFileSync(new URL(page, webRoot), 'utf8')).replace('<head>', '<head>' + shim));
  else if (req.url === '/') res.end(fixture);
  else { res.statusCode = 404; res.end('Not found'); }
}).listen(port, '127.0.0.1', () => console.log('HUD fixture: http://127.0.0.1:' + port + ' (' + fileURLToPath(webRoot) + ')'));
