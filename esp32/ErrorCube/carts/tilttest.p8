pico-8 cartridge // http://www.pico-8.com
version 41
__lua__
-- tilt test: what the cube's
-- tilt and buttons send
t=0
function _update60() t+=1 end
function arrow(x,y,dx,dy,on)
 local c=on and 11 or 5
 circfill(x,y,9,on and 3 or 1)
 line(x-dx*5-dy*4,y-dy*5-dx*4,x+dx*6,y+dy*6,c)
 line(x-dx*5+dy*4,y-dy*5+dx*4,x+dx*6,y+dy*6,c)
end
function _draw()
 cls(0)
 print("tilt test",46,4,7)
 print("tilt the cube: arrows",22,14,6)
 arrow(64,44,0,-1,btn(2))
 arrow(64,84,0,1,btn(3))
 arrow(44,64,-1,0,btn(0))
 arrow(84,64,1,0,btn(1))
 circfill(64,64,4,8+(t\8)%8)
 local o,x=btn(4),btn(5)
 circfill(40,108,10,o and 8 or 2) print("o",38,106,7)
 circfill(88,108,10,x and 12 or 1) print("x",86,106,7)
 print("- button",22,121,o and 7 or 5)
 print("+ button",72,121,x and 7 or 5)
end
