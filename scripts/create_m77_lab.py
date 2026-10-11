#!/usr/bin/env python3
"""Original CC0 weighted-foot robots; normal named authoring and glTF import.

These are explicitly structural mechanical fixtures, not a humanoid-recovery
claim. Geometry coincides with the authored collider boxes. Existing edited
projects are never overwritten without the explicit --overwrite option.
"""
import argparse, base64, copy, hashlib, json, math, re, struct, subprocess
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
PROFILES={
 'anchor':dict(names=['Chassis','PortLink','PortRod','PortShoe','StarboardLink','StarboardRod','StarboardShoe'],
  positions=[[0,1.4,0],[-.4,1.05,0],[-.4,.65,0],[-.4,.3,.08],[.4,1.05,0],[.4,.65,0],[.4,.3,.08]],
  sizes=[[.25,.25,.15],[.09,.2,.09],[.07,.2,.07],[.3,.3,.3],[.09,.2,.09],[.07,.2,.07],[.3,.3,.3]],
  masses=[4,1,1,10,1,1,10],anchors=[0,.2,.2,.15,.2,.2,.15],patch=.44),
 'receiver':dict(names=['Housing','Aster','Birch','Copper','Dune','Elm','Flint'],
  positions=[[0,1.35,0],[-.34,1,0],[-.34,.62,0],[-.34,.28,.06],[.34,1,0],[.34,.62,0],[.34,.28,.06]],
  sizes=[[.28,.24,.18],[.1,.18,.1],[.08,.2,.08],[.28,.28,.33],[.1,.18,.1],[.08,.2,.08],[.28,.28,.33]],
  masses=[3.5,1.5,.75,12,1.5,.75,9],anchors=[0,.18,.2,.14,.18,.2,.14],patch=.52),
 'stride':dict(names=['Tower','LeftBeam','LeftStrut','LeftPad','RightBeam','RightStrut','RightPad'],
  positions=[[0,1.42,0],[-.4,.87,0],[-.4,.47,0],[-.4,.12,.08],[.4,.87,0],[.4,.47,0],[.4,.12,.08]],
  sizes=[[.25,.4,.15],[.09,.2,.09],[.07,.2,.07],[.35,.12,.38],[.09,.2,.09],[.07,.2,.07],[.35,.12,.38]],
  masses=[10,3,2,2,3,2,2],anchors=[0,.2,.2,.15,.2,.2,.15],patch=.52)
}
PARENTS=[-1,0,1,2,0,4,5]

def source(path,p):
 blob=bytearray();views=[];accessors=[]
 def accessor(rows,kind,component=5126):
  while len(blob)%4:blob.append(0)
  start=len(blob);flat=[x for row in rows for x in (row if isinstance(row,list) else [row])]
  blob.extend(struct.pack('<'+('f' if component==5126 else 'H')*len(flat),*flat))
  views.append(dict(buffer=0,byteOffset=start,byteLength=len(blob)-start))
  accessors.append(dict(bufferView=len(views)-1,componentType=component,count=len(rows),type=kind));return len(accessors)-1
 nodes=[]
 for n,pos in enumerate(p['positions']):
  parent=PARENTS[n];local=[pos[k]-(p['positions'][parent][k] if parent>=0 else 0) for k in range(3)]
  nodes.append(dict(name=p['names'][n],translation=local))
  if parent>=0:nodes[parent].setdefault('children',[]).append(n)
 positions=[];normals=[];uv=[];joints=[];weights=[];indices=[]
 for bone,(center,half) in enumerate(zip(p['positions'],p['sizes'])):
  for axis,sign in [(0,1),(0,-1),(1,1),(1,-1),(2,1),(2,-1)]:
   normal=[0,0,0];normal[axis]=sign;other=[k for k in range(3) if k!=axis]
   if (axis==1)==(sign==1):other.reverse()
   start=len(positions)
   for a,b in [(-1,-1),(1,-1),(1,1),(-1,1)]:
    v=center.copy();v[axis]+=half[axis]*sign;v[other[0]]+=half[other[0]]*a;v[other[1]]+=half[other[1]]*b
    positions.append(v);normals.append(normal);uv.append([(a+1)/2,(b+1)/2]);joints.append([bone,0,0,0]);weights.append([1,0,0,0])
   indices.extend([start,start+1,start+2,start,start+2,start+3])
 binds=[[1,0,0,0,0,1,0,0,0,0,1,0,-v[0],-v[1],-v[2],1] for v in p['positions']]
 attributes={k:accessor(v,kind,component) for k,v,kind,component in [('POSITION',positions,'VEC3',5126),('NORMAL',normals,'VEC3',5126),('TEXCOORD_0',uv,'VEC2',5126),('JOINTS_0',joints,'VEC4',5123),('WEIGHTS_0',weights,'VEC4',5126)]}
 primitive=dict(attributes=attributes,indices=accessor(indices,'SCALAR',5123),material=0)
 skin=dict(joints=list(range(7)),skeleton=0,inverseBindMatrices=accessor(binds,'MAT4'))
 times=accessor([0,1],'SCALAR');rotations=accessor([[0,0,0,1],[0,0,0,1]],'VEC4')
 nodes.append(dict(name='Exact collider-box presentation',mesh=0,skin=0))
 data=dict(asset=dict(version='2.0',generator='Original CC0 M77 mechanical fixture'),scene=0,scenes=[dict(nodes=[0,7])],nodes=nodes,skins=[skin],meshes=[dict(primitives=[primitive])],materials=[dict(name='Painted fixture',pbrMetallicRoughness=dict(baseColorFactor=[.55,.75,.8,1],metallicFactor=0,roughnessFactor=.8))],animations=[dict(name='Rest',samplers=[dict(input=times,output=rotations,interpolation='LINEAR')],channels=[dict(sampler=0,target=dict(node=0,path='rotation'))])],buffers=[dict(byteLength=len(blob),uri='data:application/octet-stream;base64,'+base64.b64encode(blob).decode())],bufferViews=views,accessors=accessors)
 path.write_text(json.dumps(data,separators=(',',':'))+'\n')

