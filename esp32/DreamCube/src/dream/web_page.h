// web_page.h: DREAM-CUBE's page, on the cube's own WiFi (http://192.168.4.1) and on the home WiFi (http://<its IP>).
// WIFI first (the cube needs the internet to dream), then how it is + buttons, settings, its own hotspot, and the way
// to the full control page at error.blue/dream. One file, nothing from the internet. The API is in web.cpp.
#pragma once
static const char DREAM_PAGE[] PROGMEM = R"HTML(<!doctype html><html lang=en><head><meta charset=utf-8>
<meta name=viewport content="width=device-width,initial-scale=1"><title>DREAM-CUBE</title><style>
:root{--fg:#39ff7a;--dim:#1b7a3c;--bg:#031208;--red:#ff5a6a;--warn:#ffb43a}
*{box-sizing:border-box}
body{background:var(--bg);color:var(--fg);font:16px/1.45 ui-monospace,Consolas,Menlo,monospace;margin:0 auto;padding:0 12px 40px;max-width:760px;text-shadow:0 0 5px rgba(57,255,122,.45)}
header{display:flex;justify-content:space-between;align-items:center;background:var(--fg);color:var(--bg);text-shadow:none;margin:0 -12px;padding:6px 12px;font-weight:bold}
header small{font-weight:normal}
h2{font-size:15px;margin:24px 0 8px;border-bottom:2px double var(--fg);padding-bottom:3px}
button,.btn{background:var(--fg);color:var(--bg);border:0;font:bold 15px ui-monospace,monospace;padding:10px 14px;cursor:pointer;text-decoration:none;display:inline-block}
button.b2,.btn.b2{background:none;color:var(--fg);border:1px solid var(--fg)}
.row{display:flex;gap:8px;flex-wrap:wrap;align-items:center;margin:8px 0}
.dim{color:var(--dim)}.red{color:var(--red)}.warn{color:var(--warn)}
.bar{display:flex;align-items:center;gap:10px;border:1px solid var(--dim);padding:8px 10px}
#dot{width:14px;height:14px;border-radius:50%;background:var(--dim);flex:none}
#dot.on{background:var(--fg);box-shadow:0 0 10px var(--fg)}#dot.hear{background:#d8ffe5;box-shadow:0 0 14px #d8ffe5}#dot.think{background:var(--warn)}#dot.bad{background:var(--red)}
#title{font-size:26px;margin:12px 0 2px;letter-spacing:1px}
#heard{min-height:1.4em}
label{display:block;margin:12px 0 4px;color:var(--dim)}
input,select{background:#000;color:var(--fg);border:1px solid var(--fg);font:inherit;padding:8px;width:100%}
.grid{display:grid;grid-template-columns:1fr 1fr;gap:0 12px}
.chip{display:inline-block;border:1px solid var(--dim);margin:3px;padding:3px 8px;cursor:pointer}
.code{font-size:30px;letter-spacing:4px;color:#d8ffe5}
.box{border:1px solid var(--dim);padding:10px;margin:10px 0}
#toast{position:fixed;left:50%;bottom:18px;transform:translateX(-50%);background:var(--fg);color:var(--bg);padding:8px 14px;font-weight:bold;display:none;text-shadow:none}
a{color:#b8ffd0}
</style></head><body>
<header><span>C:\&gt;DREAM-CUBE</span><small id=hst>...</small></header>

<h2>INTERNET (WIFI)</h2>
<div id=wst class=dim>...</div>
<form id=wf><label>WIFI NETWORK</label><input name=ssid id=ssid autocomplete=off><div id=nets></div>
<label>PASSWORD <span class=dim>(empty = keep the saved one)</span></label><input type=password name=pass autocomplete=off>
<div class=row><button>SAVE AND JOIN</button><button type=button class=b2 id=bScan>SCAN</button><span id=wMsg class=dim></span></div></form>

<h2>THE CUBE</h2>
<div class=bar><div id=dot></div><div id=st>...</div><span class=dim id=lv></span></div>
<div id=title>...</div><div id=heard class=dim></div>
<div class=row>
 <button id=bListen>PAUSE</button><button class=b2 data-m=prev>&lt; OLDER</button><button class=b2 data-m=next>NEWER &gt;</button>
 <button class=b2 data-m=wake>SCREEN ON</button><button class=b2 data-m=sleep>SCREEN OFF</button>
</div>
<form id=sf class=row><input id=say placeholder="type something: the cube dreams it" style="flex:1"><button>DREAM IT</button></form>

<h2>CONTROL PAGE ON ERROR.BLUE</h2>
<div class=box>From any phone or computer with internet: <a id=lnk href="https://error.blue/dream/">error.blue/dream</a><br>
the cube's code: <span class=code id=code>--------</span><br><span class=dim id=lst></span></div>
<div class=dim>There: the scenes it made (and show them again), start / pause, the screen, all settings, and type words to dream.</div>

<h2>SETTINGS</h2>
<form id=cf><div class=grid>
 <div><label>NEW SCENE</label><select name=often><option value=0>OFTEN<option value=1>NORMAL<option value=2>CALM</select></div>
 <div><label>THINKING</label><select name=mind><option value=0>QUICK<option value=1>SMART (SLOWER)</select></div>
 <div><label>WORDS ON SCREEN</label><select name=words><option value=1>ON<option value=0>OFF</select></div>
 <div><label>TITLE ON SCREEN</label><select name=title><option value=1>ON<option value=0>OFF</select></div>
 <div><label>CAMERA</label><select name=cam><option value=0>STILL<option value=1>SLOW<option value=2>NORMAL<option value=3>FAST</select></div>
 <div><label>PIXELS</label><select name=soft><option value=1>SMOOTH<option value=0>SHARP</select></div>
 <div><label>DETAIL</label><select name=detail><option value=1>FULL<option value=0>LESS (FASTER)</select></div>
 <div><label>NEW SCENE WAKES THE SCREEN</label><select name=wake><option value=1>YES<option value=0>NO</select></div>
 <div><label>LANGUAGE</label><select name=lang id=lang></select></div>
 <div><label>WHAT COUNTS AS SPEECH</label><select name=sens><option value=0>LOUD ONLY<option value=1>NORMAL<option value=2>QUIET TOO<option value=3>EVERYTHING</select></div>
 <div><label>MICROPHONE GAIN</label><select name=gain id=gain></select></div>
 <div><label>MICROPHONES</label><select name=mics><option value=0>BOTH<option value=1>LEFT<option value=2>RIGHT</select></div>
</div><div class=row><button>SAVE</button><span id=cMsg class=dim></span></div></form>

<h2>THE CUBE'S OWN WIFI</h2>
<form id=hf><div class=grid>
 <div><label>NAME</label><input name=apName id=apName></div>
 <div><label>PASSWORD (8+, EMPTY = OPEN)</label><input name=apPass id=apPass></div>
 <div><label>ALWAYS ON</label><select name=hotspot id=hs><option value=1>YES<option value=0>ONLY WITHOUT HOME WIFI</select></div>
 <div><label>CONTROL PAGE LINK</label><select name=link id=link><option value=1>ON<option value=0>OFF</select></div>
</div><div class=row><button>SAVE</button><span id=hMsg class=dim></span></div></form>
<p class=dim id=ver></p>
<div id=toast></div>
<script>
const $=i=>document.getElementById(i);
async function getj(u){try{const r=await fetch(u,{cache:'no-store'});return await r.json()}catch(e){return null}}
async function post(u,d){try{const r=await fetch(u,{method:'POST',body:new URLSearchParams(d||{})});return await r.json()}catch(e){return {ok:false}}}
function toast(t){const e=$('toast');e.textContent=t;e.style.display='block';clearTimeout(e.t);e.t=setTimeout(()=>e.style.display='none',2600)}
let filled=false,listen=1;
async function tick(){
 const j=await getj('/api/dream');if(!j){$('st').textContent='NO CONNECTION TO THE CUBE';$('dot').className='bad';return}
 const s=j.s||{};listen=s.listen;
 $('hst').textContent=j.host;$('ver').textContent=j.v;
 $('st').textContent=s.st+(j.err&&/ERROR|LIMIT|INTERNET/.test(s.st)?' \u00b7 '+j.err:'');
 $('dot').className=!j.mic||/ERROR|INTERNET|LIMIT/.test(s.st)?'bad':j.think?'think':j.speech?'hear':s.listen?'on':'';
 $('lv').textContent=j.mic?'\u2588'.repeat(Math.round((j.lvl||0)/12)).padEnd(8,'\u2591'):'NO MIC';
 $('title').textContent=s.title||'...';$('heard').textContent=s.heard?'\u201c'+s.heard+'\u201d':'';
 $('bListen').textContent=s.listen?'PAUSE':'LISTEN';
 $('code').textContent=j.code;$('lnk').href='https://error.blue/dream/#'+j.code;$('lnk').textContent='error.blue/dream/#'+j.code;
 $('lst').textContent=!j.link?'LINK OFF (SEE BELOW)':j.linked?'LINKED: the page sees the cube':j.net?'linking...':'the cube needs the internet first';
 $('wst').innerHTML=j.net?'CONNECTED TO <b>'+esc(j.ssid)+'</b> \u00b7 '+j.ip:'<span class=warn>NOT ON THE INTERNET. Pick the home WiFi below.</span>';
 if(!filled){filled=true;
  $('lang').innerHTML=j.langs.map((n,i)=>'<option value='+i+'>'+n).join('');$('gain').innerHTML=j.gains.map((n,i)=>'<option value='+i+'>'+n).join('');
  const c=s.cfg||{};for(const k in c){const e=document.querySelector('#cf [name='+k+']');if(e)e.value=c[k]}
  $('apName').value=j.apName;$('apPass').value=j.apPass;$('hs').value=j.hotspot;$('link').value=j.link?1:0}
}
function esc(t){return String(t||'').replace(/[&<>]/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;'}[c]))}
$('bListen').onclick=async()=>{await post('/api/dream/go',{m:listen?'pause':'listen'});setTimeout(tick,400)};
document.querySelectorAll('[data-m]').forEach(b=>b.onclick=async()=>{await post('/api/dream/go',{m:b.dataset.m});setTimeout(tick,400)});
$('sf').onsubmit=async e=>{e.preventDefault();const t=$('say').value.trim();if(!t)return;const r=await post('/api/dream/go',{m:'say',t});toast(r.ok?'DREAMING...':r.err||'DIDN\'T WORK');if(r.ok)$('say').value=''};
$('cf').onsubmit=async e=>{e.preventDefault();const r=await post('/api/dream/cfg',Object.fromEntries(new FormData($('cf'))));$('cMsg').textContent=r.ok?'SAVED':'DIDN\'T WORK'};
$('hf').onsubmit=async e=>{e.preventDefault();const f=Object.fromEntries(new FormData($('hf')));if(f.apPass&&f.apPass.length<8){$('hMsg').textContent='PASSWORD: 8 OR MORE';return}
 const r=await post('/api/dream/cfg',f);$('hMsg').textContent=r.ok?'SAVED'+(r.hs?'. THE CUBE\'S WIFI RESTARTS: JOIN IT AGAIN.':''):'DIDN\'T WORK'};
async function netInfo(){const j=await getj('/info.json');if(!j)return;
 $('nets').innerHTML=(j.nets||[]).map(n=>'<span class=chip data-s="'+esc(n.ssid)+'">'+esc(n.ssid)+(n.open?'':' \u25a0')+'</span>').join('');
 document.querySelectorAll('#nets .chip').forEach(c=>c.onclick=()=>{$('ssid').value=c.dataset.s})}
$('bScan').onclick=async()=>{$('wMsg').textContent='SCANNING...';await post('/scan');setTimeout(()=>{netInfo();$('wMsg').textContent=''},4500)};
$('wf').onsubmit=async e=>{e.preventDefault();const r=await post('/save',Object.fromEntries(new FormData($('wf'))));$('wMsg').textContent=r.msg||'SAVED';setTimeout(tick,6000)};
tick();netInfo();setInterval(tick,2000);
</script></body></html>)HTML";
