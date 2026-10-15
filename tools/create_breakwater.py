#!/usr/bin/env python3
"""Reproducible editable M88 consumer content; no engine gameplay branches."""
import hashlib,json,math,struct,wave
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];P=ROOT/'projects/breakwater'
def write(path,value):
 path.parent.mkdir(parents=True,exist_ok=True);path.write_text(json.dumps(value,indent=2)+'\n')
def asset(path,kind):
 ident=hashlib.sha256(('M88 Breakwater '+path).encode()).hexdigest()[:32]
 f=P/'Assets'/path;f.parent.mkdir(parents=True,exist_ok=True)
 f.with_name(f.name+'.judasmeta').write_text(f'JudasAssetMeta 1\nid "{ident}"\ntype {kind}\nsource "Original M88 Breakwater content"\n');return ident
ids={s:asset('scripts/'+s+'.js','script') for s in ['game','ball','drop']}
ids['hud']=asset('ui/hud.judasui','ui')
for s in ['ball','paddle','wide','drop']:ids[s+'_prefab']=asset('prefabs/'+s+'.judasprefab','prefab')
font=asset('fonts/DejaVuSans.ttf','font');(P/'Assets/fonts/DejaVuSans.ttf').write_bytes((ROOT/'assets/fonts/DejaVuSans.ttf').read_bytes())
for name,hz in [('hit',660),('power',880),('lost',160),('launch',440)]:
 ids[name+'_audio']=asset('audio/'+name+'.wav','audio')
 with wave.open(str(P/'Assets/audio'/f'{name}.wav'),'wb') as w:
  w.setparams((1,2,48000,0,'NONE','not compressed'));w.writeframes(b''.join(struct.pack('<h',int(6500*math.sin(2*math.pi*hz*i/48000)*math.exp(-i/2300))) for i in range(6500)))
TAGS=['controller','paddle','ball','brick','drop','shard']
def obj(i,name,pos,half,color,motion=None,shape='box',script=None,tag=None,radius=.3):
 template=json.loads((ROOT/'projects/physics_stress_lab/Scenes/room128.judas').read_text())['data']['objects'][2]['fields']
 f={k:v for k,v in template.items() if k.startswith('render.') or (motion and k.startswith('body.'))}
 f.update({'position':pos,'rotation':[1,0,0,0],'scale':[1,1,1],'render':{'symbol':shape},'render.half-extents':half,'render.radius':radius,'render.color':color,'render.alpha':1})
 if motion:f.update({'body':{'motion':{'symbol':motion},'shape':{'symbol':shape}},'body.half-extents':half,'body.radius':radius,'body.mass':1,'body.friction':0,'body.restitution':1,'body.initial-velocity':[0,0,0],'body.pickable':False})
 if script:f.update({'scripts':1,'script.0.id':1,'script.0.asset':ids[script],'script.0.enabled':True,'script.0.properties':'{}'})
 if tag is not None:f['tags']=1<<TAGS.index(tag)
 return {'id':i,'name':name,'fields':f}
settings={'name':'BREAKWATER','world-origin':[0,0,0],'sun-direction':[.2,.4,.9],'sun-color':[.9,.95,1],'ambient':[.48,.5,.55],'next-id':1000,'fidelity-policy':{'mode':{'symbol':'none'}}}
settings={**json.loads((ROOT/'projects/tuppence/Scenes/cabinet.judas').read_text())['data']['settings'],**settings}
def scene(objects,kind='scene'):return {'kind':kind,'schema':1,'data':{'settings':settings,'objects':objects}}
colors=[[.22,.85,.76],[.3,.65,1],[.68,.45,.96],[1,.42,.38],[1,.69,.26],[.93,.84,.45]]
objects=[obj(1,'Slate backboard',[0,0,-.7],[9.5,13,.3],[.025,.045,.08])]
objects.append({'id':2,'name':'Ordinary gravity','fields':{'position':[0,0,0],'rotation':[1,0,0,0],'scale':[1,1,1],'gravity':{'kind':{'symbol':'uniform'},'magnitude':6},'gravity.region':{'shape':{'symbol':'sphere'},'radius':1000}}})
for i,(pos,half) in enumerate([([-8.6,0,0],[.35,12.5,1]),([8.6,0,0],[.35,12.5,1]),([0,12.4,0],[8.6,.35,1])],10):objects.append(obj(i,'Solid boundary',pos,half,[.13,.28,.35],'static'))
for row in range(6):
 for col in range(7):
  i=100+row*7+col;x=(col-3)*2.28;y=9.6-row*1.25
  objects.append(obj(i,'Ceramic block', [x,y,0],[1.02,.48,.42],colors[row],'static',tag='brick'))
# One ordinary controller owns the UI; its mesh is not used.
c=obj(50,'Breakwater game',[0,0,0],[0,0,0],[0,0,0],script='game',tag='controller');c['fields']={k:v for k,v in c['fields'].items() if k!='render' and not k.startswith('render.')};c['fields'].update({'ui.asset':ids['hud'],'ui.name':'breakwater','ui.enabled':True});objects.append(c)
for i,name in enumerate(['hit','power','lost','launch'],600):
 a=obj(i,name+' audio',[0,0,0],[0,0,0],[0,0,0]);a['fields']={k:v for k,v in a['fields'].items() if k!='render' and not k.startswith('render.')};a['fields'].update({'audio.asset':ids[name+'_audio'],'audio.enabled':True,'audio.play-on-start':False,'audio-emitter':[],'audio.volume':.22,'audio.loop':False,'audio.pitch':1,'audio.reference-distance':1,'audio.maximum-distance':30,'audio.rolloff':1,'audio.attenuation':{'symbol':'none'},'audio.spatial':False});objects.append(a)
