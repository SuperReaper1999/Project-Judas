#!/usr/bin/env python3
"""Ordinary M84 authored volume lab. Uses the shared CLI cook, public JS and normal scenes."""
import pathlib,json,shutil,subprocess,hashlib,math,argparse
ROOT=pathlib.Path(__file__).resolve().parents[1]
a=argparse.ArgumentParser();a.add_argument('--output',type=pathlib.Path,default=ROOT/'projects/volume_terrain_lab');a.add_argument('--baker',type=pathlib.Path,default=ROOT/'build/judas_volume_author');args=a.parse_args();out=args.output.resolve();base=ROOT/'projects/terrain_lab';out.mkdir(parents=True,exist_ok=True)
def write(p,data):p.parent.mkdir(parents=True,exist_ok=True);p.write_text(json.dumps(data,indent=2)+'\n')
def identity(name):return hashlib.sha256(('M84/'+name).encode()).hexdigest()[:32]
for folder in ['fonts','ui','scripts']:
 shutil.copytree(base/'Assets'/folder,out/'Assets'/folder,dirs_exist_ok=True)
(out/'Assets/prefabs').mkdir(parents=True,exist_ok=True)
for p in (base/'Assets/prefabs').glob('drop.*'):shutil.copy2(p,out/'Assets/prefabs'/p.name)
project=json.loads((base/'terrain_lab.judasproj').read_text());p=project['data'];p.update(name='Volumetric Terrain Lab',startupScene='Scenes/cave.judas',saveIdentity=hashlib.sha256(b'M84-volume-lab-save').hexdigest(),worldManifest=identity('world'));p['runtimeAssets']=['ca7848d55e08f6bb5a042f38dbc8a311'];p['exportScenes']=[];p['excludeScenes']=[]
p['input'].append({'name':'region','axis':False,'bindings':[{'control':'key:F3','scale':1,'deadzone':0}]})
projectPath=out/'volume_terrain_lab.judasproj';write(projectPath,project)
source=out/'Imports/cave.judasvolume';source.parent.mkdir(parents=True,exist_ok=True)
# One shared bounded source: a rocky solid, a through-tunnel, a larger chamber,
# and a genuinely sealed pocket. Fields are physical geometry, not water/game state.
if not source.exists():
 subprocess.run([str(args.baker),'create',str(source),'41','25','41','20','12','20'],check=True)
t=json.loads(source.read_text());t.update(identity=identity('solid'),meshId=identity('mesh'),collisionId=identity('collision'),productDirectory='Assets/Volumes/cave')
nx,ny,nz=t['resolution'];extent=t['extent'];cell=[extent[k]/(t['resolution'][k]-1) for k in range(3)]
def box(q,half):v=[abs(q[k])-half[k] for k in range(3)];return math.sqrt(sum(max(0,x)**2 for x in v))+min(max(v),0)
t['density']=[]
for z in range(nz):
 for y in range(ny):
  for x in range(nx):
   q=[x*cell[0]-10,y*cell[1]-6,z*cell[2]-10];solid=box(q,[extent[k]/2-cell[k]*1.25 for k in range(3)])
   # Analytic capsule crosses both front/back faces; no scripted excavation.
   tunnel=math.hypot(q[0],q[1]+2.5)-2.2
   chamber=math.sqrt(q[0]**2+(q[1]+1.9)**2+q[2]**2)-2.8
   sealed=math.sqrt((q[0]+4.4)**2+(q[1]-1.3)**2+q[2]**2)-1.35
   value=max(solid,-tunnel,-chamber,-sealed)
   if x in [0,nx-1] or y in [0,ny-1] or z in [0,nz-1]:value=max(value,.1)
   t['density'].append(value)
source.write_text(json.dumps(t,separators=(',',':'))+'\n')
subprocess.run([str(args.baker),'cook',str(projectPath),'Imports/cave.judasvolume'],check=True)
scene=json.loads((base/'Scenes/landscape.judas').read_text());s=scene['data'];s['settings']['name']='Dig Lab - caves, tunnel and enclosed pocket';objects=[o for o in s['objects'] if o['id']!=3]
for o in objects:
 f=o['fields']
 if o['id']==1:o['name']='Editable volumetric rock / select then View > Volumetric terrain';f.update({'position':[0,5,0],'render.mesh-asset':t['meshId'],'body.collision-asset':t['collisionId'],'render.texture-asset':'','render.color':[.52,.43,.32]})
 if o['id']==2:f['gravity.region']['halfExtents']=[16,12,16]
 if o['id']==10:f['position']=[0,1.4,13]
 if 20<=o['id']<=23:f['position']=[-4 if o['id']<22 else 4,1,12+(o['id']%2)*2]
 if o['id']==50:f.update({'nav.surface.asset':'','nav.surface.halfExtents':'24 12 24'});o['id']=84
