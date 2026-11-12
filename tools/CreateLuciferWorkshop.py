#!/usr/bin/env python3
"""Create the editable Lucifer acceptance courtyard from original Judas geometry.
Never overwrite an existing project. No private downloaded vehicle assets.
"""
import hashlib,json,math,shutil,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
OUT=Path(sys.argv[1]) if len(sys.argv)>1 else ROOT/'projects/lucifer_workshop'
if (OUT/'Lucifer_Workshop.judasproj').exists():raise SystemExit('Refusing to overwrite an existing project')
def write(path,text):
 p=OUT/path;p.parent.mkdir(parents=True,exist_ok=True);p.write_text(text)
def asset(path,kind,text=None):
 id=hashlib.md5(('Lucifer-owned/'+path).encode()).hexdigest()
 if text is not None:write(path,text)
 write(path+'.judasmeta',f'JudasAssetMeta 1\nid "{id}"\ntype {kind}\nsource "Original Judas Lucifer Workshop"\n');return id
for path in ['Assets/Fonts','Assets/Models','Assets/Materials','Assets/scripts','Assets/ui']:(OUT/path).mkdir(parents=True,exist_ok=True)
shutil.copyfile(ROOT/'assets/fonts/DejaVuSans.ttf',OUT/'Assets/Fonts/DejaVuSans.ttf')
shutil.copyfile(ROOT/'assets/fonts/DejaVuSans-LICENSE.txt',OUT/'Assets/Fonts/LICENSE.txt')
font=asset('Assets/Fonts/DejaVuSans.ttf','font')
materialText='JudasMaterial 1\nmodel 1\nalpha 0\nbase 1 1 1 1\nmetallic 0\nroughness .65\nemissive 0 0 0\nemission 1\nnormal 1\nocclusion 1\ncutoff .5\ndoubleSided 0\nflipV 0\nuv 1 1 0 0\n'+''.join(f'map {i} "" 10497 10497 9987 9729\n' for i in range(5))
surfaceMaterial=asset('Assets/Materials/surface.judasmat','material',materialText)

wheel=asset('Assets/Models/tread.obj','mesh',(ROOT/'projects/vehicle_v2_workshop/Assets/Models/tread.obj').read_text())
asset('Assets/scripts/player.js','script',(ROOT/'docs/judasjs/examples/first-person-player.js').read_text()+'\nexport {add,mul,qm,axis,rotate};\n')
script=asset('Assets/scripts/night.js','script',(ROOT/'tools/lucifer_workshop/night.js').read_text())
# Ordinary runtime UI, authored through the same format as other current projects.
elements=[]
def element(id,kind=2,text='',pos=(0,0),size=(600,28),fontSize=18,color=(.9,.95,1,1),bg=(0,0,0,0),parent='canvas'):
 fields=[id,parent,kind,0,1,1,0,0,1,0,0,0,0,*pos,*size,0,0,0,0,0,0,0,0,0,0,0,0,0,0,*bg,*color,8,fontSize,0,0,1,text,'',font]
 elements.append(' '.join(json.dumps(v) if isinstance(v,str) else str(v) for v in fields))
element('canvas',0,parent='',size=(1280,720))
element('panel',1,pos=(20,20),size=(980,180),bg=(.015,.025,.04,.88))
element('title',text='LUCIFER / NIGHT WATCH',pos=(36,30),fontSize=24,color=(.2,1,.85,1))
element('controls',text='WASD + mouse | Space jump | F flashlight | LMB shoot lamp',pos=(36,67),fontSize=18)
element('controls2',text='E patrol | Q emergency lights | N day/night | K/L save/load | R restart | Esc pointer',pos=(36,94),fontSize=17)
element('status',text='Explore the lit courtyard. Aim at a hanging bulb and shoot.',pos=(36,126),size=(770,25),fontSize=16,color=(1,.75,.45,1))
element('exposure',text='Exposure: waiting for physical sample',pos=(36,154),size=(940,25),fontSize=16,color=(.3,.9,.8,1))
element('crosshair',text='+',pos=(630,348),size=(20,24),fontSize=20)
ui=asset('Assets/ui/night.judasui','ui','JudasUI 1\n1280 720 1 1 1 '+str(len(elements))+'\n'+'\n'.join(elements)+'\n')
lines=['JudasScene 3','settings',' name "Lucifer / Night Watch"',' world-origin 0 0 0',' sun-enabled false',' sun-direction -0.3 0.8 0.5',' sun-color 0.12 0.16 0.3',' ambient 0.015 0.02 0.035',' background-color 0.008 0.012 0.025',' linear-rendering true',' exposure 1',' fidelity-policy none',' next-id 10000','end']
def vec(a):return ' '.join(f'{x:.8g}' for x in a)
def obj(id,name,pos=(0,0,0),fields=None,parent=None,rot=(1,0,0,0),scale=(1,1,1)):
 lines.extend([f'object {id} {json.dumps(name)}',' position '+vec(pos),' rotation '+vec(rot),' scale '+vec(scale)])
 if parent is not None:lines.append(' parent '+str(parent))
 f=dict(fields or {})
 if 'render' in f:
  f.setdefault('render.materials',json.dumps('1 '+json.dumps(surfaceMaterial)+' 0'))
  for k,v in {'render.half-extents':'.5 .5 .5','render.radius':'.5','render.color':'1 1 1','render.alpha':'1','render.secondary-color':'.8 .8 .8','render.secondary-alpha':'1','render.mesh-asset':'""','render.texture-asset':'""'}.items():f.setdefault(k,v)
 if 'body' in f:
  for k,v in {'body.half-extents':'.5 .5 .5','body.radius':'.5','body.mass':'1','body.friction':'.8','body.restitution':'0','body.initial-velocity':'0 0 0','body.managed':'false','body.compound-count':'0'}.items():f.setdefault(k,v)
 if 'motor.enabled' in f:
  for k,v in {'motor.offset':'0 0 0','motor.maxSlopeDegrees':'50','motor.reorientationDegreesPerSecond':'120','motor.interactionMass':'80','motor.maxPushImpulse':'20','motor.collisionLayer':'0','motor.collisionMask':'18446744073709551615','motor.requiredTags':'0','motor.excludedTags':'0'}.items():f.setdefault(k,v)
 if 'ui.asset' in f:f.setdefault('ui.enabled','true')
 for k,v in f.items():lines.append(' '+k+' '+str(v))
 lines.append('end')
