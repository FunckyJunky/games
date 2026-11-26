// web_page.h: EAR-CUBE's page, on the cube's own WiFi (http://192.168.4.1) and on the home WiFi (http://<its IP>).
// LIVE (the text as it comes, start / stop), TEXTS (read, download, delete), SETTINGS, WIFI (internet + firmware).
// One file, nothing from the internet (a phone on the cube's WiFi has none). The API is in web.cpp.
#pragma once
static const char EAR_PAGE[] PROGMEM = R"HTML(<!doctype html><html lang=en><head><meta charset=utf-8>
<meta name=viewport content="width=device-width,initial-scale=1"><title>EAR-CUBE</title><style>
:root{--fg:#39ff7a;--dim:#1b7a3c;--bg:#031208;--red:#ff5a6a;--warn:#ffb43a}
*{box-sizing:border-box}
body{background:var(--bg);color:var(--fg);font:16px/1.45 ui-monospace,Consolas,Menlo,monospace;margin:0 auto;padding:0 12px 40px;max-width:760px;text-shadow:0 0 5px rgba(57,255,122,.45)}
header{display:flex;justify-content:space-between;align-items:center;background:var(--fg);color:var(--bg);text-shadow:none;margin:0 -12px;padding:6px 12px;font-weight:bold}
header small{font-weight:normal}
nav{display:flex;gap:6px;margin:12px 0}
nav button{flex:1;background:none;color:var(--dim);border:1px solid var(--dim);font:bold 15px ui-monospace,monospace;padding:9px 0;cursor:pointer}
nav button.on{background:var(--fg);color:var(--bg);border-color:var(--fg)}
h2{font-size:15px;margin:22px 0 8px;border-bottom:2px double var(--fg);padding-bottom:3px}
button,.btn{background:var(--fg);color:var(--bg);border:0;font:bold 15px ui-monospace,monospace;padding:10px 14px;cursor:pointer;text-decoration:none;display:inline-block}
button.b2,.btn.b2{background:none;color:var(--fg);border:1px solid var(--fg)}
button.rec{background:var(--red);color:#fff}
button:disabled{opacity:.35;cursor:default}
.row{display:flex;gap:8px;flex-wrap:wrap;align-items:center;margin:8px 0}
.dim{color:var(--dim)}.red{color:var(--red)}.warn{color:var(--warn)}
.bar{display:flex;align-items:center;gap:10px;border:1px solid var(--dim);padding:8px 10px}
#dot{width:14px;height:14px;border-radius:50%;background:var(--dim);flex:none}
#dot.on{background:var(--fg);box-shadow:0 0 10px var(--fg)}#dot.hear{background:#d8ffe5;box-shadow:0 0 14px #d8ffe5}
#dot.send{background:var(--warn)}#dot.bad{background:var(--red)}
#st{flex:1}
.lv{display:flex;gap:6px;align-items:center}.lv i{display:block;width:60px;height:12px;border:1px solid var(--dim);position:relative}
.lv i b{position:absolute;left:0;top:0;bottom:0;background:var(--fg);width:0;transition:width .15s}
#txt{border:1px solid var(--dim);min-height:45vh;max-height:62vh;overflow:auto;padding:10px;margin-top:10px;white-space:pre-wrap;word-wrap:break-word}
#txt p{margin:0 0 8px}#txt time{color:var(--dim);margin-right:8px}
#txt.big{font-size:24px;line-height:1.35}
#txt .wait{color:var(--dim)}
.f{border-left:4px solid var(--fg);padding:6px 10px;margin:8px 0;background:rgba(57,255,122,.05)}
.f.cur{border-color:var(--red)}.f b{display:block}.f .row{margin:6px 0 0}.f button,.f .btn{padding:6px 10px;font-size:13px}
#view{white-space:pre-wrap;word-wrap:break-word;border:1px solid var(--dim);padding:10px;max-height:70vh;overflow:auto}
label{display:block;margin:12px 0 4px;color:var(--dim)}
input,select{background:#000;color:var(--fg);border:1px solid var(--fg);font:inherit;padding:8px;width:100%}
input[type=checkbox]{width:auto;margin-right:8px}
.chk{display:flex;align-items:center;color:var(--fg);margin:10px 0}
.chip{display:inline-block;border:1px solid var(--dim);margin:3px;padding:3px 8px;cursor:pointer}
#toast{position:fixed;left:50%;bottom:18px;transform:translateX(-50%);background:var(--fg);color:var(--bg);padding:8px 14px;font-weight:bold;display:none;text-shadow:none}
progress{width:100%;height:18px;accent-color:var(--fg)}
a{color:#b8ffd0}
</style></head><body>
<header><span>C:\&gt;EAR-CUBE</span><small id=hst>...</small></header>
<nav><button data-t=live class=on>LIVE</button><button data-t=texts>TEXTS</button><button data-t=set>SETTINGS</button><button data-t=net>WIFI</button></nav>

<section id=live>
 <div class=bar><span id=dot></span><span id=st>CONNECTING...</span><span class=lv>L<i><b id=lvL></b></i>R<i><b id=lvR></b></i></span></div>
 <div class=row><button id=bLive>LISTEN LIVE</button><button id=bRec class=rec>&#9679; RECORD</button><button id=bStop class=b2>STOP</button><span id=run class=dim></span></div>
 <div id=txt><p class=wait>Nothing yet. Press LISTEN LIVE (or RECORD) and talk near the cube.</p></div>
 <div class=row><button class=b2 id=bCopy>COPY ALL</button><button class=b2 id=bSave>SAVE AS A FILE</button><button class=b2 id=bClear>CLEAR</button>
 <span class=chk><input type=checkbox id=big>BIG LETTERS</span></div>
</section>

<section id=texts hidden>
 <div id=list>...</div>
 <div id=viewer hidden><div class=row><button class=b2 id=bBack>&lt; BACK</button><a class="btn b2" id=vDl>DOWNLOAD .TXT</a><span id=vName class=dim></span></div><div id=view></div></div>
</section>

<section id=set hidden>
 <form id=sf>
 <h2>LISTENING</h2>
 <label>LANGUAGE</label><select name=lang id=sLang></select>
 <label>WRITE IT</label><select name=task><option value=0>AS SPOKEN</option><option value=1>IN ENGLISH (TRANSLATED)</option></select>
 <label>SENSITIVITY (WHAT COUNTS AS SPEECH)</label><select name=sens><option value=0>LOW: ONLY LOUD, CLOSE SPEECH</option><option value=1>NORMAL</option><option value=2>HIGH: QUIET SPEECH TOO</option><option value=3>EVERYTHING</option></select>
 <label>MICROPHONE GAIN</label><select name=gain id=sGain></select>
 <label>MICROPHONES</label><select name=mics><option value=0>BOTH</option><option value=1>LEFT</option><option value=2>RIGHT</option></select>
 <label>A PAUSE ENDS A LINE</label><select name=pause><option value=0>SHORT (0.4 S)</option><option value=1>NORMAL (0.7 S)</option><option value=2>LONG (1.2 S)</option></select>
 <h2>SAVING</h2>
 <span class=chk><input type=checkbox name=stamps value=1>THE TIME IN FRONT OF EVERY LINE</span>
 <span class=chk><input type=checkbox name=keepAudio value=1>RECORD: KEEP THE SOUND TOO (WAV, TF CARD)</span>
 <span class=chk><input type=checkbox name=liveKeep value=1>LIVE: SAVE THE TEXT (LIVE-DATE.TXT)</span>
 <h2>THE CUBE'S OWN WIFI</h2>
 <span class=chk><input type=checkbox name=hotspot value=1>ON, ALL THE TIME</span>
 <label>NAME</label><input name=apName maxlength=23 autocomplete=off>
 <label>PASSWORD <span class=dim>(8 OR MORE, EMPTY = OPEN TO ANYONE NEARBY)</span></label><input name=apPass maxlength=23 autocomplete=off>
 <div class=row><button>SAVE</button><span id=sMsg class=dim></span></div>
 </form>
</section>

<section id=net hidden>
 <div id=ns class=bar style="white-space:pre-wrap;display:block">...</div>
 <h2>CONNECT THE CUBE TO THE INTERNET</h2>
 <p class=dim>The words are written by error.blue, so the cube needs the internet for LIVE. RECORD keeps the sound without it and writes it later.</p>
 <form id=wf><label>WIFI NETWORK</label><input name=ssid id=ssid autocomplete=off><div id=nets></div>
 <label>PASSWORD <span class=dim>(empty = keep the saved one)</span></label><input type=password name=pass autocomplete=off>
 <div class=row><button>JOIN</button><button type=button class=b2 id=bScan>SCAN</button><span id=wMsg class=dim></span></div></form>
 <h2>FIRMWARE</h2><p class=dim>A .bin for this cube (EAR-CUBE, or ERROR-CUBE to go back). It restarts when done. Normally it updates itself from error.blue.</p>
 <input type=file id=fw accept=.bin><div class=row><button id=bFw>INSTALL</button></div><progress id=pg max=100 value=0 hidden></progress>
</section>
<div id=toast></div>

<script>
'use strict';
const $=id=>document.getElementById(id),esc=s=>String(s??'').replace(/[&<>"]/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;'}[c]));
const post=(u,d)=>fetch(u,{method:'POST',body:new URLSearchParams(d||{})}).then(r=>r.json()).catch(()=>({}));
const getj=u=>fetch(u,{cache:'no-store'}).then(r=>r.ok?r.json():null).catch(()=>null);
let tab='live',seq=0,S=null,lastOk=0;
function toast(t){const e=$('toast');e.textContent=t;e.style.display='block';clearTimeout(e.t);e.t=setTimeout(()=>e.style.display='none',2600)}
function kb(n){return n<1024?n+' B':n<1048576?Math.round(n/1024)+' KB':(n/1048576).toFixed(1)+' MB'}
function hms(s){const h=s/3600|0,m=s/60%60|0,x=s%60;return(h?h+':'+String(m).padStart(2,'0'):String(m).padStart(2,'0'))+':'+String(x).padStart(2,'0')}

// ---- tabs
document.querySelector('nav').onclick=e=>{const t=e.target.dataset.t;if(!t)return;tab=t;
 document.querySelectorAll('nav button').forEach(b=>b.classList.toggle('on',b.dataset.t==t));
 document.querySelectorAll('section').forEach(s=>s.hidden=s.id!=t);
 if(t=='texts')files();if(t=='set')loadCfg();if(t=='net')netInfo()};

// ---- LIVE
const txt=$('txt');
$('big').checked=localStorage.getItem('earBig')=='1';txt.classList.toggle('big',$('big').checked);
$('big').onchange=()=>{txt.classList.toggle('big',$('big').checked);try{localStorage.setItem('earBig',$('big').checked?'1':'0')}catch(e){}};
function addLines(ls){if(!ls||!ls.length)return;const near=txt.scrollTop+txt.clientHeight>=txt.scrollHeight-40;
 const w=txt.querySelector('.wait');if(w)w.remove();
 for(const l of ls){const p=document.createElement('p');p.innerHTML=(l.at?'<time>'+esc(l.at)+'</time>':'')+esc(l.t);txt.appendChild(p)}
 if(near)txt.scrollTop=txt.scrollHeight}
async function poll(){
 const j=await getj('/api/ear?since='+seq);
 if(!j){if(Date.now()-lastOk>6000){$('st').textContent='CUBE NOT REACHABLE';$('dot').className='bad'}return}
 lastOk=Date.now();S=j;
 if(j.seq<seq){seq=0;txt.innerHTML='';}   // the cube restarted
 addLines(j.lines);seq=j.seq;
 $('hst').textContent=j.host+'  '+j.v;
 const d=$('dot');d.className={1:'on',2:'hear',3:'send',4:'bad',5:'bad',6:'bad',7:'bad'}[j.st]||'';
 let s=j.stText;if(j.waiting>0&&(j.st==3||j.st==4||j.st==5))s+=' ('+j.waiting+' WAITING)';
 if(j.err&&(j.st==4||j.st==5||j.st==6))s+=' - '+j.err;
 $('st').textContent=s;
 $('lvL').style.width=j.lvl[1]+'%';$('lvR').style.width=j.lvl[2]+'%';
 $('bLive').disabled=j.mode!='idle';$('bRec').disabled=j.mode!='idle';$('bStop').disabled=j.mode=='idle';
 $('run').textContent=j.mode=='idle'?'':(j.mode=='rec'?'RECORDING ':'LIVE ')+hms(j.secs)+(j.words?' \u00b7 '+j.words+' WORDS':'');
 if(j.mode=='rec')$('run').className='red';else $('run').className='dim';
}
async function go(m){const r=await post('/api/ear/go',{m});if(r.err)toast(r.err);setTimeout(poll,400)}
$('bLive').onclick=()=>go('live');$('bRec').onclick=()=>go('rec');$('bStop').onclick=()=>go('stop');
$('bClear').onclick=async()=>{await post('/api/ear/clear');txt.innerHTML='<p class=wait>Cleared.</p>'};
$('bSave').onclick=async()=>{const r=await post('/api/ear/save');toast(r.ok?'SAVED: '+r.name:'NOTHING TO SAVE YET')};
$('bCopy').onclick=()=>{const t=[...txt.querySelectorAll('p:not(.wait)')].map(p=>p.textContent).join('\n');
 if(navigator.clipboard&&navigator.clipboard.writeText)navigator.clipboard.writeText(t).then(()=>toast('COPIED'),()=>fallback(t));else fallback(t);
 function fallback(t){const a=document.createElement('textarea');a.value=t;document.body.appendChild(a);a.select();try{document.execCommand('copy');toast('COPIED')}catch(e){}a.remove()}};
setInterval(()=>{if(tab=='live'||Date.now()%3000<800)poll()},800);poll();

// ---- TEXTS
async function files(){const j=await getj('/api/ear/files');if(!j){$('list').textContent="Can't reach the cube.";return}
 if(!j.files.length){$('list').innerHTML='<p class=dim>No transcripts yet. Press RECORD on the LIVE tab (or on the cube).</p>';return}
 $('list').innerHTML='<p class=dim>'+esc(j.where)+'</p>'+j.files.map((f,i)=>'<div class="f'+(f.cur?' cur':'')+'"><b>'+esc(f.name)+(f.cur?' <span class=red>&#9679; RECORDING</span>':'')+'</b>'+
  '<span class=dim>'+esc(f.p.replace(/^\/sd/,'A:').replace(/^\/c/,'C:').replace(/\//g,'\\'))+' \u00b7 '+kb(f.size)+'</span><div class=row>'+
  '<button data-v='+i+'>READ</button><a class="btn b2" href="/dl?p='+encodeURIComponent(f.p)+'">.TXT</a>'+
  (f.wav?'<a class="btn b2" href="/dl?p='+encodeURIComponent(f.p.replace(/\.txt$/,'.wav'))+'">.WAV</a>':'')+
  (f.cur?'':'<button class=b2 data-d='+i+'>DELETE</button>')+'</div></div>').join('');
 $('list').onclick=async e=>{const v=e.target.dataset.v,d=e.target.dataset.d;
  if(v!=null)view(j.files[v]);
  if(d!=null){const f=j.files[d];if(!confirm('Delete '+f.name+'?'))return;await post('/api/rm',{p:f.p});if(f.wav)await post('/api/rm',{p:f.p.replace(/\.txt$/,'.wav')});files()}}}
async function view(f){$('list').hidden=true;$('viewer').hidden=false;$('vName').textContent=f.name;$('vDl').href='/dl?p='+encodeURIComponent(f.p);
 $('view').textContent='...';const t=await fetch('/api/ear/text?p='+encodeURIComponent(f.p)).then(r=>r.text()).catch(()=>"Can't read it.");$('view').textContent=t;$('view').scrollTop=$('view').scrollHeight}
$('bBack').onclick=()=>{$('viewer').hidden=true;$('list').hidden=false};

// ---- SETTINGS
async function loadCfg(){const j=await getj('/api/ear/cfg');if(!j)return;const f=$('sf');
 $('sLang').innerHTML=j.langs.map((n,i)=>'<option value='+i+'>'+esc(n)+'</option>').join('');
 $('sGain').innerHTML=j.gains.map((n,i)=>'<option value='+i+'>'+esc(n)+'</option>').join('');
 for(const k in j.cfg){const e=f.elements[k];if(!e)continue;if(e.type=='checkbox')e.checked=!!j.cfg[k];else e.value=j.cfg[k]}
 if(!f.elements.apName.value)f.elements.apName.placeholder=j.apName}
$('sf').onsubmit=async e=>{e.preventDefault();const f=$('sf'),d={};
 for(const el of f.elements){if(!el.name)continue;d[el.name]=el.type=='checkbox'?(el.checked?1:0):el.value}
 if(d.apPass&&d.apPass.length<8){$('sMsg').textContent='THE PASSWORD NEEDS 8 OR MORE (OR NONE)';return}
 const r=await post('/api/ear/cfg',d);$('sMsg').textContent=r.ok?'SAVED'+(r.hs?'. THE WIFI RESTARTS: JOIN IT AGAIN.':''):'DIDN\'T WORK'};

// ---- WIFI
async function netInfo(){const j=await getj('/info.json');if(!j)return;
 $('ns').textContent='INTERNET: '+(j.state=='ON'?'ON, '+j.ssid+'  '+j.ip:j.state)+'\nBATTERY: '+(j.bat<0?'-':j.bat+'%')+'\nFIRMWARE: '+j.ver;
 if(!$('ssid').value&&j.ssid)$('ssid').value=j.ssid;
 $('nets').innerHTML=j.nets.map(n=>'<span class=chip data-s="'+esc(n.ssid)+'">'+esc(n.ssid)+' '+(n.rssi>-60?'####':n.rssi>-70?'###':n.rssi>-80?'##':'#')+(n.open?'':' *')+'</span>').join('')}
$('nets').onclick=e=>{const s=e.target.dataset.s;if(s!=null)$('ssid').value=s};
$('bScan').onclick=async()=>{$('wMsg').textContent='SCANNING...';await post('/scan');setTimeout(()=>{netInfo();$('wMsg').textContent=''},4500)};
$('wf').onsubmit=async e=>{e.preventDefault();const r=await post('/save',Object.fromEntries(new FormData($('wf'))));$('wMsg').textContent=r.msg||'SAVED';setTimeout(netInfo,6000)};
$('bFw').onclick=()=>{const f=$('fw').files[0];if(!f)return;if(!confirm('Install '+f.name+' on the cube?'))return;const x=new XMLHttpRequest(),d=new FormData();d.append('fw',f);$('pg').hidden=false;
 x.upload.onprogress=e=>$('pg').value=e.loaded*100/e.total;x.onload=()=>toast(x.responseText+' - RESTARTING');x.open('POST','/update');x.send(d)};
</script></body></html>)HTML";
