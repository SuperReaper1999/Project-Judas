#!/usr/bin/env python3
"""Original Tuppence cabinet content. Ordinary public Judas assets and bodies."""
from pathlib import Path
import json,hashlib,copy,shutil,math,wave,struct
R=Path(__file__).resolve().parents[1];P=R/'projects/tuppence';A=P/'Assets'
for d in ['scripts','prefabs','ui','fonts','audio']: (A/d).mkdir(parents=True,exist_ok=True)
(P/'Scenes').mkdir(exist_ok=True)
def write(p,x):p.write_text(json.dumps(x,indent=2)+'\n')
def aid(s):return hashlib.sha256(('tuppence/'+s).encode()).hexdigest()[:32]
def meta(p,kind):i=aid(str(p.relative_to(P)));p.with_name(p.name+'.judasmeta').write_text(f'JudasAssetMeta 1\nid "{i}"\ntype {kind}\nsource "Original Tuppence arcade project"\n');return i
for p in (R/'projects/volume_terrain_lab/Assets/fonts').iterdir():
 if p.suffix!='.judasmeta':shutil.copy2(p,A/'fonts'/p.name)
font=meta(next((A/'fonts').glob('*.ttf')),'font')
for name,freq in [('drop',420),('win',1046),('jackpot',1568)]:
 p=A/'audio'/f'{name}.wav';rate=22050;duration=.16 if name=='drop' else .45
 with wave.open(str(p),'w') as w:
  w.setparams((1,2,rate,0,'NONE','not compressed'));w.writeframes(b''.join(struct.pack('<h',int(7000*math.sin(2*math.pi*freq*(1+i/rate*.12)*i/rate)*max(0,1-i/(rate*duration))**2)) for i in range(int(rate*duration))))
 meta(p,'audio')
settings={'name':'TUPPENCE / Ball Pusher','world-origin':[0,0,0],'sun-direction':[.4,.8,.3],'sun-color':[.9,.95,1],'ambient':[.3,.32,.38],'next-id':10000}
baseScene=json.load(open(R/'projects/volume_terrain_lab/Scenes/cave.judas'))
settings={**baseScene['data']['settings'],**settings}
baseBody=json.load(open(R/'projects/volume_terrain_lab/Assets/prefabs/drop.judasprefab'))['data']['objects'][0]['fields']
objects=[]
def obj(id,name,pos,half=None,color=(.1,.15,.2),motion='static',radius=None,physical=True,rotation=None):
 f=copy.deepcopy(baseBody)
 if not physical:
  for key in list(f):
   if key.startswith('body'):del f[key]
 f.update({'position':list(pos),'rotation':rotation or [1,0,0,0],'scale':[1,1,1],'render':{'symbol':'sphere' if radius else 'box'},'render.color':list(color)})
 if radius:f['render.radius']=radius
 else:f['render.half-extents']=list(half)
 if physical:
  f.update({'body':{'motion':{'symbol':motion},'shape':{'symbol':'sphere' if radius else 'box'}},'body.mass':.045 if radius else 20,'body.friction':.38,'body.restitution':.08,'body.pickable':False})
  if radius:f['body.radius']=radius
  else:f['body.half-extents']=list(half)
 o={'id':id,'name':name,'fields':f};objects.append(o);return f
