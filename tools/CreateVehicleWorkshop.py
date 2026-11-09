#!/usr/bin/env python3
"""Author editable Vehicles V2 content. Physics and driving remain ordinary components/JS.

Usage: python3 tools/CreateVehicleWorkshop.py [new-output-directory]
Never overwrites an existing project. Assets are original procedural content,
except the redistributed DejaVu font with its accompanying licence.
"""
import copy
import hashlib
import json
import math
from pathlib import Path
import shutil
import sys
import subprocess

ROOT=Path(__file__).resolve().parents[1]
OUT=Path(sys.argv[1]) if len(sys.argv)>1 else ROOT/'projects/vehicle_v2_workshop'
if (OUT/'Vehicle_V2_Workshop.judasproj').exists():
    raise SystemExit('Refusing to overwrite an existing editable vehicle project.')

def write(path,text):
    path=OUT/path;path.parent.mkdir(parents=True,exist_ok=True);path.write_text(text,encoding='utf-8')

def asset(path,kind,text=None):
    identifier=hashlib.md5(('judas-vehicle-v2/'+str(path)).encode()).hexdigest()
    if text is not None:write(path,text)
    write(str(path)+'.judasmeta',f'JudasAssetMeta 1\nid "{identifier}"\ntype {kind}\nsource "Original Judas Vehicles V2 Workshop"\n')
    return identifier

driver=asset('Assets/Scripts/vehicle-driver.js','script',(ROOT/'docs/judasjs/examples/vehicle-driver.js').read_text())
controller=asset('Assets/Scripts/workshop.js','script',(ROOT/'tools/vehicle_workshop/controller.js').read_text())
font='Assets/Fonts/DejaVuSans.ttf'
(OUT/'Assets/Fonts').mkdir(parents=True,exist_ok=True)
shutil.copyfile(ROOT/'assets/fonts/DejaVuSans.ttf',OUT/font)
shutil.copyfile(ROOT/'assets/fonts/DejaVuSans-LICENSE.txt',OUT/'Assets/Fonts/LICENSE.txt')
font_id=asset(font,'font')

def cylinder():
    lines=['# Original vehicle tread source; radius .5, half length 1, local Y.']
    for y in (-1,1):
        for n in range(32):
            a=n*2*math.pi/32;lines.append(f'v {.5*math.cos(a):.9g} {y} {.5*math.sin(a):.9g}')
    lines+=['v 0 -1 0','v 0 1 0','s 1']
    for n in range(32):
        a=n+1;b=(n+1)%32+1;c=a+32;d=b+32
        lines.extend([f'f {a} {c} {d}',f'f {a} {d} {b}',f'f 65 {a} {b}',f'f 66 {d} {c}'])
    return '\n'.join(lines)+'\n'
wheel_mesh=asset('Assets/Models/tread.obj','mesh',cylinder())

def vector(value):return ' '.join(str(v) for v in value)
objects=[]
def obj(identifier,name,position=(0,0,0),parent=0,fields=None,rotation=(1,0,0,0),scale=(1,1,1)):
    text=f'object {identifier} {json.dumps(name)}\n'
    if parent:text+=f'  parent {parent}\n'
    for key,value in (fields or {}).items():text+=f'  {key} {value}\n'
    text+=f'  position {vector(position)}\n  rotation {vector(rotation)}\n  scale {vector(scale)}\nend\n'
    objects.append(text)

def render(half,color):
    return {'render':'box','render.half-extents':vector(half),'render.radius':'.5',
            'render.color':vector(color),'render.alpha':'1','render.secondary-color':'.8 .8 .8',
            'render.secondary-alpha':'1','render.mesh-asset':'""','render.texture-asset':'""'}
def body(half,mass=0,motion='static'):
    return {'body':f'{motion} box','body.half-extents':vector(half),'body.radius':'.5','body.mass':str(max(mass,1)),
            'body.friction':'.8','body.restitution':'0','body.initial-velocity':'0 0 0',
            'body.managed':'true' if motion=='dynamic' else 'false','body.compound-count':'0'}
def box(identifier,name,position,half,color,mass=0,rotation=(1,0,0,0)):
    obj(identifier,name,position,fields={**render(half,color),**body(half,mass,'dynamic' if mass else 'static')},rotation=rotation)

