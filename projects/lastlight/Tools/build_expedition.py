#!/usr/bin/env python3
"""Append ordinary, editable expedition content. Does not regenerate town art/UI.
Run after build_town.py, then bake the town navigation again.
"""
from pathlib import Path
import json,hashlib,math,re
P=Path(__file__).resolve().parents[1]; A=P/'Assets'; ids=json.loads((A/'ids.json').read_text())
for name in ['ship','expedition','pilot_seated']:
 path=(A/'models'/f'{name}.glb') if name=='pilot_seated' else (A/'scripts'/f'{name}.js'); key=hashlib.md5(('lastlight/'+str(path.relative_to(P))).encode()).hexdigest()
 path.with_name(path.name+'.judasmeta').write_text(f'JudasAssetMeta 1\nid "{key}"\ntype {"mesh" if name=="pilot_seated" else "script"}\nsource ""\n');ids[name]=key
(A/'ids.json').write_text(json.dumps(ids,indent=2)+'\n')
def v(a):return ' '.join(map(str,a))
def transform(pos=(0,0,0),q=(1,0,0,0)):return f'  position {v(pos)}\n  rotation {v(q)}\n  scale 1 1 1\n'
def render(half,color,shape='box',asset=''):return f'  render {shape}\n  render.half-extents {v(half)}\n  render.radius .5\n  render.color {v(color)}\n  render.alpha 1\n  render.secondary-color .8 .8 .8\n  render.secondary-alpha 1\n  render.texture-asset ""\n  render.mesh-asset "{asset}"\n'
def body(half,motion='static',mass=10):return f'  body {motion} box\n  body.half-extents {v(half)}\n  body.radius .5\n  body.terrain ""\n  body.mass {mass}\n  body.friction .8\n  body.restitution 0\n  body.initial-velocity 0 0 0\n  body.pickable false\n  body.managed false\n  body.compound-count 0\n  body.fluid-cavity-count 0\n'
def obj(id,name,text):return f'object {id} {json.dumps(name)}\n{text}end\n'
objects=[];counter=3020
def piece(name,pos,half,color,parent=0,collision=False,q=(1,0,0,0),mesh=''):
 global counter
 i=counter;counter+=1;text=transform(pos,q)+render(half,color,'mesh' if mesh else 'box',ids.get(mesh,''))
 if collision:text+=body(half)
 if parent:text+=f'  parent {parent}\n'
 objects.append(obj(i,name,text));return i
# One hollow compound body, not a solid bounding box around the cabin.
# Coordinates are deck-local; all child visuals remain ordinary editable content.
hull=[((0,0,0),(2.4,.15,4)),((-2.3,1.5,0),(.1,1.35,4)),((2.3,1.5,0),(.1,1.35,4)),
 ((0,3,0),(2.4,.12,4)),((-1.68,1.5,3.9),(.58,1.35,.1)),((1.68,1.5,3.9),(.58,1.35,.1)),
 ((0,2.7,3.9),(1.1,.15,.1)),((0,.42,-3.9),(2.3,.27,.1)),((0,2.7,-3.9),(2.3,.3,.1)),
 ((-2.05,1.5,-3.9),(.25,.95,.1)),((2.05,1.5,-3.9),(.25,.95,.1)),
 ((0,1.5,-3.9),(1.8,.95,.1)),((0,.68,-2.85),(.85,.53,.4)),((0,.5,-1.65),(.42,.13,.42)),((0,.92,-1.22),(.42,.43,.1)),((-1.35,.55,1),(.5,.2,1.05)),((1.35,.55,1),(.5,.2,1.05))]
text=transform((0,.18,48))+body((2.4,1.5,4),'dynamic',1200).replace('body dynamic box','body dynamic compound').replace('  body.compound-count 0\n','')+f'  body.compound-count {len(hull)}\n'
for pos,half in hull:text+=f'  body.compound-box {v(pos)} {v(half)}\n'
objects.append(obj(3000,'WAYFARER / walkable spacecraft',text))
for n,(pos,half) in enumerate(hull):
 if pos==(0,1.5,-3.9):
  global_glass=obj(counter,'Wayfarer windshield',transform(pos)+render(half,(.38,.68,.78)).replace('render.alpha 1','render.alpha .18')+'  parent 3000\n');objects.append(global_glass);counter+=1
 else:piece('Wayfarer cabin hull',pos,half,(.18,.28,.32) if n else (.26,.32,.35),3000)
# Trim, landing stripes, large glazed-looking open windshield, chairs and console.
for x in [-2.42,2.42]:
 piece('Copper hull stripe',(x,1.8,0),(.03,.13,3.7),(.92,.48,.17),3000)
 piece('Engine nacelle',(x*1.25,.35,1.3),(.45,.5,1.65),(.12,.19,.24),3000)
 piece('Engine cyan exhaust',(x*1.25,.35,3),(.3,.3,.04),(.18,.8,.95),3000)
for z in [-3.2,2.7]:piece('Deck aisle lighting',(0,.157,z),(.65,.006,.13),(.35,.8,.85),3000)
piece('Captain chair seat',(0,.5,-1.65),(.42,.13,.42),(.63,.3,.16),3000)
piece('Captain chair back',(0,.92,-1.22),(.42,.43,.1),(.63,.3,.16),3000)
for x in [-1.35,1.35]:
 piece('Cabin bench',(x,.55,1),(.5,.2,1.05),(.48,.56,.5),3000)
 piece('Cabin bench back',(x*1.4,1.1,1),(.08,.5,1.05),(.3,.4,.38),3000)
