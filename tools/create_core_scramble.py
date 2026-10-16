#!/usr/bin/env python3
"""Create ordinary editable M89 Core Scramble scene/assets, never engine policy."""
import copy,hashlib,json,math,shutil,wave,struct
from pathlib import Path
R=Path(__file__).resolve().parents[1];P=R/'projects/core_scramble'
def write(p,v):p.parent.mkdir(parents=True,exist_ok=True);p.write_text(json.dumps(v,indent=2)+'\n')
def asset(path,kind):
 f=P/'Assets'/path;f.parent.mkdir(parents=True,exist_ok=True);i=hashlib.sha256(('M89 Core Scramble '+path).encode()).hexdigest()[:32];f.with_name(f.name+'.judasmeta').write_text(f'JudasAssetMeta 1\nid "{i}"\ntype {kind}\nsource "Original M89 Core Scramble"\n');return i
ids={name:asset('scripts/'+name+'.js','script')for name in ['game','math','controls']};ids['hud']=asset('ui/hud.judasui','ui');ids['local']=asset('ui/player.judasui','ui');font=asset('fonts/DejaVuSans.ttf','font');shutil.copyfile(R/'assets/fonts/DejaVuSans.ttf',P/'Assets/fonts/DejaVuSans.ttf');shutil.copyfile(R/'projects/breakwater/Assets/fonts/LICENSE.txt',P/'Assets/fonts/LICENSE.txt')
base=json.loads((R/'projects/breakwater/Scenes/arcade.judas').read_text())['data'];settings=copy.deepcopy(base['settings']);settings.update(name='CORE SCRAMBLE',ambient=[.40,.44,.51],**{'sun-direction':[.4,1,.6],'sun-color':[1,.94,.84]})
template=next(o['fields']for o in base['objects']if o['id']==100)
seq=1
colors=[[.16,.85,.77],[1,.39,.35],[1,.72,.16],[.65,.41,1]]
def obj(name,p,h,col,motion=None,shape='box',radius=.5,tag=0,mass=1):
 global seq
 f={k:copy.deepcopy(v)for k,v in template.items()if k.startswith('render.')or(motion and k.startswith('body.'))};f.update(position=p,rotation=[1,0,0,0],scale=[1,1,1],render={'symbol':shape},tags=tag)
 f.update({'render.half-extents':h,'render.radius':radius,'render.color':col})
 if motion:f.update(body={'motion':{'symbol':motion},'shape':{'symbol':shape}},**{'body.half-extents':h,'body.radius':radius,'body.mass':mass,'body.friction':.35,'body.restitution':.45})
 result={'id':seq,'name':name,'fields':f};seq+=1;return result
def scene(objects,kind='scene'):return {'kind':kind,'schema':1,'data':{'settings':settings,'objects':objects}}
objects=[obj('Workshop floor',[0,-.5,0],[11.7,.5,11.7],[.045,.072,.11],'static')]
for pos,h in [([0,1.1,-11.4],[11.7,1.1,.3]),([0,1.1,11.4],[11.7,1.1,.3]),([-11.4,1.1,0],[.3,1.1,11.4]),([11.4,1.1,0],[.3,1.1,11.4])]:objects.append(obj('Safety bumper',pos,h,[.13,.23,.30],'static'))
for i in range(-5,6):
 objects.append(obj('Floor inlay',[i*2,.015,0],[.015,.015,11],[.08,.14,.19]))
 objects.append(obj('Floor inlay',[0,.016,i*2],[11,.015,.015],[.08,.14,.19]))
for i,(x,z)in enumerate([(-8,-8),(8,8),(8,-8),(-8,8)]):
 objects.append(obj('Goal deck',[x,.025,z],[2.25,.025,2.25],colors[i]))
 for off,h in [((0,2.15),(2.2,.16)),((0,-2.15),(2.2,.16)),((2.15,0),(.16,2.2)),((-2.15,0),(.16,2.2))]:objects.append(obj('Goal marking',[x+off[0],.08,z+off[1]],[h[0],.05,h[1]],[v*.5 for v in colors[i]]))