write(P/'Scenes/arcade.judas',scene(objects))
for name,half,col in [('paddle',[1.65,.32,.6],[.22,.85,.76]),('wide',[2.6,.32,.6],[1,.69,.26])]:
 write(P/'Assets/prefabs'/f'{name}.judasprefab',scene([obj(1,'Paddle',[0,-9.5,0],half,col,'kinematic',tag='paddle')],'prefab'))
write(P/'Assets/prefabs/ball.judasprefab',scene([obj(1,'Ball',[0,0,0],[.3,.3,.3],[.97,.96,.88],'dynamic','sphere','ball','ball',.3)],'prefab'))
drop=obj(1,'Power capsule',[0,0,0],[.36,.36,.36],[1,.69,.26],'dynamic','sphere','drop','drop',.36);drop['fields']['body.restitution']=.15
write(P/'Assets/prefabs/drop.judasprefab',scene([drop],'prefab'))
proj={'name':'Breakwater','legacyGameplay':False,'saveIdentity':hashlib.sha256(b'M88 Breakwater saved game').hexdigest(),'startupScene':'Scenes/arcade.judas','assetsDir':'Assets','scenesDir':'Scenes','savesDir':'Saves','exportAssetPolicy':'all','runtimeAssets':[],'input':[], 'classification':{'tags':{'nextId':len(TAGS),'entries':[{'id':i,'name':t}for i,t in enumerate(TAGS)]},'collision':{'nextId':1,'entries':[{'id':0,'name':'Default'}]},'render':{'nextId':1,'entries':[{'id':0,'name':'Default'}]}}}
def action(name,controls,axis=False):proj['input'].append({'name':name,'axis':axis,'bindings':[{'control':c,'scale':s,'deadzone':.15 if c.startswith('stick:') else 0} for c,s in controls]})
action('move',[('key:A',-1),('key:D',1),('key:Left',-1),('key:Right',1),('stick:LeftX',1)],True)
for n,controls in {'launch':['key:Space','pad:South'],'pause':['key:Escape','key:AC Back','pad:Start'],'restart':['key:R','pad:Back'],'ui_click':['mouse:Left'],'ui_activate':['key:Return','key:Space','pad:South'],'ui_back':['key:Escape','key:AC Back','pad:East'],'ui_down':['key:Down','pad:DpadDown'],'ui_up':['key:Up','pad:DpadUp']}.items():action(n,[(c,1)for c in controls])
defaults=json.loads((ROOT/'projects/tuppence/tuppence.judasproj').read_text())['data'];defaults.update(proj);defaults['exportScenes']=['Scenes/arcade.judas'];defaults['excludeScenes']=[]
write(P/'breakwater.judasproj',{'kind':'project','schema':1,'data':defaults})
base=json.loads((ROOT/'projects/tuppence/Assets/ui/cabinet.judasui').read_text())['data']['elements'][0]
ui=[]
def element(id,text,offset,size,kind=2,bg=[0,0,0,0],fontsize=24,parent='canvas',anchor=[0,0],align=[0,0],color=[.9,.95,1,1]):
 e=dict(base);e.update({'id':id,'parent':parent,'text':text,'font':font,'kind':1 if kind==0 else kind,'fontSize':fontsize,'offset':offset,'size':size,'anchorMin':anchor,'anchorMax':anchor,'align':align,'textAlign':[.5,.5] if kind==4 else [0,.5],'background':bg,'color':color,'wrap':False});ui.append(e)
canvas=dict(base);canvas.update({'id':'canvas','font':font});ui.append(canvas)
element('header','',[0,0],[0,120],0,[.025,.045,.08,.98]);ui[-1]['anchorMax']=[1,0];ui[-1]['relativeSize']=[0,0]
element('brand','BREAKWATER',[28,14],[480,46],fontsize=34)
element('score','00000',[28,66],[400,32],fontsize=24,color=[.22,.85,.76,1])
element('pause','II',[-26,24],[72,64],4,[.12,.23,.31,1],28,anchor=[1,0],align=[1,0])
element('footer','',[0,0],[0,150],0,[.025,.045,.08,.98],anchor=[0,1],align=[0,1]);ui[-1]['anchorMax']=[1,1];ui[-1]['relativeSize']=[0,0]
element('hint','DRAG BELOW TO MOVE',[28,-112],[620,30],fontsize=22,anchor=[0,1],color=[.58,.72,.8,1])
element('notice','Launch when ready',[28,-70],[415,40],fontsize=22,anchor=[0,1])
element('launch','LAUNCH',[-26,-26],[190,58],4,[.16,.64,.56,1],24,anchor=[1,1],align=[1,1])
element('shade','',[0,0],[0,0],0,[.015,.025,.05,.95]);ui[-1]['anchorMax']=[1,1];ui[-1]['relativeSize']=[0,0];ui[-1]['visible']=False
for id,text,y,kind in [('menu_title','TAKE A BREATHER',-170,2),('menu_detail','42 blocks. Three lives. One clean sweep.',-100,2),('resume','RESUME',-10,4),('restart','NEW GAME',80,4),('save','SAVE & CONTINUE',170,4),('load','LOAD SAVED GAME',260,4),('quit','EXIT GAME',350,4)]:
 element(id,text,[0,y],[600 if kind==2 else 440,68],kind,[.1,.25,.3,1]if kind==4 else[0,0,0,0],28 if id=='menu_title' else 22,parent='shade',anchor=[.5,.5],align=[.5,.5])
write(P/'Assets/ui/hud.judasui',{'kind':'ui','schema':1,'data':{'reference':[720,1280],'visible':True,'enabled':True,'modal':False,'elements':ui}})
(P/'Assets/scripts/content.js').write_text('export const CONTENT='+json.dumps(ids)+';\n');asset('scripts/content.js','script')
print(P)
