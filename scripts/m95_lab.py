#!/usr/bin/env python3
"""Original skinned source art and ordinary project content for the animation studio."""
import json,base64,struct,math,hashlib,shutil,copy,argparse
from pathlib import Path
from create_m70_lab import make_source
R=Path(__file__).resolve().parents[1];P=R/'projects/animation_studio';A=P/'Assets'
parser=argparse.ArgumentParser();parser.add_argument('--overwrite',action='store_true');args=parser.parse_args()
if (P/'animation_studio.judasproj').exists()and not args.overwrite:raise SystemExit('Refusing to overwrite authored Motion Studio content. Use --overwrite only to regenerate the original lab.')
for folder in ['Sources','Imports','Scenes','Assets/models','Assets/scripts','Assets/fonts','Assets/ui']:(P/folder).mkdir(parents=True,exist_ok=True)
def identity(s):return hashlib.sha256(('Judas.M95.'+s).encode()).hexdigest()[:32]
def meta(path,kind,key):path.with_name(path.name+'.judasmeta').write_text(f'JudasAssetMeta 1\nid "{identity(key)}"\ntype {kind}\nsource "{path.relative_to(P).as_posix()}"\n')
source=P/'Sources/performer.gltf';make_source(source);g=json.loads(source.read_text());blob=bytearray(base64.b64decode(g['buffers'][0]['uri'].split(',')[1]));nodes={n['name']:i for i,n in enumerate(g['nodes'])}
def access(data,kind):
 while len(blob)%4:blob.append(0)
 start=len(blob);flat=[x for row in data for x in (row if isinstance(row,list) else [row])];blob.extend(struct.pack('<'+'f'*len(flat),*flat));g['bufferViews'].append(dict(buffer=0,byteOffset=start,byteLength=len(blob)-start));g['accessors'].append(dict(bufferView=len(g['bufferViews'])-1,componentType=5126,count=len(data),type=kind));return len(g['accessors'])-1
def clip(name,duration,travel=0,amplitude=.0,action=False):
 times=[duration*i/32 for i in range(33)];samplers=[];channels=[]
 def track(node,path,values,kind):
  samplers.append(dict(input=access(times,'SCALAR'),output=access(values,kind),interpolation='LINEAR'));channels.append(dict(sampler=len(samplers)-1,target=dict(node=nodes[node],path=path)))
 track('OriginHelper','translation',[[0,0,-travel*t/duration]for t in times],'VEC3')
 for key,sign in [('ThighA',1),('ThighB',-1),('ShoulderA',-1),('ShoulderB',1)]:
  angles=[amplitude*sign*math.sin(t/duration*2*math.pi)for t in times]
  if action and key=='ShoulderB':angles=[1.4*math.sin(t/duration*math.pi)for t in times]
  track(key,'rotation',[[math.sin(a/2),0,0,math.cos(a/2)]for a in angles],'VEC4')
 for key,sign in [('ShinA',1),('ShinB',-1)]:
  angles=[max(0,sign*math.sin(t/duration*2*math.pi))*amplitude*.8 for t in times];track(key,'rotation',[[math.sin(a/2),0,0,math.cos(a/2)]for a in angles],'VEC4')
 if name=='Jump':track('Vertebra0','rotation',[[math.sin(-.25*math.sin(math.pi*t/duration)/2),0,0,math.cos(-.25*math.sin(math.pi*t/duration)/2)]for t in times],'VEC4')
 return dict(name=name,samplers=samplers,channels=channels)
