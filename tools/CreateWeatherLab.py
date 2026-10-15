#!/usr/bin/env python3
"""Build the editable M86 consumer lab from ordinary registered assets.
No native demo dispatch; this script only authors content. It overwrites this lab.
"""
from pathlib import Path
import copy,hashlib,json,math,random,shutil,wave,array
ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'projects/weather_lab'
OLD=ROOT/'projects/planetary_terrain_lab'
def aid(name):return hashlib.md5(('judas-m86-weather-lab/'+name).encode()).hexdigest()
def write(path,data):
 p=OUT/path;p.parent.mkdir(parents=True,exist_ok=True);p.write_text(json.dumps(data,indent=2)+'\n' if not isinstance(data,str) else data)
def meta(path,kind):write(str(path)+'.judasmeta',f'JudasAssetMeta 1\nid "{aid(str(path))}"\ntype {kind}\nsource "M86 original editable weather lab"\n')
def asset(path,kind,data):write(path,data);meta(path,kind);return aid(str(path))
def copyasset(old,new,kind):
 p=OUT/new;p.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(OLD/old,p);meta(new,kind);return aid(str(new))
def obj(i,name,p=(0,0,0),fields=None):return {'id':i,'name':name,'fields':{'position':list(p),'rotation':[1,0,0,0],'scale':[1,1,1],**(fields or {})}}
settings=copy.deepcopy(json.loads((OLD/'Scenes/earth.judas').read_text())['data']['settings'])
settings['name']='M86 Weather Laboratory';settings['world-origin']=[0,0,0];settings['next-id']=1000;settings['ambient']=[.23,.29,.34];settings['sun-direction']=[-.5,.68,.48]
def scene(objects,title,origin=(0,0,0)):
 s=copy.deepcopy(settings);s['name']=title;s['world-origin']=list(origin);return {'kind':'scene','schema':1,'data':{'settings':s,'objects':objects}}
def box(i,name,p,half,color,body='static',mass=20,response=None):
 f={'render':{'symbol':'box'},'render.half-extents':half,'render.radius':.5,'render.color':color,'render.alpha':1,'render.secondary-color':[.8]*3,'render.secondary-alpha':1,'render.mesh-asset':'','render.texture-asset':''}
 if body:f.update({'body':{'motion':{'symbol':body},'shape':{'symbol':'box'}},'body.half-extents':half,'body.radius':.5,'body.terrain':'','body.mass':mass,'body.friction':.25 if body=='dynamic' else .7,'body.restitution':.03,'body.initial-velocity':[0,0,0],'body.pickable':False,'body.managed':False,'body.compound-count':0,'body.fluid-cavity-count':0})
 if response:f['weather.response']=json.dumps({'version':1,'enabled':True,'area':response,'dragCoefficient':1.2,'maxAcceleration':12,'occlusionMask':2**64-1},separators=(',',':'))
 return obj(i,name,p,f)
font=copyasset('Assets/fonts/DejaVuSans.ttf','Assets/fonts/DejaVuSans.ttf','font');shutil.copy2(OLD/'Assets/fonts/LICENSE.txt',OUT/'Assets/fonts/LICENSE.txt')
meta('Assets/scripts/profiles.js','script')
mathid=copyasset('Assets/scripts/math.js','Assets/scripts/math.js','script')
traveller=copyasset('Assets/prefabs/traveller.judasprefab','Assets/prefabs/traveller.judasprefab','prefab')
probe=copyasset('Assets/prefabs/probe.judasprefab','Assets/prefabs/probe.judasprefab','prefab')
# Export closure includes scripts imported by the controller and runtime prefabs.
scriptid=aid('Assets/scripts/lab.js');meta('Assets/scripts/lab.js','script')
u=json.loads((OLD/'Assets/ui/planet.judasui').read_text());elements=u['data']['elements']
for e in elements:
 if e.get('font'):e['font']=font
 if e['id']=='title':e.update(text='JUDAS / WEATHER OBSERVATORY',fontSize=24,color=[.85,.94,1,1],offset=[24,16],size=[1000,36])
 if e['id']=='help':e.update(text='1 SUN  /  2 CLOUD  /  3 RAIN  /  4 STORM  /  5 TORNADO  /  6 SNOW  /  7 BLIZZARD  /  8 FOG\nF5 WATCH TOUR   |   F9 FLAT / PLANET   |   Q QUALITY   |   0 VISUALS OFF / ON   |   ESC PAUSE',fontSize=15,anchorMin=[0,1],anchorMax=[0,1],offset=[24,-84],size=[1220,64],color=[.8,.88,.92,1])
 if e['id']=='status':e.update(text='Preparing the observatory...',fontSize=16,anchorMin=[0,1],anchorMax=[0,1],offset=[24,-205],size=[900,110],background=[.025,.055,.07,.75],padding=[12,8,12,8],color=[.92,.97,1,1])
 if e['id']=='reload':e['text']='RELOAD WEATHER LAB'
