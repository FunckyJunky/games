pico-8 cartridge // http://www.pico-8.com
version 41
__lua__
-- tolk: een dag op de schietbaan
-- pijltjes: lopen/springen/schuilen
-- o (z) = a, x = b
gy=100
function X(t) return (t-5.5)*200 end
function hh(t)
 local h,m=flr(t),flr((t%1)*60)
 return (h<10 and "0" or "")..h..":"..(m<10 and "0" or "")..m
end
nl={"magazijn eruit!","richt op het doel!","veiligheidspal eraf!","let op de wind!","herladen, nu!","vuur vrij!","ok jongens!"}
ua={"magazyn heti!","tsil'tes' u tsil!","znimit' zapobizhnyk!","vrakhuy viter!","perezaryad' zaraz!","vohon' vilno!","dobre, khlopci!"}
function say(s) toast=s tt=2.6 end
function rst()
 px,py,vy,f=10,gy,0,1
 act,at,sh,mv=0,0,false,false
 cig,en,lup,made,smk,good,tot,cons=100,10,0,0,0,0,0,0
 t=5.5 sp=nil rep=nil pke=0 outt=0 nsp=4 cam=0
 pk={13.3+rnd(.5),14.5+rnd(.7)} pkd={false,false}
 dl={}
 for d in all({{7,300,"wapenkamer"},{8,500,"tank"},{9,700,"de vlag"},{15.5,2000,"wapenkamer"}}) do
  add(dl,{t=d[1],x=d[2],n=d[3],ok=false,w=false})
 end
 parts={} toast="" tt=0
end
function _init()
 fr=0 st=0 ac=0 lt=false why=""
 wx={rnd()<.5,rnd()<.5,rnd()<.5}
 rst()
end
function sil() return t>=9 and t<9.15 end
function rn()
 if t>=12 and t<13 then return 2 end
 if (t>=8.5 and t<12 and wx[2]) or (t>=13 and t<16 and wx[3]) then return 1 end
 return 0
end
function windy() return t>=7 and t<8.5 and wx[1] end
function ix(i)
 local o={15,38,-12}
 return X(max(t,7))+o[i]+20*sin(t*1.43+i*.3)
end
function over(m,l)
 if st==2 then st=3 why=m lt=l ac=0 end
end
function smoke()
 if cig<=0 then say("sigaretten op") return end
 local n=min(4,cig)
 cig-=n smk+=n act=3 at=1.4
 for i=1,6 do add(parts,{x=px+f*5,y=py-12,vx=f*(.1+rnd(.4))+rnd(.2)-.1,vy=-.2-rnd(.4),l=-rnd(.3)}) end
end
function pa()
 if t<6.5 then
  if t>=6 and abs(px-100)<22 then made+=1 lup+=1 act=1 at=.5
  else say("lupa maken: eetzal 6:00-6:30") end
 elseif t<7 then act=4 at=2.2
 elseif lup>0 and (t<12 or t>=13 or sh) then lup-=1 en=min(100,en+10) act=2 at=.8
 else say(lup>0 and "zit eerst (omlaag)" or "geen lupa meer!") end
end
function pb()
 if t<6.5 then
  smoke() act=6 at=1.4 rep={tx="dobroho ranku!",r=1.6,w=0}
 elseif t<7 then act=5 at=1 en=min(100,en+8)
 elseif sil() then over("gerookt in de stilte",true)
 elseif sp then
  good+=1 cons=0 rep={tx=ua[sp.k],r=1.8,w=1} sp=nil nsp=5+rnd(6) act=7 at=.6
 else
  if t>=13 and t<15.5 and pke<=0 then en=max(1,en-8) say("roken buiten perekur!") end
  smoke()
 end
