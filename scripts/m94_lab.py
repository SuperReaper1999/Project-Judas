#!/usr/bin/env python3
"""Build a small ordinary, editable developer-workflow project."""
import json,copy,hashlib,shutil
from pathlib import Path
R=Path(__file__).resolve().parents[1];root=R/'projects/developer_lab';A=root/'Assets'
for p in [A/'scripts',A/'ui',A/'fonts',root/'Scenes']:p.mkdir(parents=True,exist_ok=True)
def identity(s):return hashlib.sha256(('Judas.M94.'+s).encode()).hexdigest()[:32]
def meta(path,kind,key):path.with_name(path.name+'.judasmeta').write_text(f'JudasAssetMeta 1\nid "{identity(key)}"\ntype {kind}\nsource "{path.relative_to(root).as_posix()}"\n')
for name in ['lab','worker','calibration'] :meta(A/'scripts'/f'{name}.js','script',name)
for name in ['DejaVuSans.ttf','LICENSE.txt']:shutil.copyfile(R/'projects/asset_lab/Assets/fonts'/name,A/'fonts'/name)
meta(A/'fonts/DejaVuSans.ttf','font','font');meta(A/'ui/workshop.judasui','ui','ui')
project=json.loads((R/'projects/asset_lab/asset_lab.judasproj').read_text());p=project['data'];p.update(name='Judas / The Repair Bench',saveIdentity=hashlib.sha256(b'Judas.M94.developer-lab').hexdigest(),startupScene='Scenes/workshop.judas',visualBudgetMiB=0,exportScenes=['Scenes/workshop.judas'],runtimeAssets=[identity(k)for k in ['lab','worker','calibration','ui']]);p['input']=[dict(name=n,axis=False,bindings=[dict(control='key:'+k,scale=1.,deadzone=0.)])for n,k in [('pause','Escape'),('repair_test','E'),('reset','R')]]
(root/'developer_lab.judasproj').write_text(json.dumps(project,indent=2)+'\n')
scene=json.loads((R/'projects/asset_lab/Scenes/museum.judas').read_text());scene['data']['objects']=[];scene['data']['settings'].update(name='The Repair Bench',**{'world-origin':[0,0,0],'next-id':1000})
def obj(i,name,pos,fields):scene['data']['objects'].append(dict(id=i,name=name,fields=dict(position=pos,rotation=[1,0,0,0],scale=[1,1,1],**fields)))
def script(n):return {'scripts':1,'script.0.id':1,'script.0.asset':identity(n),'script.0.enabled':True,'script.0.properties':'{}'}
obj(1,'Workshop presentation',[0,0,0],{**script('lab'),'ui.asset':identity('ui'),'ui.name':'workshop','ui.enabled':True})
base=copy.deepcopy(json.loads((R/'projects/asset_lab/Scenes/museum.judas').read_text())['data']['objects'][1]['fields']);base={k:v for k,v in base.items()if k not in ['position','rotation','scale']}
def box(i,name,pos,size,color,extra={}):obj(i,name,pos,{**base,'render.half-extents':size,'render.color':color,**extra})
box(10,'Workshop floor',[0,-.3,0],[9,.3,7],[.085,.12,.16]);box(11,'Repair bench',[0,1.1,0],[3,.18,1.2],[.24,.32,.37])
for i,x in enumerate([-2.5,2.5]):box(20+i,'Bench support',[x,.5,0],[.15,.5,.8],[.12,.18,.23])
box(30,'Calibrated assembly',[0,1.9,0],[.6,.6,.6],[.9,.58,.16],script('worker'))
for i in range(6):box(40+i,'Diagnostic strip',[-2.5+i,1.4,-1],[.3,.035,.08],[.12,.8,.7])
(root/'Scenes/workshop.judas').write_text(json.dumps(scene,indent=2)+'\n')
u=json.loads((R/'projects/asset_lab/Assets/ui/asset.judasui').read_text());elements=u['data']['elements'];elements=[e for e in elements if e['id']in ['canvas','title','help','status','pause','resume','reload']]
for e in elements:
 if e['font']:e['font']=identity('font')
 if e['id']=='title':e['text']='JUDAS / THE REPAIR BENCH'
 if e['id']=='help':e['text']='E: test calibration (intentional fault until repaired) | R: reload scene | Esc: pause\nEditor: View > JudasJS developer tools | Check project scripts | choose worker.js line 10'
 if e['id']=='status':e['text']='The amber assembly is driven by worker.js. Its neighbours survive an isolated fault.'
 if e['id']=='reload':e['text']='RELOAD AUTHORED SCENE'
u['data']['elements']=elements;(A/'ui/workshop.judasui').write_text(json.dumps(u,indent=2)+'\n')