g['animations']=[clip('Idle',2),clip('Walk',1.2,2.4,.48),clip('Run',.8,3.2,.8),clip('Jump',.9,0,.25),clip('Interact',1.1,0,.04,True),clip('Strike',.8,0,.12,True)]
g['buffers'][0].update(byteLength=len(blob),uri='data:application/octet-stream;base64,'+base64.b64encode(blob).decode());g['asset']['generator']='Judas M95 original CC0 animation studio source';source.write_text(json.dumps(g,separators=(',',':'))+'\n')
clips=[]
for c in g['animations']:
 name=c['name'];events=[]
 if name in ['Walk','Run']:
  duration=1.2 if name=='Walk' else .8;events=[dict(id='left',name='footstep',time=duration*.25,data='left'),dict(id='right',name='footstep',time=duration*.75,data='right')]
 if name=='Strike':events=[dict(id='contact',name='contact',time=.38,data='palm')]
 if name=='Interact':events=[dict(id='interact',name='interact',time=.55,data='beacon')]
 clips.append(dict(sourceClip=name,name=name,loop=name in ['Idle','Walk','Run'],events=events,rootMotion=dict(policy='extract',node='OriginHelper',translation=[True,False,True])))
recipe=dict(format='JudasImport',version=1,assetId=identity('performer'),source='Sources/performer.gltf',output='Assets/models/performer.judasmodel',settings=dict(unitMeters=1,sampleRate=60,dependencyRemaps={}),clips=clips)
(P/'Imports/performer.judasimport').write_text(json.dumps(recipe,indent=2)+'\n')
for n in ['lab','math']:meta(A/'scripts'/f'{n}.js','script',n)
shutil.copyfile(R/'projects/character_lab/Assets/scripts/math.js',A/'scripts/math.js')
for n in ['DejaVuSans.ttf','LICENSE.txt']:shutil.copyfile(R/'projects/developer_lab/Assets/fonts'/n,A/'fonts'/n)
meta(A/'fonts/DejaVuSans.ttf','font','font');meta(A/'ui/studio.judasui','ui','ui')
project=json.loads((R/'projects/developer_lab/developer_lab.judasproj').read_text());p=project['data'];p.update(name='Judas / Motion Studio',saveIdentity=hashlib.sha256(b'Judas.M95.studio').hexdigest(),startupScene='Scenes/studio.judas',exportScenes=['Scenes/studio.judas','Scenes/rotated.judas'],runtimeAssets=[identity(x)for x in ['lab','math','ui','performer']]);p['input']=[dict(name=n,axis=False,bindings=[dict(control='key:'+k,scale=1.,deadzone=0.)])for n,k in [('forward','W'),('back','S'),('left','A'),('right','D'),('run','Left Shift'),('jump','Space'),('strike','E'),('interact','Q'),('pause','Escape'),('reset','R'),('rotate','F2'),('flat','F1'),('tour','T'),('save','F6'),('load','F7')]]
for n,k in [('ui_click','mouse:Left'),('ui_activate','key:Return'),('ui_up','key:Up'),('ui_down','key:Down'),('ui_left','key:Left'),('ui_right','key:Right')]:p['input'].append(dict(name=n,axis=False,bindings=[dict(control=k,scale=1.,deadzone=0.)]))
(P/'animation_studio.judasproj').write_text(json.dumps(project,indent=2)+'\n')
baseScene=json.loads((R/'projects/developer_lab/Scenes/workshop.judas').read_text());base=copy.deepcopy(baseScene['data']['objects'][1]['fields']);base={k:v for k,v in base.items()if k not in ['position','rotation','scale']}
# Ordinary uniform gravity authored identically to the established character lab.
bodyTemplate={k:v for k,v in json.loads((R/'projects/character_lab/Scenes/wall.judas').read_text())['data']['objects'][0]['fields'].items()if k.startswith('body')}
gravity=copy.deepcopy(json.loads((R/'projects/character_lab/Scenes/wall.judas').read_text())['data']['objects'][1]['fields'])
for rotated,name in [(False,'studio'),(True,'rotated')]:
 scene=copy.deepcopy(baseScene);objects=[];scene['data']['objects']=objects;scene['data']['settings'].update(name='Motion Studio / '+name,**{'next-id':1000})
 def transform(v):return [-v[1],v[0],v[2]]if rotated else v
 def obj(i,n,pos,fields):objects.append(dict(id=i,name=n,fields=dict(position=transform(pos),rotation=[math.sqrt(.5),0,0,math.sqrt(.5)]if rotated else [1,0,0,0],scale=[1,1,1],**fields)))
 def box(i,n,pos,size,color,extra={}):
  body={}if n=='Floor lane'else {**copy.deepcopy(bodyTemplate),'body.half-extents':size}
  obj(i,n,pos,{**base,'render.half-extents':size,'render.color':color,**body,**extra})
 box(1,'Studio floor',[0,-.3,0],[10,.3,10],[.08,.12,.17])
 obj(2,'Gravity field',[0,0,0],{k:v for k,v in gravity.items()if k not in ['position','rotation','scale']})
 # uniform field direction is authored in world coordinates
 for k in objects[-1]['fields']:
  if 'acceleration' in k or 'direction' in k:
   v=objects[-1]['fields'][k]
   if isinstance(v,list)and len(v)==3:objects[-1]['fields'][k]=transform(v)
 actor={k:v for k,v in base.items()if not k.startswith('body.')};actor.update({'render':{'symbol':'mesh'},'render.mesh-asset':identity('performer'),'render.color':[1,1,1],'render.half-extents':[1,1,1], 'animation.enabled':True,'animation.play-on-start':True,'animation.clip':'Idle','animation.loop':True,'animation.speed':1.,'animation.time':0.,'character.enabled':True,'character.radius':.28,'character.half-height':.6,'character.offset':[0,.9,0]})
 # Copy established motor field names rather than invent authoring fields.
 actorSource=json.loads((R/'projects/character_lab/Scenes/physical.judas').read_text())['data']['objects'];motor=next(o['fields']for o in actorSource if any(k.startswith('motor.') for k in o['fields']))
 for k,v in motor.items():
  if k.startswith('motor.'):actor[k]=v
 actor={k:v for k,v in actor.items()if not k.startswith('character.')};actor.update({'motor.offset':[0,.9,0],'motor.radius':.28,'motor.halfHeight':.6,'motor.collisionLayer':0,'motor.collisionMask':18446744073709551615})
 obj(10,'Performer / root-motion capsule',[0,.03,4],actor)
 box(30,'Physical timing target',[0,.55,-1],[.55,.55,.55],[.92,.53,.16],{'body':{'motion':{'symbol':'dynamic'},'shape':{'symbol':'box'}},'body.mass':6})
 box(31,'Interaction beacon',[3,1,-1],[.35,1,.35],[.12,.75,.65])
 box(40,'Collision wall',[-3,1.4,-3],[1.6,1.4,.3],[.24,.34,.44])
 for i in range(3):box(50+i,'Practice step',[-5,.1+i*.1,1-i*.65],[1,.1+i*.1,.35],[.32,.42,.5])
 for i in range(11):box(100+i,'Floor lane',[-8+i*1.6,.012,-.5],[.5,.012,.025],[.15,.37,.4])
 obj(90,'Studio game policy',[0,0,0],{'scripts':1,'script.0.id':1,'script.0.asset':identity('lab'),'script.0.enabled':True,'script.0.properties':json.dumps(dict(rotated=rotated)), 'ui.asset':identity('ui'),'ui.name':'studio','ui.enabled':True})
 (P/'Scenes'/f'{name}.judas').write_text(json.dumps(scene,indent=2)+'\n')
u=json.loads((R/'projects/developer_lab/Assets/ui/workshop.judasui').read_text());elements=u['data']['elements']
for e in elements:
 if e['font']:e['font']=identity('font')
 if e['id']=='title':e['text']='JUDAS / MOTION STUDIO'
 if e['id']=='help':e['text']='WASD: move | Shift: run | Space: jump | E: timed strike | Q: interact\nT: watch tour | F1/F2: flat/rotated | F6/F7: save/load | R: reset | Esc: pause'
 if e['id']=='status':e['text']='Animation-driven motion, collision-resolved displacement. Markers decide when game actions happen.'
(A/'ui/studio.judasui').write_text(json.dumps(u,indent=2)+'\n')
(P/'Sources/LICENSE.txt').write_text('Original M95 source geometry and authored motions: CC0-1.0. Built from the original CC0 M70 multipart rig.\n')
print(P)