for x,z in [(-4,0),(4,0),(0,-4),(0,4)]:objects.append(obj('Center guard',[x,.5,z],[.8,.5,.8],[.19,.29,.34],'static'))
for x,z in [(-3,-3),(3,3),(-3,3),(3,-3)]:objects.append(obj('Loose cargo',[x,.7,z],[.65,.65,.65],[.46,.30,.17],'dynamic',mass=2,tag=4))
objects.append({'id':900,'name':'Arena gravity','fields':{'position':[0,0,0],'rotation':[1,0,0,0],'scale':[1,1,1],'gravity':{'kind':{'symbol':'uniform'},'magnitude':9.81},'gravity.region':{'shape':{'symbol':'sphere'},'radius':1000}}})
objects.append({'id':950,'name':'Core Scramble game','fields':{'position':[0,0,0],'rotation':[1,0,0,0],'scale':[1,1,1],'scripts':1,'script.0.id':1,'script.0.asset':ids['game'],'script.0.enabled':True,'script.0.properties':'{}','ui.asset':ids['hud'],'ui.name':'scramble','ui.enabled':True}})
for i,hz in enumerate([540,860]):
 name=['bump','goal'][i];aid=asset('audio/'+name+'.wav','audio');ids[name]=aid
 with wave.open(str(P/'Assets/audio'/(name+'.wav')),'wb')as w:w.setparams((1,2,48000,0,'NONE','none'));w.writeframes(b''.join(struct.pack('<h',int(9000*math.sin(2*math.pi*hz*t/48000)*math.exp(-t/2800)))for t in range(9000)))
 objects.append({'id':960+i,'name':name,'fields':{'position':[0,0,0],'rotation':[1,0,0,0],'scale':[1,1,1],'audio.asset':aid,'audio.enabled':True,'audio.play-on-start':False,'audio-emitter':[],'audio.volume':.3,'audio.loop':False,'audio.pitch':1,'audio.reference-distance':1,'audio.maximum-distance':30,'audio.rolloff':1,'audio.attenuation':{'symbol':'none'},'audio.spatial':False}})
write(P/'Scenes/arena.judas',scene(objects))
for i,col in enumerate(colors):
 name='bot'+str(i);ids[name]=asset('prefabs/'+name+'.judasprefab','prefab');bot=obj('Bumper bot',[0,1,0],[.7,.7,.7],col,'dynamic','sphere',.7,1,8);bot['id']=1;bot['fields']['body.restitution']=.3;bot['fields']['body.friction']=.25;write(P/'Assets/prefabs'/(name+'.judasprefab'),scene([bot],'prefab'))
ids['core']=asset('prefabs/core.judasprefab','prefab');core=obj('Energy core',[0,1,0],[.42,.42,.42],[.9,.96,1],'dynamic','sphere',.42,2,1);core['id']=1;core['fields']['body.restitution']=.65;write(P/'Assets/prefabs/core.judasprefab',scene([core],'prefab'))
project=json.loads((R/'projects/breakwater/breakwater.judasproj').read_text());d=project['data'];d.update(name='Core Scramble',saveIdentity=hashlib.sha256(b'M89 Core Scramble').hexdigest(),startupScene='Scenes/arena.judas',exportScenes=['Scenes/arena.judas'],input=[]);d['classification']['tags']={'nextId':3,'entries':[{'id':0,'name':'bot'},{'id':1,'name':'core'},{'id':2,'name':'cargo'}]}
def action(n,keys):d['input'].append({'name':n,'axis':False,'bindings':[{'control':k,'scale':1,'deadzone':0}for k in keys]})
for n,keys in {'pause':['key:Escape','key:AC Back','pad:Back'],'split':['key:V','pad:RightShoulder'],'restart':['key:Backspace'],'two':['key:F2'],'three':['key:F3'],'four':['key:F4'],'ui_click':['mouse:Left'],'ui_activate':['key:Return','pad:South'],'ui_up':['key:Up','pad:DpadUp'],'ui_down':['key:Down','pad:DpadDown']}.items():action(n,keys)
write(P/'core_scramble.judasproj',project)
baseUI=json.loads((R/'projects/breakwater/Assets/ui/hud.judasui').read_text())['data']['elements'][0]
def document(ref):
 root=copy.deepcopy(baseUI);root.update(id='canvas',font=font);return {'reference':ref,'visible':True,'enabled':True,'modal':False,'elements':[root]}
def element(doc,id,text,off,size,kind=2,bg=[0,0,0,0],fontSize=24,parent='canvas',anchor=[0,0],align=[0,0],color=[.92,.96,1,1]):
 e=copy.deepcopy(baseUI);e.update(id=id,parent=parent,font=font,text=text,kind=kind,offset=off,size=size,anchorMin=anchor,anchorMax=anchor,align=align,background=bg,fontSize=fontSize,color=color,textAlign=[.5,.5]if kind==4 else[0,.5]);doc['elements'].append(e);return e