def box(id,name,pos,half,color,parent=None,physical=False):
 f={'render':'box','render.half-extents':vec(half),'render.color':vec(color)}
 if physical:f.update({'body':'static box','body.half-extents':vec(half),'body.friction':'.8'})
 obj(id,name,pos,f,parent)
def light(id,name,pos,color,intensity=35,range=12,kind='point',shadow=True,parent=None):
 obj(id,name,pos,{'light':kind,'light.color':vec(color),'light.intensity':intensity,'light.range':range,'light.cone':'14 25','light.shadows':str(shadow).lower()},parent)
obj(1,'Gravity',fields={'gravity':'uniform 9.81','gravity.region':'sphere 10000'})
box(2,'Courtyard',(0,-.2,-4),(24,.2,25),(.18,.20,.23),physical=True)
box(3,'Road',(0,.012,-5),(3.5,.012,24),(.055,.065,.075))
for n,z in enumerate(range(-26,20,4)):box(10+n,'Road marking',(0,.03,z),(.06,.012,.8),(.65,.68,.62))
for i,x in enumerate([-24,24]):box(30+i,'Boundary',(x,2,-4),(.25,2,25),(.25,.28,.34),physical=True)
box(32,'Back wall',(0,2,-29),(24,2,.3),(.28,.32,.38),physical=True)
# A real room with a doorway: directional and local light occlusion controls.
box(40,'Room left wall',(-13,2,-15),(.2,2,6),(.42,.33,.26),physical=True)
box(41,'Room right wall',(-5,2,-15),(.2,2,6),(.42,.33,.26),physical=True)
box(42,'Room back wall',(-9,2,-21),(4,2,.2),(.42,.33,.26),physical=True)
box(43,'Doorway left',(-12,2,-9),(1,2,.2),(.42,.33,.26),physical=True)
box(44,'Doorway right',(-6,2,-9),(1,2,.2),(.42,.33,.26),physical=True)
box(45,'Door lintel',(-9,3.6,-9),(2,.4,.2),(.42,.33,.26),physical=True)
box(46,'Room roof',(-9,4.2,-15),(4.3,.2,6.3),(.24,.29,.32),physical=True)
for n,(x,z) in enumerate([(-10,1),(9,-2),(-9,-15),(12,-19)]):
 base=200+n
 obj(base,'Breakable bulb '+str(n+1),(x,3.7,z),{'light':'point','light.color':'1 .70 .37','light.intensity':65,'light.range':13,'light.shadows':'true','render':'sphere','render.radius':'.23','render.color':'1 .75 .35','body':'static sphere','body.radius':'.3'})
 if n!=2:
  box(210+n,'Lamp post',(x+.6,1.8,z),(.075,1.8,.075),(.12,.14,.17),physical=True)
  box(220+n,'Lamp arm',(x+.3,3.8,z),(.4,.07,.07),(.12,.14,.17))