end
function _update()
 fr+=1
 if st==0 then if btnp(4) then rst() st=1 ac=0 end return end
 if st==1 then ac+=1 if ac>66 then st=2 say("wekker 05:30! naar de eetzal") end return end
 if st>=3 then ac+=1 if ac>20 and btnp(4) then rst() st=1 ac=0 end return end
 local dt=1/30
 t+=dt/20
 if at>0 then
  at-=dt
  if act==4 then en=min(100,en+6*dt) end
  if at<=0 then act=0 end
 end
 local s=sil()
 sh=btn(3) and py>=gy and at<=0 and not s
 mv=false
 if at<=0 and not sh and not s then
  if btn(0) then px-=1.4 f=-1 mv=true end
  if btn(1) then px+=1.4 f=1 mv=true end
  if btn(2) and py>=gy then vy=-3.3 end
 end
 px=mid(0,px,2200)
 vy+=.22 py+=vy
 if py>=gy then py=gy vy=0 end
 if btnp(4) and at<=0 and not s then pa() end
 if btnp(5) and (at<=0 or s or sp) then pb() end
 if st!=2 then return end
 cam=max(0,px-40)
 en-=(mv and .25 or .03)*dt
 if t>=12 and t<13 and not sh then en-=3*dt end
 en=min(100,en)
 if en<=0 then over("energie op",false) return end
 for d in all(dl) do
  if not d.ok then
   if t>=d.t-.5 and not d.w then d.w=true say(hh(d.t).." bij "..d.n.."!") end
   if t>=d.t-.5 and abs(px-d.x)<22 then d.ok=true
   elseif t>=d.t then over("te laat bij "..d.n.." "..hh(d.t),false) return end
  end
 end
 if t>=7 then
  local h=false
  for i=1,3 do local q=ix(i)-cam if q<0 or q>127 then h=true end end
  if h then outt+=dt else outt=0 end
  if outt>3 then over("instructeurs uit zicht",true) return end
 end
 if t>=8 and t<15.5 and not(t>=12 and t<13) and not s then
  for i=1,2 do
   if not pkd[i] and t>=pk[i] then
    pkd[i]=true pke=12 sp=nil
    rep={tx="perekur!",r=2.5,w=2} say("perekur! rook nu (x)")
   end
  end
  if pke>0 then pke-=dt end
  if sp then
   sp.r-=dt
   if sp.r<=0 then
    sp=nil cons+=1 nsp=5+rnd(5)
    if cons>=2 then over("2x niet getolkt",true) return end
    say("niet getolkt!")
   end
  elseif pke<=0 then
   nsp-=dt
   if nsp<=0 then sp={k=flr(rnd(#nl))+1,r=2} tot+=1 end
  end
 end
 if rep then rep.r-=dt if rep.r<=0 then rep=nil end end
 if tt>0 then tt-=dt end
 if t>=16 then
  if abs(px-2000)<22 then over("wapenkamer aangeraakt om 16:00",true)
  else st=4 ac=0 end
  return
 end
 if act==3 and rnd()<.5 then add(parts,{x=px+f*5,y=py-12,vx=f*.3+rnd(.2)-.1,vy=-.3-rnd(.3),l=0}) end
 for i=#parts,1,-1 do
  local p=parts[i] p.l+=dt
  if p.l>0 then p.x+=p.vx p.y+=p.vy end
  if p.l>2.6 then deli(parts,i) end
 end
end
function man(x,y,f,bc,lc,hc,pt,sit,wk,air,mus,arm)
 x=flr(x) y=flr(y)
 if sit then
  rectfill(x-3,y-2,x+3,y,lc) rectfill(x-2,y-9,x+1,y-3,bc)
 else
  local s=wk and flr(fr/4)%2 or 0
  if air then rectfill(x-2,y-4,x-1,y-1,lc) rectfill(x,y-4,x+1,y-1,lc)
  else rectfill(x-2+s,y-3,x-1+s,y,lc) rectfill(x-s,y-3,x+1-s,y,lc) end
  rectfill(x-2,y-8,x+1,y-3,bc)
 end
 local hy=y-12-(sit and 1 or 0)
 rectfill(x-2,hy,x+1,hy+3,15) rectfill(x-2,hy-1,x+1,hy,hc)
 if mus then rectfill(x+(f>0 and 0 or -2),hy+2,x+(f>0 and 1 or -1),hy+2,0) end
 local q=f>0 and x-2 or x+1
 if pt==1 then pset(q,y-8,8) pset(q,y-7,7) pset(q,y-6,12)
 elseif pt==2 then pset(q,y-8,12) pset(q,y-7,10) end
 if arm==3 then rectfill(x+f*2,hy+3,x+f*4,hy+3,7) pset(x+f*5,hy+3,8)
 elseif arm==1 or arm==2 then rectfill(x+f*2,hy+2,x+f*4,hy+4,9) pset(x+f*3,hy+3,11)
 elseif arm==5 then rectfill(x+f*2,hy+2,x+f*3,hy+4,7) pset(x+f*2,hy+2,4)
 elseif arm==6 then rectfill(x+f*2,hy-3,x+f*2,hy+3,15)
 elseif arm==7 then pset(x+f*2,hy+4,15) pset(x+f*3,hy+3,15) end
end
function bub(x,y,s,c)
 local w=#s*4+3 x=mid(w/2+1,x,127-w/2)
 rectfill(x-w/2,y-9,x+w/2,y,c) print(s,x-w/2+2,y-7,0)
end
function lab(s,x,y,c) print(s,x-#s*2,y,c or 7) end
function wire(a,b)
 for x=a,b,3 do
  pset(x,gy-6+(flr(x/3)%2)*2,6)
  if flr(x/3)%6==0 then line(x,gy-9,x,gy,4) end
 end
end
function wk(s)
 rectfill(s-22,gy-24,s+22,gy,6) rectfill(s-24,gy-27,s+24,gy-24,5)
 rectfill(s-4,gy-12,s+4,gy,0) rectfill(s-17,gy-18,s-11,gy-14,0) rectfill(s+11,gy-18,s+17,gy-14,0)
 wire(s-70,s-26) wire(s+26,s+70) lab("wapenkamer",s,gy-35)
end
function bld(x,fn)
 local s=x-cam
 if s>-80 and s<208 then fn(s) end
end
function sky()
 local r=rn()
 local a,b=12,6
 if t<7 then a,b=0,1
 elseif t<7.7 then a,b=1,2
 elseif t<8.5 then
  if wx[1] then a,b=5,6 else a,b=2,9 end
 elseif r>0 then a,b=5,6
 elseif t>=15.3 then a,b=13,9 end
 return a,b
end
function _draw()
 cls(0)
 if st==0 then
  lab("t o l k",64,30,10) lab("een dag uit het leven van",64,46) lab("een militair tolk",64,54)
  lab("100 sigaretten moeten op",64,70,9) lab("voor 16:00",64,78,9)
  lab("druk o/z: wekker op 05:30",64,102,7) return
 end
 if st==1 then
  local s=sin(ac*.4)*2
  rectfill(52+s,50,76+s,74,8) rectfill(55+s,53,73+s,71,7) line(64+s,56,64+s,63,0) line(64+s,63,69+s,63,0)
  rectfill(52+s,46,58+s,49,5) rectfill(70+s,46,76+s,49,5) lab("05:30  ring ring!",64,92,7) return
 end
 local a,b=sky()
 rectfill(0,0,127,50,a) rectfill(0,51,127,gy,b)
 if t<7.3 then for i=1,20 do pset((i*37)%128,(i*53)%60,7) end end
 local s=(t-6.5)/10
 if s>0 and s<1 and rn()==0 then circfill(10+s*108,95+sin(s/2)*80,5,(t>8 and t<9) and 9 or 10) end
 local cc=7
 if rn()>0 or windy() or t<7 then cc=5 end
 if t>=8 and t<9.2 then cc=9 end
 for k=0,5 do
  local cx=(k*55-cam*.25-t*(windy() and 120 or 25))%190-30
  local cy=10+(k*13)%30
  rectfill(cx,cy,cx+22,cy+4,cc) rectfill(cx+4,cy-3,cx+16,cy,cc)
 end
 for i=0,127,4 do
  local h=10+5*sin((i+cam*.2)/80)
  rectfill(i,gy-h,i+3,gy,t<7 and 1 or 3)
 end
 if t>=13 and t<15.6 then
  for k=0,2 do
   local dx=(k*55+fr*1.2)%150-10
   local dy=20+k*10+sin(fr/60+k)*2
   rectfill(dx,dy,dx+3,dy,0) pset(dx+1,dy+1,((fr+k*8)%16<8) and 8 or 0)
  end
 end
 rectfill(0,gy,127,127,4)
 for i=0,127,16 do rectfill(i,gy,i+15,gy+1,(cam+i<1300) and 3 or 11) end
 for h=6,16 do
  local q=X(h)-cam
  if q>-10 and q<138 then line(q,gy+2,q,gy+5,7) print(hh(h),q-9,gy+7,6) end
 end
 bld(0,function(q)
  rectfill(q-22,gy-26,q+22,gy,8) rectfill(q-24,gy-30,q+24,gy-26,2)
  for i=0,2 do rectfill(q-17+i*14,gy-20,q-12+i*14,gy-15,t<7.5 and 10 or 12) end
  lab("kazarme",q,gy-38)
 end)
 bld(100,function(q)
  rectfill(q-22,gy-20,q+22,gy,9) rectfill(q-24,gy-25,q+24,gy-20,4)
  rectfill(q-16,gy-15,q-9,gy-10,10) rectfill(q+6,gy-12,q+12,gy,0)
  lab("eetzal",q,gy-33)
  man(q-32,gy,1,4,1,4,0,true) man(q-42,gy,1,13,5,9,0,true) man(q+34,gy,-1,8,5,0,0,true)
 end)
 bld(300,wk)
 bld(2000,function(q)
  wk(q)
  if t>=15.4 then for i=0,2 do man(q-34-i*9,gy,1,3,5,0,1,true) end end
 end)
 bld(500,function(q)
  rectfill(q-16,gy-4,q+16,gy,0) rectfill(q-15,gy-8,q+15,gy-4,3) rectfill(q-6,gy-13,q+7,gy-8,3)
  rectfill(q+7,gy-12,q+22,gy-11,3) pset(q-9,gy-6,7) lab("tank",q,gy-20)
 end)
 bld(700,function(q)
  rectfill(q-15,gy-4,q+15,gy,4) line(q,gy-34,q,gy-4,7)
  for i=0,12 do
   local o=sil() and 0 or flr(sin(fr/50+i*.07)*1.5)
   line(q+1+i,gy-34+o,q+1+i,gy-30+o,12) line(q+1+i,gy-29+o,q+1+i,gy-25+o,10)
  end
  lab("ua",q-9,gy-12,7)
 end)
 bld(1300,function(q)
  for k=0,3 do rectfill(q-30+k*20,gy-8,q-28+k*20,gy,4) circfill(q-29+k*20,gy-16,9,3) circfill(q-29+k*20,gy-19,6,11) end
  lab("12:00 open veld",q,gy-34)
 end)
 bld(1500,function(q)
  rectfill(q-30,gy,q+30,gy+3,0)
  for k=0,6 do rectfill(q-33+k*9,gy-4,q-27+k*9,gy,15) end
  lab("loopgraf",q,gy-12)
 end)
 bld(2100,function(q) line(q,gy-20,q,gy,7) rectfill(q+1,gy-20,q+9,gy-15,8) lab("16:00",q,gy-28,10) end)
 local ms=X(t)-cam
 line(ms,14,ms,gy,10) rectfill(ms-10,8,ms+10,14,0) print(hh(t),ms-9,9,10)
 if t>=6.9 then
  for i=1,3 do
   local q=ix(i)-cam
   man(q,gy,1,3,5,0,i<3 and 1 or 2,false,not sil())
   if rep and rep.w==(i==3 and 1 or (i==2 and 2 or -1)) then bub(q,gy-16,rep.tx,10) end
  end
  if sp then bub(ix(1)-cam,gy-16,nl[sp.k],7) end
 end
 local sit=(act==4) or sh
 man(px-cam,py,f,11,5,4,0,sit,mv and py>=gy,py<gy,true,act)
 if sh then rectfill(px-cam-10,gy-22,px-cam+10,gy-20,3) line(px-cam-9,gy-20,px-cam-9,gy-10,4) line(px-cam+9,gy-20,px-cam+9,gy-10,4) end
 if act==4 then for i=0,2 do circfill(px-cam+4+i*3,gy-22-i*5-(fr/3)%3,1+i*.5,7) end end
 if rep and rep.w==0 then bub(px-cam,gy-18,rep.tx,7) end
 for p in all(parts) do
  if p.l>0 then circfill(p.x-cam,p.y,1+p.l*2.5,p.l<1.4 and 7 or (p.l<2.1 and 6 or 13)) end
 end
 local r=rn()
 if r>0 or windy() then
  local n=windy() and 25 or (r==2 and 70 or 35)
  for i=1,n do
   local rx=(i*23-fr*.5)%128
   local ry=(i*41+fr*(r==2 and 5 or 3))%gy
   if windy() then line(rx,ry,rx+4,ry,6) else line(rx,ry,rx,ry+2,r==2 and 12 or 13) end
  end
 end
 if sil() then rectfill(24,44,104,62,0) lab("minuut stilte",64,47,7) lab("niet roken!",64,55,8) end
 if tt>0 then rectfill(0,0,127,6,0) lab(toast,64,1,7) end
 if sp then rectfill(40,8,40+sp.r*24,10,10) end
 if pke>0 then lab("perekur "..ceil(pke),64,16,11) end
 if outt>0 then lab("! instructeurs uit beeld !",64,22,8) end
 rectfill(0,113,127,127,0)
 print(hh(min(t,16)),2,115,10)
 print("sig "..cig,30,115,9) rect(58,115,92,119,5) rectfill(59,116,59+cig*.32,118,9)
 print("lupa "..lup,98,115,7)
 print("energie",2,122,7) rect(32,122,66,126,5) rectfill(33,123,33+en*.32,125,en<25 and 8 or 11)
 print("tolk "..good.."/"..tot,74,122,6)
 if st==3 then
  rectfill(0,0,127,127,0)
  lab("game over",64,20,8) lab(why,64,34,7)
  if lt then
   man(80,100,-1,3,5,8,1) man(60,100,1,11,5,4,0,false,false,false,true)
   bub(80,80,"oprotten !!!",7)
  end
  lab("druk o/z voor opnieuw",64,118,6)
 elseif st==4 then
  rectfill(0,0,127,127,0)
  lab("16:00 - de dag is voorbij",64,10,10)
  for i=0,2 do man(40+i*22,64-abs(sin(fr/30+i*.2))*8,1,3,5,0,i<2 and 1 or 2,false,true) end
  lab("sigaretten: "..smk.." / 100",64,76)
  lab("goed getolkt: "..good.." / "..tot,64,84)
  lab("lupa over: "..lup.." (gemaakt "..made..")",64,92)
  if cig<=0 then lab("alle sigaretten op!",64,102,11) lab("missie geslaagd",64,110,11)
  else lab(cig.." over - missie mislukt",64,102,8) end
  lab("druk o/z voor opnieuw",64,120,6)
 end
end
