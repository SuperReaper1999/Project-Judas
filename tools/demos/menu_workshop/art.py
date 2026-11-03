"""Original, deterministic demo art. Pillow is authoring-only, never a game dependency."""
from pathlib import Path
from PIL import Image,ImageDraw
import math,sys
out=Path(sys.argv[1]);out.mkdir(parents=True,exist_ok=True)
colours=['#b49470','#4b6259','#392d29','#87603a','#d3b161','#a6b5b6','#252b2c','#8c4534']
for name in ['sword','axe','shield','wood','apple','ore','potion','pack']:
 im=Image.new('RGBA',(96,96));d=ImageDraw.Draw(im)
 # Deliberately hand-authored silhouettes, rather than letters masquerading as icons.
 if name=='sword':
  d.polygon([(24,73),(67,12),(77,8),(78,22),(36,80)],fill='#bac8c7',outline='#e3eee4',width=2);d.line((36,69,70,18),fill='#728887',width=2);d.line((18,62,47,81),fill='#d1ad62',width=7);d.line((25,77,17,88),fill='#72513a',width=8)
 elif name=='axe':
  d.line((27,83,65,17),fill='#6b4930',width=11);d.line((30,80,63,18),fill='#ac8452',width=4);d.polygon([(47,20),(65,13),(83,25),(77,52),(56,43)],fill='#acb8b6',outline='#e2ded0',width=3)
 elif name=='shield':
  d.polygon([(48,10),(80,25),(75,61),(48,87),(21,61),(16,25)],fill='#8e5438',outline='#d4b96d',width=5);d.line((48,15,48,80),fill='#d4b96d',width=4);d.line((23,40,74,40),fill='#d4b96d',width=4);d.ellipse((39,35,57,53),fill='#d8c588',outline='#665339',width=3)
 elif name=='wood':
  for y in [37,54]:
   d.polygon([(15,y),(67,y-20),(85,y-7),(32,y+17)],fill='#8e5c35',outline='#d0a36b',width=2);d.ellipse((14,y-2,37,y+20),fill='#bd965c',outline='#62412e',width=3);d.arc((20,y+3,32,y+15),0,320,fill='#7e5634',width=2)
 elif name=='apple':
  d.ellipse((19,29,56,80),fill='#a43d31',outline='#e37a45',width=2);d.ellipse((39,29,77,80),fill='#b54232');d.ellipse((28,34,37,47),fill='#e4a16f');d.line((47,33,50,14),fill='#866141',width=5);d.ellipse((51,12,75,26),fill='#72864d')
 elif name=='ore':
  d.polygon([(13,65),(23,32),(55,17),(82,39),(78,71),(43,83)],fill='#778385',outline='#c7c4a1',width=3);d.polygon([(23,32),(51,42),(55,17)],fill='#b5a073');d.polygon([(51,42),(82,39),(78,71)],fill='#465960');d.line((24,64,55,42,66,61),fill='#d6b15c',width=5)
 elif name=='potion':
  d.rounded_rectangle((29,32,68,84),radius=13,fill='#734346',outline='#cab58b',width=3);d.rectangle((39,15,57,38),fill='#c8b38c');d.rectangle((36,11,60,20),fill='#705038');d.ellipse((35,45,61,71),fill='#ad5348');d.line((36,40,36,62),fill='#e2c9ac',width=3)
 else:
  d.rounded_rectangle((23,23,76,84),radius=11,fill='#86653f',outline='#cfb078',width=3);d.arc((34,9,65,36),180,360,fill='#b99158',width=6);d.rectangle((29,49,70,75),fill='#6d5034',outline='#b99c61',width=2);d.line((34,26,34,55),fill='#d6b674',width=5);d.line((64,26,64,55),fill='#d6b674',width=5)
 im.save(out/(name+'.png'))
palette=Image.new('RGB',(32,256));pd=ImageDraw.Draw(palette)
for i,c in enumerate(colours):pd.rectangle((0,i*32,31,(i+1)*32-1),fill=c)
palette.save(out/'palette.png')
verts=[];faces=[]
def box(center,half,colour):
 x,y,z=center;a,b,c=half;start=len(verts)
 verts.extend([(x+sx*a,y+sy*b,z+sz*c) for sx,sy,sz in [(-1,-1,-1),(1,-1,-1),(1,1,-1),(-1,1,-1),(-1,-1,1),(1,-1,1),(1,1,1),(-1,1,1)]])
 for q in [(0,3,2,1),(4,5,6,7),(0,4,7,3),(1,2,6,5),(0,1,5,4),(3,7,6,2)]:
  for tri in [(q[0],q[1],q[2]),(q[0],q[2],q[3])]:faces.append(([start+i+1 for i in tri],colour))
def write(name):
 lines=['# Original menu-workshop low-poly display model']
 lines+=['v %.6f %.6f %.6f'%v for v in verts]
 lines+=['vt .5 %.6f'%(1-(i+.5)/8) for i in range(8)]
 for indices,colour in faces:lines.append('f '+' '.join(str(i)+'/'+str(colour+1) for i in indices))
 (out/(name+'.obj')).write_text('\n'.join(lines)+'\n')
box((0,1.05,0),(.25,.34,.13),1);box((0,.72,0),(.23,.12,.13),2);box((0,.81,.14),(.24,.035,.025),4)
box((0,1.58,0),(.135,.17,.12),0);box((0,1.74,-.01),(.145,.045,.13),2)
for side in [-1,1]:
 box((side*.36,1.18,0),(.11,.14,.14),3);box((side*.4,.94,0),(.075,.18,.09),1);box((side*.41,.72,.015),(.085,.075,.085),3)
 box((side*.12,.42,0),(.095,.24,.1),2);box((side*.12,.12,.05),(.11,.12,.16),3)
box((0,1.14,-.145),(.27,.33,.025),7)
write('traveller');verts.clear();faces.clear()
box((0,.45,0),(.04,.42,.025),5);box((0,.02,0),(.17,.025,.04),4);box((0,-.12,0),(.035,.12,.035),3)
write('blade')
print('Original menu icons and preview mesh written to',out)

verts.clear();faces.clear()
box((0,.2,0),(.035,.4,.035),3);box((.1,.45,0),(.18,.14,.04),5);write('held_axe')
verts.clear();faces.clear()
box((0,0,0),(.25,.32,.035),3);box((0,0,.04),(.035,.33,.02),4);box((0,0,.04),(.26,.035,.02),4);write('held_shield')
verts.clear();faces.clear()
box((0,0,0),(.19,.24,.09),3);box((0,-.1,-.1),(.15,.08,.04),2);write('held_pack')
