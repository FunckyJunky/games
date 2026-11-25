// files_page.h: the cube's FILES page (http://<cube>/files): upload to the SD card or the cube's flash
// over your own WiFi (fast), download, delete, new folder, rename. ERROR-DOS look, works on a phone.
#pragma once
static const char FILES_PAGE[] PROGMEM = R"HTML(<!doctype html><html><head><meta charset=utf-8>
<meta name=viewport content="width=device-width,initial-scale=1"><title>ERROR-CUBE FILES</title><style>
body{margin:0;background:#031208;color:#39ff7a;font:17px/1.35 ui-monospace,Consolas,monospace}
main{max-width:720px;margin:0 auto;padding:0 12px 60px}h1{background:#39ff7a;color:#031208;margin:0 -12px 10px;padding:6px 12px;font-size:19px}
button,select,input{font:inherit;background:#031208;color:#39ff7a;border:2px solid #39ff7a;padding:6px 10px}
button{cursor:pointer}button.on{background:#39ff7a;color:#031208}.row{display:flex;gap:8px;align-items:center;flex-wrap:wrap;margin:8px 0}
#path{word-break:break-all;color:#b8ffd0}#list div{display:flex;gap:8px;align-items:center;border-bottom:1px solid #0c3a1c;padding:7px 2px}
#list .n{flex:1;cursor:pointer;word-break:break-all}#list .s{color:#1b7a3c;white-space:nowrap}#list a,#list .x{color:#39ff7a;cursor:pointer;text-decoration:none;padding:0 4px}
#drop{border:3px dashed #1b7a3c;padding:18px;text-align:center;margin:10px 0}#drop.hot{border-color:#39ff7a;background:#062a12}
#bar{height:14px;border:2px solid #1b7a3c;margin:6px 0}#fill{height:100%;width:0;background:#39ff7a}.dim{color:#1b7a3c}
</style></head><body><main><h1>ERROR-CUBE &middot; FILES</h1>
<div class=row><button id=bsd onclick="go('/sd')">SD CARD</button><button id=bc onclick="go('/c')">CUBE FLASH</button><span id=info class=dim></span></div>
<div class=row><button onclick=up1()>&lt; UP</button><span id=path></span></div>
<div id=drop>Drop files here, or <button onclick="fi.click()">CHOOSE FILES</button><input id=fi type=file multiple hidden>
<div class=dim>MUSIC, VIDEO, PICS, apps... they go into the folder you're in</div></div>
<div id=bar><div id=fill></div></div><div id=st class=dim>&nbsp;</div>
<div class=row><button onclick=mk()>NEW FOLDER</button><button onclick=ls()>REFRESH</button></div>
<div id=list></div></main><script>
let cur='/sd';const $=i=>document.getElementById(i);
function sz(n){return n>=1048576?(n/1048576).toFixed(1)+' MB':n>=1024?Math.round(n/1024)+' KB':n+' B'}
function esc(s){return s.replace(/[&<>"]/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;'})[c])}
async function post(u){const r=await fetch(u,{method:'POST'});return r.json()}
function go(d){cur=d;ls()}
function up1(){if(cur=='/sd'||cur=='/c')return;cur=cur.slice(0,cur.lastIndexOf('/'));ls()}
async function ls(){
  $('bsd').className=cur.startsWith('/sd')?'on':'';$('bc').className=cur.startsWith('/c')?'on':'';
  $('path').textContent=(cur.startsWith('/sd')?'A:':'C:')+(cur.replace(/^\/(sd|c)/,'')||'\\').replace(/\//g,'\\');
  const j=await (await fetch('/api/ls?d='+encodeURIComponent(cur))).json();
  $('info').textContent=cur.startsWith('/sd')?(j.sd?(j.sdInfo+' · '+(j.free??'?')+' MB free'):'NO SD CARD'):((j.free??'?')+' MB free');
  const e=(j.e||[]).sort((a,b)=>b[1]-a[1]||a[0].localeCompare(b[0]));
  $('list').innerHTML=e.length?e.map(f=>{const p=cur+'/'+f[0];
    return '<div><span class=n data-d="'+(f[1]?esc(p):'')+'">'+(f[1]?'&#9618; ':'')+esc(f[0])+'</span><span class=s>'+(f[1]?'&lt;DIR&gt;':sz(f[2]))+'</span>'+
    (f[1]||!/\.(mp3|wav|avi|jpe?g|bmp|txt|md|json|csv|log|p8|bin)$/i.test(f[0])?'':'<span class=x data-o="'+esc(p)+'">&#9654;</span>')+
    (f[1]?'':'<a href="/dl?p='+encodeURIComponent(p)+'">&#8595;</a>')+'<span class=x data-r="'+esc(p)+'">REN</span><span class=x data-x="'+esc(p)+'">DEL</span></div>'}).join(''):
    '<div class=dim>'+(j.ok?'Empty':(cur.startsWith('/sd')?'No SD card in the cube':'Can\'t read this folder'))+'</div>';
}
$('list').onclick=async e=>{const t=e.target;
  if(t.dataset.d){cur=t.dataset.d;ls()}
  else if(t.dataset.o){await post('/api/open?p='+encodeURIComponent(t.dataset.o));$('st').textContent='OPENED ON THE CUBE: '+t.dataset.o.split('/').pop()}
  else if(t.dataset.x){if(confirm('Delete '+t.dataset.x.split('/').pop()+'?')){const j=await post('/api/rm?p='+encodeURIComponent(t.dataset.x));if(!j.ok)alert('Could not delete (folders must be empty)');ls()}}
  else if(t.dataset.r){const o=t.dataset.r,n=prompt('New name',o.split('/').pop());if(n&&n.indexOf('/')<0){await post('/api/mv?a='+encodeURIComponent(o)+'&b='+encodeURIComponent(o.slice(0,o.lastIndexOf('/')+1)+n));ls()}}};
async function mk(){const n=prompt('Folder name');if(n&&n.indexOf('/')<0){await post('/api/mk?p='+encodeURIComponent(cur+'/'+n));ls()}}
function send(f,i,n){return new Promise(res=>{const x=new XMLHttpRequest(),fd=new FormData();fd.append('f',f,f.name);
  x.open('POST','/api/up?d='+encodeURIComponent(cur));
  x.upload.onprogress=e=>{$('fill').style.width=(e.loaded/e.total*100)+'%';$('st').textContent='SENDING '+f.name+' ('+(i+1)+'/'+n+') '+sz(e.loaded)+' / '+sz(e.total)};
  x.onload=()=>{let ok=false;try{ok=JSON.parse(x.responseText).ok}catch(_){}res(ok)};x.onerror=()=>res(false);x.send(fd)})}
async function upload(fs){let ok=0;for(let i=0;i<fs.length;i++)if(await send(fs[i],i,fs.length))ok++;
  $('st').textContent=ok+' of '+fs.length+' file(s) sent'+(ok<fs.length?' · some failed (SD card full or missing?)':'');$('fill').style.width='0';ls()}
$('fi').onchange=()=>upload([...$('fi').files]);
const dr=$('drop');dr.ondragover=e=>{e.preventDefault();dr.classList.add('hot')};dr.ondragleave=()=>dr.classList.remove('hot');
dr.ondrop=e=>{e.preventDefault();dr.classList.remove('hot');upload([...e.dataTransfer.files])};
ls();
</script></body></html>)HTML";