# Cabinet visual skin and actual playfield are deliberately distinct objects.
obj(1,'Midnight plinth',(0,-.65,.3),(3.9,.65,4.4),(.035,.055,.09),physical=False)
obj(2,'Red cabinet back',(0,3.2,-3.0),(3.9,3.2,.25),(.45,.055,.08))
obj(3,'Felt playfield',(0,1.25,.4),(3.25,.15,3.0),(.025,.24,.23)) # lip z=3.7
# A shallow physical upslope retains free-rolling pieces; no vertical stopper.
angle=math.atan(.006/.3)
obj(26,'Shallow ramped payout lip',(0,1.403-.02/math.cos(angle),3.55),(3.25,.02,.15),(.72,.56,.2),rotation=[math.cos(angle/2),-math.sin(angle/2),0,0])
obj(4,'Left physical rail',(-3.38,1.75,.6),(.13,.55,3.3),(.07,.1,.16))
obj(5,'Right physical rail',(3.38,1.75,.6),(.13,.55,3.3),(.07,.1,.16))
obj(6,'PUSHER / short-stroke physical shelf',(0,1.52,-1.25),(3.22,.11,.55),(.85,.63,.2),'dynamic')
objects[-1]['fields']['body.mass']=.5
obj(27,'Fixed shelf backboard',(0,1.94,-.75),(3.22,.3,.05),(.12,.2,.23))
obj(7,'Ball drop nozzle',(0,5.3,-.2),(.32,.16,.3),(.9,.7,.3),physical=False)
obj(8,'Marquee',(0,6,-2.7),(3.65,.5,.15),(.04,.14,.2),physical=False)
for i,x in enumerate([-3.65,3.65]):
 obj(30+i,'Cabinet upright',(x,2.7,-.4),(.16,3,3.1),(.42,.045,.08),physical=False)
 obj(32+i,'Gold edge',(x,2.3,2.8),(.055,2.7,.055),(.95,.67,.2),physical=False)
for row in range(2):
 for col in range(6-row):
  obj(50+row*8+col,'Fixed brass deflector',( -2.65+col*1.06+row*.53,4.15-row*.85,-.2),None,(.85,.65,.2),radius=.14)
# Collectors are below the ledge. Scoring is based on an actual crossed plane.
obj(9,'Collection tray',(0,-.25,4.45),(3.45,.12,.85),(.06,.1,.15))
obj(11,'Collection end',(0,.15,5.25),(3.45,.4,.1),(.1,.18,.23))
for i,x in enumerate([-3.45,-.8,.8,3.45]):obj(12+i,'Pocket divider',(x,.28,4.5),(.06,.45,.72),(.85,.65,.2))
for i,(x,w) in enumerate([(-2.1,1.25),(0,.73),(2.1,1.25)]):obj(18+i,'Prize pocket',(x,-.11,4.5),(w,.025,.65),(.95,.58,.13) if i==1 else (.07,.4,.42),physical=False)
obj(24,'Start button pedestal',(0,.5,6.0),(3.7,.5,.45),(.4,.04,.07),physical=False)
obj(25,'Glowing start button',(0,1.08,5.95),(.42,.08,.25),(.95,.65,.2),physical=False)
# Coated arcade track and smooth brass pieces: authored material properties.
for o in objects:
 if o['id'] in (3,6,26):o['fields']['body.friction']=.04
# Balls begin with clearance and settle physically. No artificial freezing.
for row in range(8):
 for col in range(14):
  f=obj(1000+row*14+col,'Machine ball',(col*.445-2.89,1.63,row*.445+.3),None,(.94,.67,.22),motion='dynamic',radius=.21);f['tags']=1;f['body.friction']=.08