hud=document([960,720]);element(hud,'title','CORE SCRAMBLE',[0,16],[600,50],fontSize=40,anchor=[.5,0],align=[.5,0]);element(hud,'timer','90  /  PUSH CORES INTO YOUR COLOUR',[0,66],[650,35],fontSize=22,anchor=[.5,0],align=[.5,0],color=[.55,.79,.85,1])
element(hud,'lobby','LOBBY',[-28,24],[150,46],4,[.12,.27,.33,1],20,anchor=[1,0],align=[1,0])
for id,text,x in [('two','2 PLAYERS',-270),('three','3 PLAYERS',-90),('four','4 PLAYERS',90),('split','SPLIT / V',270)]:element(hud,id,text,[x,-60],[170,48],4,[.09,.20,.27,1],18,anchor=[.5,1],align=[.5,1])
element(hud,'hint','WASD + SPACE   |   ARROWS + RSHIFT   |   F2/F3/F4 ROSTER',[0,-10],[920,36],fontSize=16,anchor=[.5,1],align=[.5,1],color=[.6,.72,.79,1]);element(hud,'notice','PHYSICAL CORES. FOUR GOALS. ONE WINNER.',[0,-112],[900,38],fontSize=18,anchor=[.5,1],align=[.5,1])
panel=element(hud,'shade','',[0,0],[0,0],1,[.02,.04,.07,.96]);panel['anchorMax']=[1,1];panel['visible']=False
element(hud,'menu_title','ARENA PAUSED',[0,-160],[700,64],fontSize=34,parent='shade',anchor=[.5,.5],align=[.5,.5]);
for id,text,y in [('resume','RESUME',-35),('restart','NEW ROUND',50),('quit','EXIT GAME',135)]:element(hud,id,text,[0,y],[470,66],4,[.13,.31,.38,1],26,parent='shade',anchor=[.5,.5],align=[.5,.5])
for i in range(4):element(hud,'input'+str(i),'P'+str(i+1)+' INPUT',[-130 if i%2==0 else 130,220+(i//2)*80],[240,60],4,[.12,.29,.36,1],22,parent='shade',anchor=[.5,.5],align=[.5,.5])
write(P/'Assets/ui/hud.judasui',{'kind':'ui','schema':1,'data':hud})
portrait=copy.deepcopy(hud);portrait['reference']=[720,1280];
for e in portrait['elements']:
 if e['id']=='notice':e['size']=[700,38];e['fontSize']=16
 if e['id']=='hint':e['text']='DRAG TO MOVE | TAP SHOVE | LOBBY TO PAUSE';e['size']=[700,36];e['fontSize']=14
ids['hudPortrait']=asset('ui/hud_portrait.judasui','ui');write(P/'Assets/ui/hud_portrait.judasui',{'kind':'ui','schema':1,'data':portrait})
local=document([480,480]);element(local,'strip','',[12,12],[456,66],1,[.025,.055,.085,.90]);element(local,'label','P1 / CYAN',[25,17],[270,28],fontSize=24);element(local,'score','0 CORES  /  KEYBOARD',[25,46],[330,22],fontSize=16,color=[.6,.83,.85,1]);element(local,'options','...',[-20,23],[64,43],4,[.13,.28,.34,1],26,anchor=[1,0],align=[1,0]);element(local,'cooldown','SHOVE READY',[0,-10],[380,25],fontSize=18,anchor=[.5,1],align=[.5,1])
panel=element(local,'panel','',[0,0],[0,0],1,[.025,.05,.08,.95]);panel['anchorMax']=[1,1];panel['visible']=False
element(local,'help','YOUR CONTROLS PAUSED / ARENA LIVE',[0,-150],[450,40],fontSize=16,parent='panel',anchor=[.5,.5],align=[.5,.5]);
for id,text,y in [('resume','RESUME',-70),('device','CHANGE DEVICE',0),('leave','LEAVE ARENA',70)]:element(local,id,text,[0,y],[380,56],4,[.14,.28,.36,1],22,parent='panel',anchor=[.5,.5],align=[.5,.5])
element(local,'move_hint','DRAG',[24,-64],[130,40],1,[.08,.20,.26,.72],18,anchor=[0,1],align=[0,1]);element(local,'shove','SHOVE',[-24,-64],[140,46],4,[.14,.38,.43,.90],20,anchor=[1,1],align=[1,1])
write(P/'Assets/ui/player.judasui',{'kind':'ui','schema':1,'data':local})
asset('scripts/content.js','script');(P/'Assets/scripts/content.js').write_text('export const CONTENT='+json.dumps(ids)+';\n');print(P)