objects.append(obj(3004,'Seated captain',transform((0,0,-1.7),(0,0,1,0))+render((.5,.5,.5),(1,1,1),'mesh',ids['pilot_seated'])+'  parent 3000\n  render.visible false\n  animation.enabled true\n  animation.play-on-start true\n  animation.loop true\n  animation.clip "PilotSeat"\n  animation.speed 1\n  animation.time 0\n'))
objects.append(obj(3003,'Command console / E to sit',transform((0,1.3,-2.8))+render((.75,.08,.35),(.22,.8,.78))+'  parent 3000\n'))
objects.append(obj(3001,'Wayfarer boarding door',transform((0,1.53,51.96))+render((.98,1.17,.08),(.77,.44,.17))+body((.98,1.17,.08),'dynamic',22)))
objects.append(obj(3002,'Wayfarer ordinary physical door hinge',transform()+'''  joint 1
  joint.body-a 3001
  joint.body-b 3000
  joint.anchor-a 1 0 0
  joint.anchor-b 1 1.35 3.96
  joint.frame-a .707106781 0 0 .707106781
  joint.frame-b .707106781 0 0 .707106781
  joint.enabled true
  joint.limits true
  joint.lower -.05
  joint.upper 1.6
  joint.motor false
  joint.speed 0
  joint.max-force 0
  joint.spring true
  joint.rest 0
  joint.stiffness 450
  joint.damping 90
'''))
# Distinct small spherical worlds. Real sphere collision + ordinary RadicalGravity.
planets=[('Cedar',(260,-8,0),25,(.3,.52,.28),'broadleaf_tree'),('Ember',(-230,15,-130),28,(.55,.32,.2),'broadleaf_tree'),('Frost',(70,65,-310),24,(.48,.65,.69),'pine_tree')]
def frame(center,r,x,z):
 y=math.sqrt(max(1,r*r-x*x-z*z));n=(x/r,y/r,z/r);s=math.sqrt(2*(1+n[1]));q=(s/2,n[2]/s,0,-n[0]/s)
 return tuple(center[k]+n[k]*r for k in range(3)),q,n
def rotate(q,p):
 w,x,y,z=q;a,b,c=p
 return ((1-2*y*y-2*z*z)*a+(2*x*y-2*z*w)*b+(2*x*z+2*y*w)*c,(2*x*y+2*z*w)*a+(1-2*x*x-2*z*z)*b+(2*y*z-2*x*w)*c,(2*x*z-2*y*w)*a+(2*y*z+2*x*w)*b+(1-2*x*x-2*y*y)*c)
def house(name,center,r,x,z,color):
 pos,q,n=frame(center,r,x,z)
 def part(label,p,h,c):
  local=rotate(q,p);piece(name+' '+label,tuple(pos[k]+local[k] for k in range(3)),h,c,collision=True,q=q)
 part('floor',(0,.06,0),(3,.12,3),(.36,.4,.4));part('roof',(0,3.2,0),(3.2,.16,3.2),(.22,.25,.3))
 for dx in [-2.9,2.9]:part('wall',(dx,1.65,0),(.1,1.5,3),color)
 part('rear wall',(0,1.65,-2.9),(3,1.5,.1),color)
 for dx in [-1.95,1.95]:part('open doorway',(dx,1.65,2.9),(1.05,1.5,.1),color)
 part('door lintel',(0,2.8,2.9),(.9,.35,.1),color)
 part('table',(1.5,.8,-1.5),(.6,.8,.5),(.45,.3,.19));part('bed',(-1.2,.4,-1.5),(.7,.4,1),(.57,.6,.52))
for j,(name,center,r,color,tree) in enumerate(planets):
 i=4000+j*1000
 objects.append(obj(i,name+' island planet',transform(center)+render((r,r,r),color,'sphere').replace('render.radius .5',f'render.radius {r}')+body((r,r,r)).replace('body static box','body static sphere').replace('body.radius .5',f'body.radius {r}')+f'  gravity radial 9.81\n  gravity.region sphere {r+70}\n'))
 piece(name+' landing plaza',(center[0],center[1]+r+.12,center[2]),(6,.12,5),(.38,.43,.45),collision=True)
 for x in [-5.5,5.5]:piece(name+' landing beacon',(center[0]+x,center[1]+r+.4,center[2]+4.5),(.25,.4,.25),(.3,.9,.82))
 house(name+' refuge',center,r,-10,-4,(.61,.58,.45));house(name+' watch house',center,r,10,-5,(.44,.5,.52))
 for x,z in [(-12,10),(12,10),(-16,1),(16,0),(-4,-16),(4,-17),(-8,14),(8,14),(0,17)]:
  pos,q,n=frame(center,r,x,z);piece(name+' tree',pos,(.5,.5,.5),(1,1,1),q=q,mesh=tree)
 for x,z in [(-4,-6),(4,-6),(-7,5),(7,5)]:
  pos,q,n=frame(center,r,x,z);piece(name+' supply crate',pos,(.5,.5,.5),(1,1,1),q=q,mesh='produce_crate')
scene=P/'Scenes/town.judas';old=scene.read_text();old=re.sub(r'object (?:[3-9]\d{3}|[1-9]\d{4,}) .*?\nend\n','',old,flags=re.S)
old=re.sub(r'next-id \d+','next-id 7000',old,count=1);scene.write_text(old.rstrip()+'\n\n'+'\n'.join(objects))
print('Appended',len(objects),'editable spacecraft/planet entities; highest child ID',counter-1)