uiid=asset('Assets/ui/weather.judasui','ui',u)
weather={'version':1,'enabled':True,'seed':860013,'priority':0,'profile':{'cloudCoverage':.12,'cloudDensity':.65,'cloudBase':300,'cloudThickness':450,'rain':0,'snow':0,'fog':0,'wind':[3,0,1],'gust':1,'lightningRate':0,'tornado':0,'tornadoRadius':17,'tornadoHeight':320,'tornadoSpeed':2},'frame':{'radial':False,'center':[0,0,0],'up':[0,1,0],'north':[0,0,-1],'radius':6371000},'regionShape':'global','regionCenter':[0,0,0],'regionHalfExtents':[100,100,100],'regionRotation':[1,0,0,0],'regionRadius':100,'blendDistance':10,'topAltitude':16000,'tornadoOffset':[60,45],'tornadoDirection':[-1,0,.2]}
def controls(planet):return obj(10,'Project-owned exploration, watch tour and UI',fields={'scripts':1,'script.0.id':1,'script.0.asset':scriptid,'script.0.enabled':True,'script.0.properties':json.dumps({'planet':planet}),'ui.asset':uiid,'ui.name':'weather_lab','ui.enabled':True})
def field(planet):
 w=copy.deepcopy(weather);w['frame']['radial']=planet
 if planet:w['profile'].update(cloudBase=1000,cloudThickness=1200)
 return obj(20,'Editable weather profile / F1-F9 are project JS',fields={'weather.region':json.dumps(w,separators=(',',':'))})
# Fixed pavilion with a real solid roof is the shelter test; no name-based logic.
def observatory(first=100):
 objects=[];i=first
 def b(name,p,h,c,body='static',mass=20,response=None):
  nonlocal i;objects.append(box(i,name,p,h,c,body,mass,response));i+=1
 b('Observatory terrace',(0,-.12,-10),(24,.12,28),[.25,.31,.32])
 for x,z in [(-6,-6),(6,-6),(-6,-18),(6,-18)]:b('Solid pavilion post',(x,2,z),[.17,2,.17],[.21,.31,.36])
 b('Sheltering roof: ordinary box collision',(0,4.1,-12),[6.5,.24,6.5],[.11,.2,.27])
 b('Pavilion rear wall',(0,1.45,-18),[6,1.45,.18],[.2,.29,.33])
 b('Observation desk',(0,1.05,-15),[2.5,.1,.6],[.59,.39,.2])
 for x in [-2,2]:b('Desk support',(x,.5,-15),[.09,.5,.45],[.3,.22,.17])
 # Heavy crate plus lighter boards expose bounded real wind loading.
 for n in range(5):b('Wind-responsive freight / opt-in pressure area',(10+n*2.3,.65,-8-n*2),[.65,.65,.65],[.56,.37,.2],'dynamic',300 if n==0 else 25,3.5 if n==0 else 1.5)
 for n in range(4):b('Wind-responsive panel',(11+n*2.2,1.05,1),[.75,1.05,.1],[.23,.5,.57],'dynamic',12,3)
 for x in [-18,18]:
  b('Footpath marker',(x,.5,12),[.18,.5,.18],[.86,.71,.31])
 for n in range(8):
  angle=n*math.tau/8;x,z=42*math.cos(angle),42*math.sin(angle)-8
  b('Low-poly windbreak trunk',(x,2,z),[.3,2,.3],[.28,.2,.13])
  b('Low-poly tree crown',(x,4.2,z),[1.3,1.5,1.3],[.15,.3,.2],None)
 return objects
listener=obj(32,'Presentation audio listener',fields={'audio-listener':[],'listener.enabled':True,'listener.follow-view':True})
flat=[controls(False),field(False),listener,obj(2,'Ordinary FaithfulGravity',fields={'gravity':{'kind':{'symbol':'uniform'},'magnitude':9.81},'gravity.region':{'shape':{'symbol':'sphere'},'radius':100000}}),box(3,'Flat meadow collision',(0,-.6,0),[550,.6,550],[.19,.28,.2]),*observatory()]
# Distant geometric ridges supply a horizon without a second weather system.
for n in range(14):
 angle=n*math.tau/14;x,z=300*math.cos(angle),300*math.sin(angle);ridge=box(300+n,'Distant ridge',(x,12+n%4*6,z),[55,16+n%4*6,55],[.2,.3,.27]);ridge['fields']['rotation']=[math.cos(angle*.5),0,math.sin(angle*.5),0];flat.append(ridge)
write('Scenes/flat.judas',scene(flat,'Weather Lab / Flat meadow'))
planet=json.loads((OLD/'Scenes/earth.judas').read_text());objects=planet['data']['objects'];objects[:]=[o for o in objects if o['id'] not in [10]];objects.extend([controls(True),field(True),listener]);planet['data']['settings']['name']='Weather Lab / Earth-scale planetary shell';planet['data']['settings']['sun-direction']=[-.5,.68,.48];planet['data']['settings']['ambient']=[.23,.29,.34]
write('Scenes/planet.judas',planet)
# Shared origin-zero station installs at the current world's absolute surface.
write('Scenes/station.judas',scene(observatory(),'Streamed weather observatory'))
manifest=json.loads((OLD/'Assets/world/planet.judasworld').read_text());r=manifest['data']['regions'][0];r.update(id='weather-station',halfExtents=[70,15,70],estimatedBytes=196608)
worldid=asset('Assets/world/weather.judasworld','world',manifest)
project=json.loads((OLD/'planetary_terrain_lab.judasproj').read_text());data=project['data'];data.update(name='Judas Weather Lab',saveIdentity=hashlib.sha256(b'Judas Weather Lab M86').hexdigest(),startupScene='Scenes/flat.judas',worldManifest=worldid,runtimeAssets=[traveller,probe],exportScenes=['Scenes/flat.judas','Scenes/planet.judas','Scenes/station.judas'])
data['input']=[x for x in data['input'] if x['name'] not in ['tour']]
for name,key in [('watch','F5'),('switch_scene','F9'),('quality','Q'),('visual_off','0'),*[(f'weather_{i}',str(i)) for i in range(1,9)]]:data['input'].append({'name':name,'axis':False,'bindings':[{'control':'key:'+key,'scale':1.,'deadzone':0.}]})
write('weather_lab.judasproj',project)
print('Weather Lab assets authored:',OUT)