# Conventional approach apron, separate real geometry; it does not cover the tunnel floor.
fields=dict(next(o['fields'] for o in objects if o['id']==20));fields.update({'position':[0,-.3,0],'rotation':[1,0,0,0],'scale':[1,1,1],'render':{'symbol':'box'},'render.half-extents':[15,.3,15],'render.color':[.19,.25,.22],'body':{'motion':{'symbol':'static'},'shape':{'symbol':'box'}},'body.half-extents':[15,.3,15],'body.friction':.8})
objects.append({'id':70,'name':'Approach apron / ordinary rigid geometry','fields':fields})
for i,z in enumerate([5,0,-5]):
 objects.append({'id':100+i,'name':'Ordinary cave light '+str(i),'fields':{'position':[0,3.4,z],'rotation':[1,0,0,0],'scale':[1,1,1],'light':{'symbol':'point'},'light.color':[1,.82,.6],'light.range':8,'light.cone':{'innerDegrees':15,'outerDegrees':25}}})
s['objects']=objects;write(out/'Scenes/cave.judas',scene)
# Quaternion transport of the exact same geometry, gravity, motor and navigation.
angle=.8;axis=[1/math.sqrt(14),2/math.sqrt(14),3/math.sqrt(14)];q=[math.cos(angle/2),*[x*math.sin(angle/2) for x in axis]]
def rotate(v):w,x,y,z=q;u=[x,y,z];cross=lambda a,b:[a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0]];c=cross(u,v);cc=cross(u,c);return [v[k]+2*(w*c[k]+cc[k]) for k in range(3)]
rotated=json.loads(json.dumps(scene));rotated['data']['settings']['name']='Dig Lab - arbitrarily rotated terrain and gravity'
for o in rotated['data']['objects']:
 f=o['fields'];f['position']=rotate(f.get('position',[0,0,0]));f['rotation']=q
 if o['id']==84:o['id']=85
write(out/'Scenes/rotated.judas',rotated)
annex=json.loads(json.dumps(scene));annex['data']['settings']['name']='Retained volume region'
annex['data']['objects']=[o for o in annex['data']['objects'] if o['id'] in [1,2,20,21,70,101]]
for o in annex['data']['objects']:
 if o['id']==2:o['fields']['gravity.region'].update(halfExtents=[16,12,16])
write(out/'Scenes/annex.judas',annex)
world=json.loads((base/'Assets/world/terrain.judasworld').read_text());world['data']['regions'][0].update(origin=[35,0,0],halfExtents=[16,12,16],estimatedBytes=2000000)
write(out/'Assets/world/terrain.judasworld',world);(out/'Assets/world/terrain.judasworld.judasmeta').write_text('JudasAssetMeta 1\nid "'+identity('world')+'"\ntype world\nsource "volume-lab-authoring"\n')
script=(out/'Assets/scripts/controller.js').read_text().replace('Scenes/landscape.judas','Scenes/cave.judas').replace('M75 terrain traveller','M84 volume traveller').replace('finite edited mesh','real volumetric cave geometry')
script=script.replace("if(input.pressed('reset'))scenes.reload();","if(input.pressed('reset'))scenes.reload();if(input.pressed('region')){if(this.region){scenes.releaseRegion(this.region);this.region=null;}else this.region=scenes.requestRegion('annex');}")
(out/'Assets/scripts/controller.js').write_text(script)
u=json.loads((out/'Assets/ui/terrain.judasui').read_text())
for element in u['data']['elements']:
 text=element.get('text','')
 if 'Terrain' in text or 'TERRAIN LAB' in text:element['text']='DIG / VOLUMETRIC TERRAIN'
 if 'WASD' in text:element['text']='WASD move | Mouse look | Space jump | E throw crate | F1/F2 flat/rotated | F3 region | F6/F7 save/load | R reload | Esc pause'
write(out/'Assets/ui/terrain.judasui',u)
print('Current lab:',projectPath)
