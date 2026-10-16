#!/usr/bin/env python3
"""Deterministic source fixtures for the asset lab; Pillow is authoring-only."""
from pathlib import Path
from PIL import Image
import math,sys
root=Path(sys.argv[1] if len(sys.argv)>1 else '.cache/m93/fixtures');(root/'Assets').mkdir(parents=True,exist_ok=True)
p=root/'Assets/column.obj'
with p.open('w') as f:
 for y in range(65):
  t=y/64;radius=.7+.18*math.cos(t*math.pi*8)+.05*math.sin(t*math.pi*20)
  for x in range(129):
   a=x/128*math.tau;f.write(f'v {radius*math.cos(a):.8f} {t*4:.8f} {radius*math.sin(a):.8f}\n')
 for y in range(65):
  for x in range(129):f.write(f'vt {x/128:.8f} {y/64:.8f}\n')
 for y in range(65):
  t=y/64;dy=(-.18*8*math.pi*math.sin(t*8*math.pi)+.05*20*math.pi*math.cos(t*20*math.pi))/4
  for x in range(129):
   a=x/128*math.tau;n=math.sqrt(1+dy*dy);f.write(f'vn {math.cos(a)/n:.8f} {-dy/n:.8f} {math.sin(a)/n:.8f}\n')
 for y in range(64):
  for x in range(128):
   a=y*129+x+1;b=a+1;c=a+129;d=c+1
   f.write(f'f {a}/{a}/{a} {c}/{c}/{c} {b}/{b}/{b}\nf {b}/{b}/{b} {c}/{c}/{c} {d}/{d}/{d}\n')
# Coherent painted tiles, gradients and fine authored detail, not random noise.
for n in range(4):
 size=2048 if n==0 else 1024;im=Image.new('RGBA',(size,size));pixels=im.load()
 for y in range(size):
  for x in range(size):
   mortar=x%128<4 or y%128<4;v=0 if mortar else 18*math.sin(x*.018)*math.cos(y*.023)+5*math.sin((x+y)*.35)
   pixels[x,y]=(int(max(0,min(255,(42,124,154,98)[n]+v))),int(max(0,min(255,(155,65,120,166)[n]+v))),int(max(0,min(255,(176,145,54,78)[n]+v))),255)
 im.save(root/f'Assets/tile{n}.png')
print(root)