ball=copy.deepcopy(objects[-1]);ball['id']=1;ball['fields']['position']=[0,0,0];ball['name']='Ordinary brass ball'
write(A/'prefabs/ball.judasprefab',{'kind':'prefab','schema':1,'data':{'settings':settings,'objects':[ball]}});ballid=meta(A/'prefabs/ball.judasprefab','prefab')
objects.append({'id':16,'name':'Finite-force shelf drive / native M45 slider','fields':{'position':[0,0,0],'rotation':[1,0,0,0],'scale':[1,1,1],'joint':3,'joint.body-a':6,'joint.body-b':0,'joint.anchor-a':[0,0,0],'joint.anchor-b':[0,1.52,-1.25],'joint.frame-a':[math.sqrt(.5),0,-math.sqrt(.5),0],'joint.frame-b':[math.sqrt(.5),0,-math.sqrt(.5),0],'joint.enabled':True,'joint.limits':True,'joint.lower':-1.05,'joint.upper':.05,'joint.motor':True,'joint.speed':0,'joint.max-force':3,'joint.spring':False,'joint.rest':0,'joint.stiffness':0,'joint.damping':0}})
objects.append({'id':80,'name':'Ordinary faithful gravity','fields':{'position':[0,0,0],'rotation':[1,0,0,0],'gravity':{'kind':{'symbol':'uniform'},'magnitude':9.81},'gravity.region':{'shape':{'symbol':'box'},'halfExtents':[30,30,30]}}})
# One JS owner, no per-ball scripts or callbacks.
owner={'position':[0,0,0],'rotation':[1,0,0,0],'scripts':1,'script.0.id':1,'script.0.asset':aid('Assets/scripts/game.js'),'script.0.enabled':True,'script.0.properties':'{}','ui.asset':aid('Assets/ui/cabinet.judasui'),'ui.name':'tuppence','ui.enabled':True}
for i,n in enumerate(['drop','win','jackpot']):
 o={'id':90+i,'name':n+' sound','fields':{'position':[0,2,2],'audio.asset':aid('Assets/audio/'+n+'.wav'),'audio.enabled':True,'audio.loop':False,'audio-emitter':[],'audio.volume':.25,'audio.play-on-start':False,'audio.pitch':1,'audio.reference-distance':1,'audio.maximum-distance':30,'audio.rolloff':1,'audio.attenuation':{'symbol':'none'},'audio.spatial':False}};objects.append(o)
objects.append({'id':10,'name':'Tuppence game / ordinary JS','fields':owner})
for o in objects:
 for key,value in [('position',[0,0,0]),('rotation',[1,0,0,0]),('scale',[1,1,1])]:o['fields'].setdefault(key,value)
write(P/'Scenes/cabinet.judas',{'kind':'scene','schema':1,'data':{'settings':settings,'objects':objects}})
crowded=copy.deepcopy(objects)
for i in range(96):
 o=copy.deepcopy(objects[next(k for k,v in enumerate(objects) if v['id']==1000+i)]);o['id']=2000+i;o['fields']['position'][1]+=.445;crowded.append(o)