obj(1,'Uniform gravity',fields={'gravity':'uniform 9.81','gravity.region':'box 3000 3000 3000'})
box(2,'Ground',(-15,-.5,-20),(45,.5,140),(.2,.29,.22))
box(3,'Driving lane',(0,-.02,-40),(28,.04,90),(.25,.28,.31))
angle=.12
box(4,'Climbing ramp',(0,8*math.sin(angle)-.25*math.cos(angle),-35),(7,.25,8),(.55,.52,.38),rotation=(math.cos(angle/2),math.sin(angle/2),0,0))
for n in range(12):box(10+n,'Rough strip',(18,.065,-25-n*1.8),(3,.065,.25),(.42,.43,.45))
for n in range(6):box(30+n,'Loose collision crate',(-7+n*2,.5,-15),( .45,.5,.45),(.72,.43,.18),30)
box(40,'Wall beside support test',(-19,.6,0),(.18,.6,8),(.35,.48,.56))
for identifier,x in [(41,-59),(42,95)]:box(identifier,'Boundary barrier',(x,.4,-30),(.3,.4,85),(.7,.65,.3))

# Native Terrain V2 off-road patch: its collider is not covered by a hidden box.
terrain={'format':'JudasTerrainV2','version':2,'samplesX':65,'samplesZ':65,'chunkCells':16,'spacing':1.0,
         'bottom':-16,'top':16,'edits':[],'layerColours':[[.25,.36,.12],[.38,.30,.19],[.42,.43,.40],[.7,.65,.48]],
         'layerTextures':['','','',''],'weights':[[.8,.2,0,0]]*(65*65)}
terrain['heights']=[min(1,max(0,(x-2)/12))*max(0,1-abs(z-32)/30)*(.6+.5*math.sin(z*.22)+.35*math.sin(x*.27)) for z in range(65) for x in range(65)]
write('Authoring/offroad.judasterrain2',json.dumps(terrain,separators=(',',':'))+'\n')
(OUT/'Assets/Terrain').mkdir(parents=True,exist_ok=True)
subprocess.run([ROOT/'build/judas_terrain_author','v2-pack',OUT/'Authoring/offroad.judasterrain2',OUT/'Assets/Terrain/offroad.judasterrain'],check=True)
terrain_id=asset('Assets/Terrain/offroad.judasterrain','terrain')
obj(50,'Terrain V2 off-road test',(62,0,-25),fields={**render((32,8,32),(1,1,1)),**body((32,8,32)),'terrain.asset':json.dumps(terrain_id)})

