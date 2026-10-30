#!/usr/bin/env python3
"""Original, deterministic 32px tile atlas. No downloads or model dependencies."""
import pathlib,struct,zlib,random,argparse
p=argparse.ArgumentParser(description=__doc__);p.add_argument('output');dest=pathlib.Path(p.parse_args().output)
if dest.exists():raise SystemExit('Refusing to overwrite existing artwork')
dest.parent.mkdir(parents=True,exist_ok=True);W=H=128;pixels=bytearray(W*H*4);rng=random.Random(41)
colours=[(72,131,83),(100,69,54),(180,143,88),(79,91,112),(77,135,161),(164,77,74),(69,101,79),(191,160,105),(120,99,74),(45,105,86),(216,175,67),(92,152,119),(143,113,87),(202,167,106),(74,92,102),(55,72,94)]
def rect(tile,x,y,w,h,c):
 tx=(tile%4)*32;ty=(tile//4)*32
 for yy in range(max(0,y),min(32,y+h)):
  for xx in range(max(0,x),min(32,x+w)):
   i=((ty+yy)*W+tx+xx)*4;pixels[i:i+4]=bytes((*c,255))
for t,c in enumerate(colours):
 rect(t,0,0,32,32,c)
 for _ in range(22):
  x,y=rng.randrange(32),rng.randrange(32);rect(t,x,y,rng.randrange(1,4),2,tuple(max(0,min(255,v+rng.choice([-12,12]))) for v in c))
 if t==0:
  rect(t,0,0,32,5,(109,176,104));rect(t,0,5,32,2,(52,108,67))
 elif t in (1,3,5,12):
  for y in range(0,32,8):
   rect(t,0,y,32,1,tuple(int(v*.65) for v in c))
   for x in range(-8 if (y//8)%2 else 0,32,16):rect(t,x,y,1,8,tuple(int(v*.65) for v in c))
 elif t in (2,8,13):
  for x in range(0,32,8):rect(t,x,0,1,32,tuple(int(v*.7) for v in c))
  rect(t,0,3,32,2,tuple(int(v*1.12) for v in c));rect(t,0,27,32,2,tuple(int(v*.6) for v in c))
 elif t==7:
  for x,y in [(3,4),(17,3),(9,17),(24,23),(0,27)]:
   rect(t,x,y,7,4,(204,181,135));rect(t,x,y+4,7,1,(157,132,91))
 elif t==11:
  for x,y in [(4,3),(21,9),(12,24),(28,27)]:
   rect(t,x,y,1,3,(123,179,125));rect(t,x+1,y+1,2,1,(76,132,88))
 elif t==4:
  for y in (5,17,26):rect(t,5,y,19,2,(126,191,193))
 elif t in (6,9):
  rect(t,3,3,26,25,tuple(int(v*1.15) for v in c));rect(t,8,0,16,32,c);rect(t,13,12,5,9,(183,156,86))
 elif t==10:
  rect(t,0,0,32,32,(0,0,0));
  for y in range(32):
   for x in range(32):
    i=(((t//4)*32+y)*W+(t%4)*32+x)*4;pixels[i+3]=0
  for x,y,w,h in [(13,3,6,26),(3,13,26,6),(7,7,18,18)]:rect(t,x,y,w,h,(242,206,96))
  rect(t,12,9,5,8,(255,239,169))
 elif t==14:
  rect(t,2,2,28,28,(118,140,151));rect(t,5,5,22,22,c);rect(t,0,15,32,2,(33,51,63))
 elif t==15:
  rect(t,4,4,24,24,(75,95,122));rect(t,8,8,16,16,(94,122,146))
def chunk(tag,data):return struct.pack('!I',len(data))+tag+data+struct.pack('!I',zlib.crc32(tag+data)&0xffffffff)
raw=b''.join(b'\0'+pixels[y*W*4:(y+1)*W*4] for y in range(H))
dest.write_bytes(b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('!2I5B',W,H,8,6,0,0,0))+chunk(b'IDAT',zlib.compress(raw))+chunk(b'IEND',b''))
print(dest)