write(P/'Scenes/crowded.judas',{'kind':'scene','schema':1,'data':{'settings':{**settings,'name':'Tuppence / crowded physical cabinet'},'objects':crowded}})
# Coin machine: real flat convex tokens, narrow cabinet and an upper moving shelf.
if (A/'collision/coin.judascollision').exists():
 coinCollision=meta(A/'collision/coin.judascollision','collision');coinMesh=meta(A/'models/coin.glb','mesh');meta(A/'models/coin_collider.glb','mesh')
 token=copy.deepcopy(ball);f=token['fields'];f.update({'render':{'symbol':'mesh'},'render.mesh-asset':coinMesh,'render.color':[.75,.34,.12],'body':{'motion':{'symbol':'dynamic'},'shape':{'symbol':'hull'}},'body.collision-asset':coinCollision,'body.mass':.00712,'body.friction':.42,'body.restitution':0});token['name']='Physical copper 2p token'
 write(A/'prefabs/coin.judasprefab',{'kind':'prefab','schema':1,'data':{'settings':settings,'objects':[token]}});coinid=meta(A/'prefabs/coin.judasprefab','prefab')
 coinObjects=copy.deepcopy([o for o in objects if o['id']<1000 and o['id'] not in (16,27) and not 50<=o['id']<70])
 for o in coinObjects:
  f=o['fields'];f['position'][0]*=.4;f['position'][2]*=.55
  if 'render.half-extents' in f:f['render.half-extents'][0]*=.4;f['render.half-extents'][2]*=.55
  if 'body.half-extents' in f:f['body.half-extents'][0]*=.4;f['body.half-extents'][2]*=.55
  if o['id']==10:f['script.0.properties']=json.dumps({'coins':True})
  if o['id']==7:f['position']=[0,3.2,-.8]
  if o['id']==26:f['position'][1]=1.4+.003*.55-.02/math.cos(angle)
  if o['id']==6:o['name']='PUSHER / prescribed physical body';f['body']['motion']={'symbol':'dynamic'};f['body.mass']=.5;f['position']=[0.0,1.61,-1.5*.55];f['render.half-extents']=[3.22*.4,.2,.7*.55];f['body.half-extents']=[3.22*.4,.2,.7*.55]
 coinObjects.append({'id':16,'name':'Force-limited shelf motor / native M45 slider','fields':{'position':[0,0,0],'rotation':[1,0,0,0],'scale':[1,1,1],'joint':3,'joint.body-a':6,'joint.body-b':0,'joint.anchor-a':[0,0,0],'joint.anchor-b':[0,1.61,-.825],'joint.frame-a':[math.sqrt(.5),0,-math.sqrt(.5),0],'joint.frame-b':[math.sqrt(.5),0,-math.sqrt(.5),0],'joint.enabled':True,'joint.limits':True,'joint.lower':-.75,'joint.upper':.05,'joint.motor':True,'joint.speed':0,'joint.max-force':3,'joint.spring':False,'joint.rest':0,'joint.stiffness':0,'joint.damping':0}})
 for i in range(24):
  o=copy.deepcopy(token);o['id']=1000+i
  if i<20:o['fields']['position']=[(i%4-1.5)*.435,1.422,.31+(i//4)*.435]
  else:o['fields']['position']=[(i-21.5)*.435,1.832,-.8]
  coinObjects.append(o)
 decoration=copy.deepcopy(token);decoration['id']=75;decoration['name']='Original 2p marquee emblem';f=decoration['fields']
 for k in list(f):
  if k.startswith('body') or k=='tags':del f[k]
 f.update({'position':[0,4.4,-2.65],'rotation':[math.sqrt(.5),math.sqrt(.5),0,0],'scale':[4,4,4]});coinObjects.append(decoration)
 write(P/'Scenes/coins.judas',{'kind':'scene','schema':1,'data':{'settings':{**settings,'name':'TUPPENCE / physical 2p coin pusher'},'objects':coinObjects}})
# Declarative UI: all actions have visible buttons and keyboard equivalents.
u=json.load(open(R/'projects/volume_terrain_lab/Assets/ui/terrain.judasui'))
base=copy.deepcopy(u['data']['elements'][0]);elements=[]
def ui(id,parent,kind,xy,sz,text='',bg=(0,0,0,0),size=20,visible=True):
 e=copy.deepcopy(base);e.update(id=id,parent=parent,kind=kind,anchorMin=[0,0],anchorMax=[0,0],offset=list(xy),size=list(sz),align=[0,0],text=text,font=font,fontSize=size,color=[.98,.9,.71,1],background=list(bg),visible=visible,wrap=True,fit=False);
 if kind==4:e['textAlign']=[.5,.5]
 elements.append(e);return e
ui('canvas','',0,(0,0),(0,0)).update(anchorMax=[1,1])
ui('top','canvas',1,(20,16),(1240,76),bg=(.025,.045,.07,.96))
ui('title','top',2,(20,9),(410,40),'TUPPENCE',size=34)
ui('subtitle','top',2,(22,47),(560,28),'BALL PUSHER / Drop brass. Chase the centre pocket.',size=17)
ui('score','top',2,(610,15),(260,35),'SCORE 0',size=28)
ui('bank','top',2,(920,15),(290,35),'60 DROPS',size=28)
ui('goal','top',2,(610,47),(580,25),'TARGET 500  /  BEST 0',size=17)
ui('bottom','canvas',1,(20,616),(1240,88),bg=(.025,.045,.07,.97))
ui('hint','bottom',2,(20,3),(1190,23),'A / D aim the chute. SPACE drops one ball. Centre pocket pays double.',size=19)
ui('aim_left','bottom',4,(20,32),(100,46),'A / <',(.08,.24,.27,1))
ui('drop','bottom',4,(135,29),(350,50),'DROP BALL  [SPACE]',(.65,.14,.1,1),24)
ui('aim_right','bottom',4,(500,32),(100,46),'> / D',(.08,.24,.27,1))
ui('auto','bottom',4,(625,32),(245,46),'AUTOPLAY: OFF',(.08,.24,.27,1))
ui('pause_button','bottom',4,(895,32),(320,46),'MODES / MENU  [ESC]',(.08,.14,.2,1))
ui('notice','canvas',2,(320,120),(640,38),'',size=26).update(textAlign=[.5,.5])
ui('shade','canvas',1,(0,0),(0,0),bg=(.01,.02,.03,.8),visible=False).update(anchorMax=[1,1])
# A compact ordinary cabinet; every visible ball remains fully simulated.
def compact_balls(source):
 result=[]
 for original in source:
  o=copy.deepcopy(original);f=o['fields'];i=o['id']
  if i in (51,53,55,58,60,62):continue
  if i>=1000:
   col=(i%1000)%14
   if col>=7:continue
   f['position'][0]=(col-3)*.445
  else:
   f['position'][0]*=.5
   if i in (50,52,54):f['position'][0]=(i-52)*.53
   if i in (59,61):f['position'][0]=(i-60)*.53
   for key in ['render.half-extents','body.half-extents']:
    if key in f:f[key][0]*=.5
  result.append(o)
 return result
write(P/'Scenes/cabinet.judas',{'kind':'scene','schema':1,'data':{'settings':settings,'objects':compact_balls(objects)}})
write(P/'Scenes/crowded.judas',{'kind':'scene','schema':1,'data':{'settings':{**settings,'name':'Tuppence / crowded physical cabinet'},'objects':compact_balls(crowded)}})
ui('menu','canvas',1,(340,140),(600,440),bg=(.035,.065,.1,1),visible=False)
ui('menu_title','menu',2,(30,18),(540,48),'TUPPENCE / PAUSED',size=30)
ui('menu_info','menu',2,(30,75),(540,70),'Push real balls over the front edge. Gold centre pocket pays double. Reach 500 points.',size=20)
for i,(id,text) in enumerate([('resume','RESUME'),('save','SAVE MACHINE'),('load','LOAD MACHINE'),('restart','NEW MACHINE')]):ui(id,'menu',4,(30+(i%2)*285,166+(i//2)*70),(255,54),text,(.08,.24,.27,1))
ui('mode_balls','menu',4,(30,306),(255,48),'BALL PUSHER',(.08,.24,.27,1))
ui('mode_coins','menu',4,(315,306),(255,48),'2p COIN PUSHER',(.08,.24,.27,1))
ui('quit','menu',4,(30,370),(540,44),'QUIT',(.35,.08,.08,1))
u['data'].update(reference=[1280,720],modal=False,elements=elements);write(A/'ui/cabinet.judasui',u);meta(A/'ui/cabinet.judasui','ui')
meta(A/'scripts/game.js','script')
def binding(name,axis,*controls):return {'name':name,'axis':axis,'bindings':[{'control':c,'scale':scale,'deadzone':0} for c,scale in controls]}
p=json.load(open(R/'projects/volume_terrain_lab/volume_terrain_lab.judasproj'));d=p['data'];d.update(name='Tuppence',startupScene='Scenes/cabinet.judas',saveIdentity=hashlib.sha256(b'tuppence/save-v1').hexdigest(),worldManifest='',runtimeAssets=[ballid]+([coinid] if 'coinid' in locals() else []),input=[binding('aim',True,('key:A',-1),('key:D',1),('stick:LeftX',1)),binding('drop',False,('key:Space',1),('pad:South',1)),binding('pause',False,('key:Escape',1),('pad:Start',1)),binding('ui_click',False,('mouse:Left',1)),binding('ui_activate',False,('key:Return',1)),binding('ui_up',False,('key:Up',1)),binding('ui_down',False,('key:Down',1))]);d['classification']['tags']={'nextId':1,'entries':[{'id':0,'name':'arcade_piece'}]};write(P/'tuppence.judasproj',p)
print(P/'tuppence.judasproj',len(objects),'authored template entities / 56 physical balls in compact cabinet')