layout=[]
def vehicle(root,name,axles,mass,position,color,powered=True,radius=.35):
    length=1.9*(axles-1)*.5+.4
    fields={**render((.72,.2,length),color),**body((.72,.2,length),mass,'dynamic')}
    fields.update({'body':'dynamic compound','body.compound-count':'2','body.compound-box':f'0 0 0 .72 .2 {length}\n  body.compound-box 0 .55 0 .55 .35 {length*.45}'})
    obj(root,name,position,fields=fields)
    obj(root+90,'Cabin',(0,.55,0),root,render((.55,.35,length*.45),tuple(c*.65+.1 for c in color)))
    shafts=[];sources=[]
    for n in range(axles*2):
        wheel_id=root+1+2*n
        # A cargo trailer has a rear tandem, not two widely separated car axles.
        axle_z=(.175+.65*(n//2)) if not powered else 1.9*(n//2-(axles-1)*.5)
        mount=(-.9 if n%2==0 else .9,-.1,axle_z)
        stiffness=mass*9.81/(axles*2*.08);damping=1.8*math.sqrt(stiffness*mass/(axles*2))
        obj(wheel_id,f'Axle {n//2+1} '+('left' if n%2==0 else 'right'),tuple(mount[i]+(0,-.4,0)[i] for i in range(3)),root,{
            'wheel.enabled':'true','wheel.chassis':str(root),'wheel.mount':vector(mount),'wheel.orientation':'1 0 0 0',
            'wheel.radius':str(radius),'wheel.width':'.24','wheel.minLength':'.08','wheel.restLength':'.4','wheel.maxLength':'.7',
            'wheel.springStiffness':str(stiffness),'wheel.damping':str(damping),'wheel.spinInertia':str(4*radius*radius),
            'wheel.friction':'1.05','wheel.rollingResistance':'.015','wheel.collisionLayer':'0','wheel.collisionMask':'18446744073709551615',
            'wheel.requiredTags':'0','wheel.excludedTags':'0'})
        obj(wheel_id+1,'Tread',(0,0,0),wheel_id,{**render((.5,.5,.5),(.055,.065,.075)),'render':'mesh','render.mesh-asset':json.dumps(wheel_mesh)},
            rotation=(math.sqrt(.5),0,0,math.sqrt(.5)),scale=(2*radius,.12,2*radius))
        if powered and n//2>=axles-2:
            shafts.append({'name':f'wheel_{wheel_id}','kind':'wheel','inertia':1,'initialAngularVelocity':0,'viscousDrag':0,'target':str(wheel_id),'bearing':'0','axis':[1,0,0]})
            sources.append({'name':f'motor_{wheel_id}','shaft':len(shafts)-1,'kind':'electric','torqueCurve':[],
                            'maxTorque':mass*9.81*radius/(axles*2)*.65,'maxPower':max(10000,mass*20),'starterTorque':0})
    if powered:
        graph={'version':1,'enabled':True,'shafts':shafts,'sources':sources,'links':[],'brakes':[]}
        # Insert the graph into its root block, not into the last visual child.
        root_index=next(i for i,text in enumerate(objects) if text.startswith(f'object {root} '))
        objects[root_index]=objects[root_index].replace('  position ',f'  powertrain.graph {json.dumps(json.dumps(graph,separators=(",",":")))}\n  position ',1)
        layout.append({'id':str(root),'name':name,'rig':[str(root)],'brakeTorque':mass*9.81*radius/(axles*2)*1.2})

vehicle(100,'Combustion road car',2,500,(-14,1,12),(.1,.7,.73))
vehicle(200,'Six-wheel utility truck',3,2200,(-5,1.2,16),(.8,.38,.12),radius=.5)
vehicle(300,'Eight-wheel bus',4,4000,(7,1.2,18),(.58,.28,.72),radius=.5)
vehicle(400,'Tow vehicle',2,700,(20,1,10),(.24,.58,.85))
vehicle(500,'Cargo trailer',2,900,(20,1,15),(.65,.7,.75),False)
layout[-1]['rig'].append('500')
layout[-1]['brakes']={'400':700*9.81*.35/4*1.2,'500':900*9.81*.35/4*1.2}
# The car demonstrates the existing combustion/clutch/reduction/open-diff
# mechanics. Other layouts use independent electric wheel motors.
car_graph={'version':1,'enabled':True,'shafts':[
    {'name':'engine','kind':'internal','inertia':.25,'initialAngularVelocity':0,'viscousDrag':.01,'target':'0','bearing':'100','axis':[-1,0,0]},
    {'name':'input','kind':'internal','inertia':.1,'initialAngularVelocity':0,'viscousDrag':.01,'target':'0','bearing':'100','axis':[-1,0,0]},
    {'name':'carrier','kind':'internal','inertia':.1,'initialAngularVelocity':0,'viscousDrag':0,'target':'0','bearing':'100','axis':[-1,0,0]},
    *[{'name':name,'kind':'wheel','inertia':1,'initialAngularVelocity':0,'viscousDrag':0,'target':str(wheel),'bearing':'0','axis':[1,0,0]} for name,wheel in [('left',105),('right',107)]]],
    'sources':[{'name':'engine','shaft':0,'kind':'combustion','torqueCurve':[[0,0],[800,65],[2500,150],[5000,120],[6500,0]],'maxTorque':160,'maxPower':100000,'starterTorque':120}],
    'links':[{'name':'clutch','kind':'clutch','input':0,'output':1,'outputB':2,'ratio':1,'maxTorque':200},
             {'name':'gearbox','kind':'gear','input':1,'output':2,'outputB':2,'ratio':.25,'maxTorque':200},
             {'name':'open','kind':'differential','input':2,'output':3,'outputB':4,'ratio':1,'maxTorque':200}], 'brakes':[]}
index=next(i for i,text in enumerate(objects) if text.startswith('object 100 '))
lines=objects[index].splitlines()
objects[index]='\n'.join('  powertrain.graph '+json.dumps(json.dumps(car_graph,separators=(',',':'))) if line.startswith('  powertrain.graph ') else line for line in lines)+'\n'
obj(600,'Articulated tow hitch',fields={'joint':'2','joint.body-a':'400','joint.body-b':'500','joint.anchor-a':'0 0 1.8','joint.anchor-b':'0 0 -3.2',
    'joint.frame-a':'1 0 0 0','joint.frame-b':'1 0 0 0','joint.enabled':'true','joint.limits':'false','joint.motor':'false','joint.spring':'false',
    'joint.lower':'-1','joint.upper':'1','joint.speed':'0','joint.max-force':'10000','joint.rest':'0','joint.stiffness':'10','joint.damping':'1'})
asset('Assets/Scripts/layout.js','script','export const vehicles='+json.dumps(layout,indent=2)+';\n')

ui_data=json.loads((ROOT/'projects/physics_stress_lab/Assets/ui/stress.judasui').read_text())
base=copy.deepcopy(ui_data['data']['elements'][0]);base['id']='canvas'
panel=copy.deepcopy(base);panel.update(id='hud',parent='canvas',kind=1,anchorMin=[0,0],anchorMax=[0,0],size=[1050,195],offset=[16,16],background=[.025,.035,.05,.88])
label=copy.deepcopy(base);label.update(id='telemetry',parent='hud',kind=2,anchorMin=[0,0],anchorMax=[0,0],size=[1020,175],offset=[14,10],font=font_id,fontSize=19,wrap=True,text='VEHICLES V2 — loading',color=[.85,.95,1,1])
ui_data['data']['elements']=[base,panel,label]
hud=asset('Assets/UI/workshop.judasui','ui',json.dumps(ui_data,indent=2)+'\n')
obj(1000,'Workshop controls',fields={'ui.asset':json.dumps(hud),'ui.name':'"vehicle_workshop"','ui.enabled':'true',
    'scripts':'1','script.0.id':'1','script.0.asset':json.dumps(controller),'script.0.enabled':'true','script.0.properties':'"{}"'})
settings='JudasScene 3\nsettings\n  name "Vehicles V2 Workshop"\n  world-origin 0 0 0\n  sun-direction -.4 .8 .5\n  sun-color 1 .97 .9\n  ambient .28 .3 .34\n  background-color .12 .2 .3\n  fidelity-policy none\n  next-id 1100\nend\n\n'
write('Scenes/workshop.judas',settings+'\n'.join(objects))

actions=[('vehicle_throttle',True,[('key:W',1),('key:S',-1)]),('vehicle_steer',True,[('key:A',-1),('key:D',1)]),
         ('vehicle_brake',False,[('key:Space',1)]),('pause',False,[('key:Escape',1)]),('save',False,[('key:K',1)]),
         ('load',False,[('key:L',1)]),('reset',False,[('key:R',1)])]
actions += [(f'select_{n}',False,[(f'key:{n}',1)]) for n in range(1,5)]
tokens=['1',str(len(actions))]
for name,axis,bindings in actions:
    tokens += [json.dumps(name),'1' if axis else '0',str(len(bindings))]
    for control,scale in bindings:tokens += [json.dumps(control),str(scale),'0']
write('Vehicle_V2_Workshop.judasproj','JudasProject 1\nname "Vehicles V2 Workshop"\nbasic-water "true"\nwater-swimming "true"\nadvanced-fluid-dynamics "false"\nstartup-scene "Scenes/workshop.judas"\nassets-dir "Assets"\nscenes-dir "Scenes"\nsaves-dir "Saves"\ninput-map '+json.dumps(' '.join(tokens))+'\n')
write('README.md','# Vehicles V2 Workshop\n\nEditable project-owned driving, native MK2 chassis, finite-width wheels and powertrains.\n\nW/S drive/brake (release and press again to reverse); A/D steer; Space brake; 1/2/3/4 select car/truck/bus/tow rig; K/L save/load; R reload; Esc pause/resume.\n\nThe tow rig uses an ordinary ball joint and unpowered trailer. Paint-free original procedural geometry; DejaVu font licence is in Assets/Fonts. Physics limits and qualification are in the engine Vehicles V2 documentation.\n')
print(OUT/'Vehicle_V2_Workshop.judasproj')