# Additional low-cost illumination across the route, plus window signs.
for n in range(16):
 x=(-1 if n%2 else 1)*(8+(n//4)%2*7);z=12-(n//2)*5
 light(230+n,'Courtyard light '+str(n+1),(x,2.8,z),(.25,.55,1) if n%3 else (1,.35,.17),12,7,shadow=False)
for n in range(4):
 box(60+n,'Shadow test column',(5+n*2,1.3,-12),(.3,1.3,.3),(.45,.47,.50),physical=True)
 box(70+n,'Planter',(-18,.35,8-n*8),(1.2,.35,1.2),(.29,.32,.36),physical=True)
 obj(80+n,'Foliage',(-18,1.3,8-n*8),{'render':'sphere','render.radius':1.2,'render.color':'.10 .28 .13'})
# Human-scale articulated visuals, driven by ordinary project script/kinematic bodies.
for n in range(8):
 root=300+n*10;x=-5+n%4*4;z=5-(n//4)*11
 x+=math.sin(n*.65)*1.5;z+=math.cos(n*.65)*1.5
 obj(root,'Pedestrian '+str(n+1),(x,.95,z),{'body':'kinematic box','body.half-extents':'.3 .9 .25','render':'box','render.half-extents':'.28 .35 .19','render.color':vec((.2+n*.05,.25,.48-n*.025))})
 obj(root+1,'Head',(0,.58,0),{'render':'sphere','render.radius':'.18','render.color':'.61 .40 .28'},root)
 for limb,xside in [(2,-.37),(3,.37)]:box(root+limb,'Arm',(xside,-.04,0),(.075,.32,.075),(.25,.32,.44),root)
 for limb,xside in [(4,-.14),(5,.14)]:box(root+limb,'Leg',(xside,-.60,0),(.095,.30,.10),(.06,.08,.11),root)
# Original patrol car, a kinematic assembly for light attachment tests.
obj(500,'Patrol cruiser',(7,.68,1),{'body':'kinematic box','body.half-extents':'.9 .45 2.1','render':'box','render.half-extents':'.88 .25 2.1','render.color':'.025 .035 .05'},rot=(math.sqrt(.5),0,-math.sqrt(.5),0))
box(501,'White cabin',(0,.42,.15),(.75,.25,.95),(.72,.76,.80),500)
box(502,'Windscreen',(0,.43,-.82),(.67,.18,.02),(.05,.12,.17),500)
for side in [-1,1]:box(503+(side+1)//2,'Door stripe',(side*.89,.05,0),(.015,.18,1.0),(.8,.83,.86),500)
for n,(x,z) in enumerate([(-.93,-1.3),(.93,-1.3),(-.93,1.3),(.93,1.3)]):
 obj(510+n,'Wheel',(x,-.25,z),{'render':'mesh','render.mesh-asset':json.dumps(wheel),'render.color':'.035 .045 .055'},500,rot=(math.sqrt(.5),0,0,math.sqrt(.5)),scale=(.65,.13,.65))
for n,x in enumerate([-.64,.64]):
 light(520+n,'Headlight',(x,.08,-2.12),(1,.91,.70),75,22,'spot',parent=500)
 box(522+n,'Headlight lens',(x,.08,-2.13),(.21,.10,.02),(1,.9,.55),500)
for n,(x,col) in enumerate([(-.4,(1,.025,.02)),(.4,(.025,.18,1))]):
 light(530+n,'Emergency light',(x,.82,.15),col,30,11,parent=500)
 box(532+n,'Emergency lens',(x,.82,.15),(.25,.09,.18),col,500)
light(100,'Handheld flashlight',(0,2,7),(1,.94,.79),70,20,'spot')
obj(101,'Player',(0,1.05,11),{'motor.enabled':'true','motor.radius':'.3','motor.halfHeight':'.6','motor.stepHeight':'.35','motor.supportDistance':'.15','motor.skin':'.02','motor.gravityScale':'1','scripts':'1','script.0.id':'1','script.0.asset':json.dumps(script),'script.0.enabled':'true','script.0.properties':'"{}"','ui.asset':json.dumps(ui),'ui.name':'"night"'})
write('Scenes/night.judas','\n'.join(lines)+'\n')
actions=[('move_x',True,[('key:D',1),('key:A',-1)]),('move_y',True,[('key:W',1),('key:S',-1)]),('look_x',True,[('mouse:dx',1)]),('look_y',True,[('mouse:dy',1)])]
for name,key in [('jump','Space'),('pause','Escape'),('torch','F'),('day','N'),('patrol','E'),('emergency','Q'),('save','K'),('load','L'),('reset','R'),('fire','MouseLeft')]:actions.append((name,False,[('mouse:Left' if key=='MouseLeft' else 'key:'+key,1)]))
inputmap='1 '+str(len(actions))+' '+' '.join(json.dumps(name)+' '+str(int(axis))+' '+str(len(bindings))+' '+' '.join(json.dumps(key)+' '+str(scale)+' 0' for key,scale in bindings) for name,axis,bindings in actions)
write('Lucifer_Workshop.judasproj','JudasProject 1\nname "Lucifer / Night Watch"\nadvanced-fluid-dynamics "false"\nstartup-scene "Scenes/night.judas"\nassets-dir "Assets"\nscenes-dir "Scenes"\nsaves-dir "Saves"\ninput-map '+json.dumps(inputmap)+'\n')
write('README.md','# Lucifer / Night Watch\n\nOriginal editable lighting acceptance courtyard. WASD/mouse, Space jump, F flashlight, LMB shoot bulbs, E patrol motion, Q emergency lights, N day/night, K/L save/load, R restart, Esc pointer.\n\nThe patrol cruiser and pedestrians are project-scripted kinematic assemblies; this fixture tests moving light/caster interaction. Native Vehicles V2 integration is qualified separately. All geometry is original Judas content; DejaVu licence is in Assets/Fonts.\n')
print(OUT)