def balance(p):
 chains=[]
 for upper,lower,foot in [(1,2,3),(4,5,6)]:
  half=p['sizes'][foot];patches=[]
  for axis in range(3):
   for sign in [-1,1]:
    normal=[0,0,0];normal[axis]=sign;patches.append(dict(offset=[normal[k]*half[k] for k in range(3)],normal=normal,radius=p['patch']))
  chains.append(dict(upper=p['names'][upper],lower=p['names'][lower],foot=p['names'][foot],contactOffset=[0,-half[1],0],halfExtents=half,patches=patches))
 return dict(enabled=True,visualize=True,standingHeight=p['positions'][0][1],touchdownTolerance=.2 if p is PROFILES['stride'] else .12,chains=chains)

def main():
 parser=argparse.ArgumentParser();parser.add_argument('--tool',default='build/judas_scene_author');parser.add_argument('--project',default='projects/balance_lab');parser.add_argument('--sources-only',action='store_true');parser.add_argument('--overwrite',action='store_true');args=parser.parse_args()
 project=ROOT/args.project;config=project/'balance_lab.judasproj';tool=ROOT/args.tool
 if config.exists() and not args.overwrite:raise SystemExit('Refusing to replace an edited lab without --overwrite')
 for folder in ['Sources','Authoring','Imports','Scenes','Assets/models','Assets/scripts','Assets/fonts','Assets/ui','Assets/prefabs']:(project/folder).mkdir(parents=True,exist_ok=True)
 for name,p in PROFILES.items():source(project/'Sources'/f'{name}.gltf',p)
 license='Original M77 mechanical fixtures and lab content: CC0-1.0. https://creativecommons.org/publicdomain/zero/1.0/\nWeighted feet are explicit physical geometry/mass, not hidden assistance.\n'
 (project/'Sources/LICENSE.txt').write_text(license);(project/'Assets/LICENSE.txt').write_text(license)
 (project/'Authoring/profiles.json').write_text(json.dumps(PROFILES,indent=2)+'\n')
 if args.sources_only:return
 def run(*cmd):
  r=subprocess.run([str(tool),*[str(x) for x in cmd]],cwd=ROOT,text=True,capture_output=True)
  if r.returncode:raise RuntimeError(' '.join(map(str,cmd))+'\n'+r.stdout+r.stderr)
  return r.stdout.strip()
 def patch(kind,path,data):
  edits=project/'Authoring/edit.json';edits.write_text(json.dumps([dict(op='replace',path='/data',value=data)],ensure_ascii=False));run('--patch',kind,path,path,edits,'--overwrite')
 def template(components):
  path=project/'Authoring'/('template-'+components.replace(',','-')+'.json');run('--object-template',components,'Lab object',path,'--overwrite');return json.loads(path.read_text())['data']['objects'][0]
 def track(path):
  metadata=Path(str(path)+'.judasmeta')
  if metadata.exists():return re.search(r'id "([^"]+)"',metadata.read_text()).group(1)
  identity=hashlib.md5(('M77 Balance Lab:'+str(path.relative_to(project))).encode()).hexdigest();run('--track-asset',config,path,identity);return identity
 run('--create','project',config,'--overwrite');data=json.loads(config.read_text())['data'];data.update(name='Balance Lab',legacyGameplay=False,startupScene='Scenes/standing.judas',exportScenes=['Scenes/'+n+'.judas' for n in ['standing','supports','recovery','radial']],saveIdentity=hashlib.sha256(b'Judas M77 physical balance lab').hexdigest())
 data['input']=[v for v in data['input'] if v['name'] in {'move_x','move_y','look_x','look_y','pause','ui_up','ui_down','ui_left','ui_right','ui_activate','ui_click'}]
 for name,key in [('standing','F1'),('supports','F2'),('recovery','F3'),('radial','F4'),('diagnostics','F5'),('save','F6'),('load','F7'),('select','Tab'),('stand','B'),('recover','G'),('impact','J'),('strong','K'),('weak','U'),('partial','O'),('layer','L'),('stepping','T'),('effort','Y'),('cancel','P'),('return','N'),('spawn','Z'),('remove','Delete'),('reset','R'),('push_rotate','Q'),('push_reverse','E')]:
  data['input']=[v for v in data['input'] if v['name']!=name];data['input'].append(dict(name=name,axis=False,bindings=[dict(control='key:'+key,scale=1,deadzone=0)]))
 patch('project',config,data)
 models={}
 for name in PROFILES:
  destination='Assets/models/'+name+'.judasmodel';existing=[p for p in (project/'Imports').glob('*.judasimport') if json.loads(p.read_text()).get('output')==destination]
  recipe=str(existing[0]) if existing else run('--create-import',project,project/'Sources'/f'{name}.gltf',destination).splitlines()[-1];run('--import-model',recipe);models[name]=track(project/'Assets/models'/f'{name}.judasmodel')
 # Scripts are authored directly as ordinary source files, not native dispatch.
 script=project/'Assets/scripts/lab.js'
 if not script.exists():script.write_bytes((ROOT/'projects/balance_lab/Assets/scripts/lab.js').read_bytes())
 script_id=track(script)
 font=project/'Assets/fonts/DejaVuSans.ttf';font.write_bytes((ROOT/'assets/fonts/DejaVuSans.ttf').read_bytes());(font.parent/'LICENSE.txt').write_bytes((ROOT/'assets/fonts/DejaVuSans-LICENSE.txt').read_bytes());font_id=track(font)
 ui=project/'Assets/ui/lab.judasui';run('--create','ui',ui,'--overwrite');data=json.loads(ui.read_text())['data'];base=data['elements'][0];base.update(size=[0,0],anchorMax=[1,1]);data['elements']=[base]
 def element(identity,kind,text,offset,size,parent='canvas',**extra):
  e=copy.deepcopy(base);e.update(id=identity,parent=parent,kind=kind,text=text,offset=offset,size=size,anchorMax=[0,0],font=font_id,fontSize=18,background=[0,0,0,0]);e.update(extra);data['elements'].append(e)
 element('title',2,'BALANCE LAB / M77',[20,15],[1100,32],fontSize=24)
 element('help',2,'F1 standing | F2 supports | F3 fallen | F4 radial | Tab select | B stand | G recover\nU weak | J push | K severe push | Q rotate / E reverse push | T steps | Y effort | P release | N motor | O partial | L layer\nF5 colliders/COM/contact/plan | Z spawn | Delete support | F6/F7 save/load | R reload | Esc pause',[20,54],[1240,88],wrap=True)
 element('status',2,'Loading ordinary physical fixtures…',[20,154],[1180,170],wrap=True)
 element('pause',1,'',[0,0],[360,180],anchorMin=[.5,.5],anchorMax=[.5,.5],align=[.5,.5],visible=False,padding=[15,15,15,15],flow=2,background=[.04,.09,.12,.96]);element('resume',4,'RESUME',[0,0],[330,55],parent='pause',background=[.14,.32,.42,1]);element('reload',4,'RELOAD LAB',[0,0],[330,55],parent='pause',background=[.14,.32,.42,1]);patch('ui',ui,data);ui_id=track(ui)
 def actor(identity,name,position,fallen=0):
  p=PROFILES[name];o=template('render,animation,motor');o.update(id=identity,name=name.capitalize()+' / original mechanical rig');f=o['fields'];f.update(position=position,**{'render':{'symbol':'mesh'},'render.mesh-asset':models[name],'render.color':[1,1,1],'animation.clip':'Rest','animation.play-on-start':False,'motor.offset':[0,.9,0],'motor.radius':.23,'motor.halfHeight':.65,'motor.enabled':not fallen,'ragdoll.enabled':True,'ragdoll.play-on-start':False,'ragdoll.self-collision':True,'ragdoll.bones':7,'ragdoll.balance':{'json':balance(p)}})
  for i in range(7):
   parent=PARENTS[i];anchor=[0,p['anchors'][i],0];point=[p['positions'][i][k]+anchor[k] for k in range(3)];prefix='ragdoll.bone.'+str(i)+'.'
   fields=dict(joint=p['names'][i],parent=p['names'][parent] if parent>=0 else '',shape=0,offset=[0,0,0],orientation=[1,0,0,0],**{'half-extents':p['sizes'][i],'radius':.2,'mass':p['masses'][i],'friction':.8,'restitution':0,'layer':0,'mask':18446744073709551615,'suppress-parent':True,'auto-anchors':False,'constraint':1 if i in [2,5] else 2,'enabled':True,'anchor-a':anchor,'anchor-b':[point[k]-p['positions'][parent][k] for k in range(3)] if parent>=0 else [0,0,0],'frame-a':[1,0,0,0],'frame-b':[1,0,0,0],'limits':True,'lower':-2.7,'upper':2.7})
   f.update({prefix+k:v for k,v in fields.items()})
  if fallen:
   # Initial fixture arrangement only; no runtime repair after activation.
   sine,cosine=math.sin(fallen),math.cos(fallen);minimum=1e9
   for centre,half in zip(p['positions'],p['sizes']):minimum=min(minimum,cosine*centre[1]-sine*centre[2]-(abs(cosine)*half[1]+abs(sine)*half[2]))
   f['position']=[position[0],-minimum+.02,position[2]];f['rotation']=[math.cos(fallen/2),math.sin(fallen/2),0,0]
  return o
 scenes={}
 for label in ['standing','supports','recovery','radial']:
  path=project/'Scenes'/f'{label}.judas';run('--create','scene',path,'--overwrite');data=json.loads(path.read_text())['data'];data['settings']['name']='M77 / '+label;data['settings']['next-id']=1000
  def obj(identity,components,name,position,fields):
   o=template(components);o.update(id=identity,name=name);o['fields'].update(position=position,**fields);data['objects'].append(o);return o
  if label=='radial':
   obj(1,'render,body','Ordinary curved planet',[0,0,0],{'render':{'symbol':'sphere'},'render.radius':100,'body':{'motion':{'symbol':'static'},'shape':{'symbol':'sphere'}},'body.radius':100,'render.color':[.12,.2,.23],'body.friction':.8});obj(2,'gravity','RadicalGravity',[0,0,0],{'gravity':{'kind':{'symbol':'radial'},'magnitude':9.81},'gravity.region':{'shape':{'symbol':'sphere'},'radius':200}})
  else:
   obj(1,'render,body','Ordinary finite floor',[0,-.25,0],{'render.half-extents':[12,.25,12],'body.half-extents':[12,.25,12],'render.color':[.12,.18,.22],'body.friction':.8});obj(2,'gravity','FaithfulGravity',[0,0,0],{'gravity':{'kind':{'symbol':'uniform'},'magnitude':9.81},'gravity.region':{'shape':{'symbol':'box'},'halfExtents':[100,100,100]}})
  for identity,name,x,fallen in [(10,'anchor',-3,math.pi/2 if label=='recovery' else 0),(11,'receiver',0,-math.pi/2 if label=='recovery' else 0),(12,'stride',3,0),(13,'anchor',6,0)]:
   if label=='recovery' and identity==13:fallen=math.pi/2
   o=actor(identity,name,[x,100 if label=='radial' else 0,0],fallen);data['objects'].append(o)
   if label=='radial':o['fields']['ragdoll.balance']['json']['plantedOrientationWeight']=1
   if label=='supports':o['fields']['position'][1]=.5;o['fields']['motor.enabled']=False
   if label=='standing' and identity==13:
    o['name']='Anchor / authored frictionless negative control'
    for i in range(7):o['fields'][f'ragdoll.bone.{i}.friction']=0
  if label=='supports':
   for identity,x in [(20,-3),(21,0),(22,3),(23,6)]:obj(identity,'render,body','Explicit moving support',[x,.25,0],{'body':{'motion':{'symbol':'kinematic'},'shape':{'symbol':'box'}},'body.half-extents':[1.4,.25,1.4],'render.half-extents':[1.4,.25,1.4],'render.color':[.24,.4,.43],'body.friction':.8})
  if label=='recovery':
   blocked=next(o for o in data['objects'] if o['id']==13);blocked['fields']['motor.enabled']=False;blocked['fields']['position'][1]=.42;blocked['name']='Anchor / blocked get-up'
   obj(30,'render,body','Ordinary low ceiling / blocked recovery',[6,.82,0],{'body.half-extents':[1.8,.1,12],'render.half-extents':[1.8,.1,12],'render.color':[.48,.25,.16]})
  if label=='supports':
   by={o['id']:o for o in data['objects']};by[21]['fields']['rotation']=[math.cos(math.radians(3)/2),0,0,math.sin(math.radians(3)/2)];by[21]['fields']['body']['motion']['symbol']='static';by[21]['name']='Ordinary 3 degree slope'
   obj(30,'render,body','Ordinary 2 cm uneven step',[-3.4,.51,0],{'body.half-extents':[.32,.01,.8],'render.half-extents':[.32,.01,.8]})
   q=[math.cos(.9/2)]+[math.sin(.9/2)*v/math.sqrt(14) for v in [1,2,3]]
   by[13]['fields'].update(position=[6,5,0],rotation=q);by[23]['fields'].update(position=[6,5,0],rotation=q);by[23]['fields']['body']['motion']['symbol']='static';by[23]['fields']['body.half-extents']=[1.4,.02,1.4];by[23]['fields']['render.half-extents']=[1.4,.02,1.4]
   zone=obj(31,'gravity','Oblique uniform gravity',[6,5,0],{'rotation':q,'gravity':{'kind':{'symbol':'uniform'},'magnitude':9.81},'gravity.region':{'shape':{'symbol':'sphere'},'radius':3}});data['objects'].remove(zone);data['objects'].insert(1,zone)
  obj(80,'render','Resolved skeleton socket marker',[0,0,0],{'parent':12,'render.half-extents':[.05,.05,.05],'render.color':[1,.75,.1],'socket.target':12,'socket.joint':'LeftStrut','socket.enabled':True,'socket.position':[.15,0,0],'socket.rotation':[1,0,0,0],'socket.scale':[1,1,1]})
  obj(90,'empty','Project-owned human controls',[0,0,0],{'scripts':1,'script.0.id':1,'script.0.asset':script_id,'script.0.enabled':True,'script.0.properties':{'json':{'mode':label,'actorA':{'entity':'10'},'actorB':{'entity':'11'},'actorC':{'entity':'12'},'actorD':{'entity':'13'},'namesJson':json.dumps({n:p['names'] for n,p in PROFILES.items()},separators=(',',':'))}},'ui.asset':ui_id,'ui.name':'balance_lab','ui.enabled':True})
  patch('scene',path,data);scenes[label]=data
 prefab=project/'Assets/prefabs/anchor.judasprefab';run('--create','prefab',prefab,'--overwrite');data=json.loads(prefab.read_text())['data'];data['objects']=[actor(1,'anchor',[0,0,0])];data['settings']['next-id']=2;patch('prefab',prefab,data);prefab_id=track(prefab)
 for label,data in scenes.items():
  control=next(o for o in data['objects'] if o['id']==90);control['fields']['script.0.properties']['json']['prefab']=prefab_id;patch('scene',project/'Scenes'/f'{label}.judas',data)
 (project/'Authoring/edit.json').unlink(missing_ok=True)
 print(config)

if __name__=='__main__':main()
