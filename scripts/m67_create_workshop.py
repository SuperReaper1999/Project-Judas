#!/usr/bin/env python3
"""Build ordinary named content through shipped Judas authoring services.
JSON patches and recipe parameters are inputs; no private positional emitter,
Euler/ragdoll fitter or mesh-to-collision generator lives in this script.
"""
import argparse,json,subprocess,copy,shutil,re,hashlib
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
def main():
 parser=argparse.ArgumentParser();parser.add_argument('--tool',default='build/judas_scene_author');parser.add_argument('--project',default='projects/world_workshop');args=parser.parse_args()
 tool=ROOT/args.tool;project=ROOT/args.project;config=project/'world_workshop.judasproj';authoring=project/'Authoring';authoring.mkdir(parents=True,exist_ok=True)
 for folder in ['Scenes','Assets/scripts','Assets/ui','Assets/fonts','Assets/localization','Sources']: (project/folder).mkdir(parents=True,exist_ok=True)
 def run(*cmd):
  result=subprocess.run([str(tool),*[str(x) for x in cmd]],cwd=ROOT,text=True,capture_output=True)
  print(' '.join(str(x) for x in cmd),result.stdout.strip());
  if result.returncode:raise RuntimeError(result.stderr)
  return result.stdout.strip()
 def write(path,data):path.parent.mkdir(parents=True,exist_ok=True);path.write_text(json.dumps(data,ensure_ascii=False,indent=2)+'\n')
 def patch(kind,path,new):
  edits=authoring/'last-edit.json';write(edits,[{'op':'replace','path':'/data','value':new}]);run('--patch',kind,path,path,edits,'--overwrite')
 def template(components,name):
  path=authoring/(name+'.json');run('--object-template',components,name,path,'--overwrite');return json.loads(path.read_text())['data']['objects'][0]
 def track(path,id):return run('--track-asset',config,path,id).splitlines()[-1]
 run('--create','project',config,'--overwrite');p=json.loads(config.read_text())['data'];p.update(name='World Workshop',startupScene='Scenes/workshop.judas',exportScenes=['Scenes/workshop.judas','Scenes/east.judas','Scenes/west.judas'],saveIdentity=hashlib.sha256(b'judas-world-workshop-m67').hexdigest())
 p['input']=[x for x in p['input'] if x['name'] not in ['save_state','delete_state']]
 for name,key in [('save','F6'),('load','F7'),('locale','L'),('ragdoll','G')]:p['input'].append({'name':name,'axis':False,'bindings':[{'control':'key:'+key,'scale':1,'deadzone':0}]})
 patch('project',config,p)
 ids={name:'676767676767676767676767'+f'{i:08x}' for i,name in enumerate(['script','debrisScript','prefab','ui','font','arabic','en','ar','world'],1)}
 for script,label in [('workshop.js','script'),('debris.js','debrisScript')]:
  original=ROOT/'projects/world_workshop/Assets/scripts'/script
  if original.resolve()!=(project/'Assets/scripts'/script).resolve():shutil.copy2(original,project/'Assets/scripts'/script)
  track(project/'Assets/scripts'/script,ids[label])
 for file,label in [('DejaVuSans.ttf','font'),('NotoSansArabic-Regular.ttf','arabic')]:shutil.copy2(ROOT/'projects/text_lab/Assets/fonts'/file,project/'Assets/fonts'/file);track(project/'Assets/fonts'/file,ids[label])
 shutil.copy2(ROOT/'projects/text_lab/Assets/fonts/LICENSE.txt',project/'Assets/fonts/LICENSE.txt')
 for locale,text in [('en','WORLD WORKSHOP / editable content'),('ar','ورشة العالم / محتوى قابل للتعديل')]:
  path=project/'Assets/localization'/(locale+'.judasloc');path.write_text('JudasCatalog 1 "'+locale+'"\n"workshop.title" '+json.dumps(text,ensure_ascii=False)+'\n');track(path,ids[locale])
 p=json.loads(config.read_text())['data'];p['localization']={'defaultLocale':'en','fonts':[ids['font'],ids['arabic']],'locales':{l:{'catalog':ids[l],'fallback':'en' if l=='ar' else '', 'fonts':[ids['font'],ids['arabic']]} for l in ['en','ar']}};patch('project',config,p)
 # UI template is the actual runtime/editor schema, then one validated batch.
 path=project/'Assets/ui/workshop.judasui';run('--create','ui',path,'--overwrite');d=json.loads(path.read_text())['data'];base=d['elements'][0];base['size']=[0,0];base['anchorMax']=[1,1];d['elements']=[base]
 def element(id,kind,text,offset,size,parent='canvas',**kw):
  e=copy.deepcopy(base);e.update(id=id,parent=parent,kind=kind,anchorMax=[0,0],size=size,offset=offset,text=text,background=[0,0,0,.65] if kind==1 else [0,0,0,0]);e.update(kw);d['elements'].append(e)
 element('title',2,'',[24,18],[750,38],textKey='workshop.title',fontSize=26)
 element('help',2,'WASD / mouse: move and look | Space: jump | V: view | Z: initialized prefab | G: rig physics\nWalk along the lane to cross/revisit regions | L: Arabic/English | F6/F7: save/load | R: reload | Esc: pause',[24,60],[1120,90],fontSize=19,wrap=True)
 element('position',2,'',[24,158],[1100,40],fontSize=18);element('status',2,'',[24,202],[1100,40],fontSize=18)
 element('marker',2,'800 m marker',[0,0],[150,35],anchorMin=[.5,.5],anchorMax=[.5,.5],align=[.5,.5],background=[.1,.5,.7,.85])
 element('pause',1,'',[0,0],[360,220],anchorMin=[.5,.5],anchorMax=[.5,.5],align=[.5,.5],visible=False,flow=2,padding=[15,15,15,15])
 element('resume',4,'RESUME',[0,0],[320,60],parent='pause',background=[.15,.3,.4,1]);element('reload',4,'RELOAD AUTHORED WORLD',[0,0],[320,60],parent='pause',background=[.15,.3,.4,1]);patch('ui',path,d);track(path,ids['ui'])
 scene=project/'Scenes/workshop.judas';run('--create','scene',scene,'--overwrite');d=json.loads(scene.read_text())['data'];d['settings']['name']='M67 World Workshop'
 def obj(id,components,name,position,fields=None):
  o=template(components,name);o['id']=id;o['fields']['position']=position
  if fields:o['fields'].update(fields)
  d['objects'].append(o);return o
 obj(1,'render,body','Ground',[0,-.5,-60],{'render.half-extents':[30,.5,90],'body.half-extents':[30,.5,90],'render.color':[.11,.15,.2]})
 obj(2,'gravity','Uniform field',[0,0,-60],{'gravity':{'kind':{'symbol':'uniform'},'magnitude':9.81},'gravity.region':{'shape':{'symbol':'box'},'halfExtents':[100,200,2000]}})
 obj(3,'motor,render','Script controlled traveller',[0,1,5],{'render.half-extents':[.25,.75,.25],'render.color':[.8,.5,.12],'scripts':1,'script.0.id':1,'script.0.asset':ids['script'],'script.0.enabled':True,'script.0.properties':{'json':{'prefab':ids['prefab'],'rig':{'entity':'6'}}}})
 obj(4,'empty','Authored runtime UI',[0,0,0],{'ui.asset':ids['ui'],'ui.name':'workshop','ui.enabled':True})
 obj(5,'render','Far marker',[0,80,-800],{'render.half-extents':[30,30,10],'render.color':[.05,.8,.7]})
 # Source art is original commodity glTF; M66 performs the actual import.
 subprocess.run(['python3',str(ROOT/'tools/m67_fixture.py'),str(project/'Sources/figure.gltf')],check=True,cwd=ROOT)
 existing=[x for x in (project/'Imports').glob('*.judasimport') if json.loads(x.read_text()).get('output')=='Assets/models/figure.judasmodel']
 recipe=str(existing[0]) if existing else run('--create-import',project,project/'Sources/figure.gltf','Assets/models/figure.judasmodel').splitlines()[-1];run('--import-model',recipe)
 mesh_id=re.search(r'id "([^"]+)"',(project/'Assets/models/figure.judasmodel.judasmeta').read_text()).group(1)
 obj(6,'render,animation','Multipart articulated fixture',[4,0,1],{'render':{'symbol':'mesh'},'render.mesh-asset':mesh_id,'animation.clip':'Sway','animation.limbs':1,'animation.limb.0.id':'branch-a','animation.limb.0.root':'Fork','animation.limb.0.middle':'Branch-A','animation.limb.0.end':'Tip-A','animation.limb.0.target':[1.9,2.1,0],'animation.limb.0.pole':[0,2,-1],'animation.limb.0.weight':.35,'animation.limb.0.enabled':True,'animation.limb.0.order':10})
 obj(10,'render','Socket attachment',[0,0,0],{'render.half-extents':[.14,.14,.14],'render.color':[.95,.2,.6],'socket.target':6,'socket.joint':'Tip-A','socket.enabled':True,'socket.position':[0,0,0],'socket.rotation':[1,0,0,0],'socket.scale':[1,1,1]})
 obj(7,'render','Array template',[1000,0,0],{'render.half-extents':[.15,1,.15],'render.color':[.12,.65,.75]})
 obj(8,'render,body','Step template',[1000,0,1],{'render.half-extents':[2,.12,.45],'body.half-extents':[2,.12,.45],'render.color':[.85,.35,.14]})
 obj(9,'camera','Far-view secondary camera',[0,80,0],{'camera.far':2000,'camera.width':256,'camera.height':144})
 d['settings']['next-id']=100;patch('scene',scene,d)
 # Generic fit chooses local frames/anchors through the SAME editor service.
 run('--fit-skeleton',scene,config,6,'Spindle,Fork,Branch-A,Tip-A,Branch-B,Tip-B',.18,scene,'--overwrite')
 def recipe(name,type,**values):
  path=authoring/(name+'.judasrecipe');run('--create','recipe',path,'--overwrite');r=json.loads(path.read_text())['data'];r.update(id=name,type=type,**values);patch('recipe',path,r);run('--generate',path,scene,config,scene,'--overwrite')
 for row in range(10):recipe('gallery-'+str(row),'linear',templateId=7,count=32,spacing=1.5,frame={'position':[-23+row*5,1,-12],'rotation':[.70710678,0,.70710678,0],'scale':[1,1,1]})
 recipe('steps','path',templateId=8,count=9,spacing=1,path=[[0,0,0],[0,2,-8]],frame={'position':[-7,.1,-4],'rotation':[1,0,0,0],'scale':[1,1,1]})
 recipe('quarter','arc',radius=5,segments=48,collisionSegments=16,endDegrees=90,profile=[[-3,-.15],[3,-.15],[3,.15],[-3,.15]],frame={'position':[8,.15,-8],'rotation':[1,0,0,0],'scale':[1,1,1]})
 recipe('ledge','sweep',path=[[0,0,0],[0,0,-16]],segments=4,profile=[[-1,-.15],[1,-.15],[1,.15],[-1,.15]],frame={'position':[-12,1,-10],'rotation':[1,0,0,0],'scale':[1,1,1]})
 recipe('corridor-floor','sweep',path=[[0,0,0],[0,0,-8],[3,0,-16]],smooth=True,segments=12,collisionSegments=6,profile=[[-2,-.1],[2,-.1],[2,.1],[-2,.1]],frame={'position':[0,.1,-22],'rotation':[1,0,0,0],'scale':[1,1,1]})
 recipe('corridor-left','sweep',path=[[0,0,0],[0,0,-8],[3,0,-16]],smooth=True,segments=12,collisionSegments=6,profile=[[-2.15,-.1],[-1.95,-.1],[-1.95,2.7],[-2.15,2.7]],frame={'position':[0,.1,-22],'rotation':[1,0,0,0],'scale':[1,1,1]})
 recipe('corridor-right','sweep',path=[[0,0,0],[0,0,-8],[3,0,-16]],smooth=True,segments=12,collisionSegments=6,profile=[[1.95,-.1],[2.15,-.1],[2.15,2.7],[1.95,2.7]],frame={'position':[0,.1,-22],'rotation':[1,0,0,0],'scale':[1,1,1]})
 recipe('path-rails','path',templateId=7,count=21,spacing=1.2,path=[[0,0,0],[0,0,-12],[4,1,-22]],smooth=True,twistDegrees=25,frame={'position':[16,1,-18],'rotation':[1,0,0,0],'scale':[1,1,1]})
 # Stable authored prefab source IDs, initialized only through public spawn API.
 source=template('render,body','Debris');source['fields'].update(body={'motion':{'symbol':'dynamic'},'shape':{'symbol':'box'}},**{'body.mass':2,'scripts':1,'script.0.id':1,'script.0.asset':ids['debrisScript'],'script.0.enabled':True,'script.0.properties':{'json':{}}})
 path=project/'Assets/prefabs/debris.judasprefab';run('--create','prefab',path,'--overwrite');doc=json.loads(path.read_text())['data'];doc['objects']=[source];doc['settings']['next-id']=2;patch('prefab',path,doc);track(path,ids['prefab'])
 for name,color in [('east',[.85,.35,.1]),('west',[.2,.65,.25])]:
  path=project/'Scenes'/(name+'.judas');run('--object-template','render,body',name,path,'--overwrite');doc=json.loads(path.read_text())['data'];o=doc['objects'][0];o['fields'].update({'render.half-extents':[3,.3,3],'body.half-extents':[3,.3,3],'render.color':color,'position':[0,.3,0]});traveller=copy.deepcopy(source);traveller['id']=2;traveller['name']='Region traveller';traveller['fields'].update({'position':[1,2,0],'script.0.properties':{'json':{'label':name+' traveller'}}});doc['objects'].append(traveller);doc['settings']['next-id']=3;patch('scene',path,doc)
 path=project/'Assets/world/workshop.judasworld';run('--create','world',path,'--overwrite');doc=json.loads(path.read_text())['data'];doc['regions']=[{'id':n,'scene':'Scenes/'+n+'.judas','origin':[0,0,z],'rotation':[0,0,0,1],'halfExtents':[12,6,12],'priority':1,'policy':'snapshot','estimatedBytes':65536,'dependencies':[]} for n,z in [('east',-45),('west',-95)]];patch('world',path,doc);track(path,ids['world'])
 p=json.loads(config.read_text())['data'];p['worldManifest']=ids['world'];patch('project',config,p)
 print('WORKSHOP',len(json.loads(scene.read_text())['data']['objects']),'ordinary objects')
if __name__=='__main__':main()
