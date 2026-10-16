#!/usr/bin/env python3
"""Author deterministic, editable museum districts with detailed lathed meshes."""
from pathlib import Path
from PIL import Image
import json,math,hashlib,copy,shutil,sys
R=Path(__file__).resolve().parents[1];root=R/'projects/asset_lab';art=root/'SourceArt';assets=root/'Assets'
for p in [art,assets/'meshes',assets/'textures',assets/'scripts',assets/'ui',assets/'fonts',root/'Scenes']:p.mkdir(parents=True,exist_ok=True)
def identity(s):return hashlib.sha256(('Judas.M93.'+s).encode()).hexdigest()[:32]
def meta(path,kind,key):path.with_name(path.name+'.judasmeta').write_text(f'JudasAssetMeta 1\nid "{identity(key)}"\ntype {kind}\nsource "{path.relative_to(root).as_posix()}"\n')
def profile(t,n):
 if n==0:return .7+.18*math.cos(t*math.pi*8)+.05*math.sin(t*math.pi*20)
 if n==1:return .35+.65*math.sin(t*math.pi)**2+.05*math.cos(t*math.pi*24)
 if n==2:return .45+.3*(1-t)+.12*math.cos(t*math.pi*12)*math.sin(t*math.pi)
 return .65+.1*math.sin(t*math.pi*32)+.16*math.sin(t*math.pi*4)
for n in range(4):
 with (art/f'form{n}.obj').open('w') as f:
  for y in range(65):
   t=y/64;rad=profile(t,n)
   for x in range(129):
    a=x/128*math.tau;f.write(f'v {rad*math.cos(a):.8f} {t*4:.8f} {rad*math.sin(a):.8f}\n')
  for y in range(65):
   for x in range(129):f.write(f'vt {x/128:.8f} {y/64:.8f}\n')
  for y in range(65):
   t=y/64;dy=(profile(t+.0001,n)-profile(t-.0001,n))/.0008
   for x in range(129):
    a=x/128*math.tau;length=math.sqrt(1+dy*dy);f.write(f'vn {math.cos(a)/length:.8f} {-dy/length:.8f} {math.sin(a)/length:.8f}\n')
  for y in range(64):
   for x in range(128):
    a=y*129+x+1;b=a+1;c=a+129;d=c+1;f.write(f'f {a}/{a}/{a} {c}/{c}/{c} {b}/{b}/{b}\nf {b}/{b}/{b} {c}/{c}/{c} {d}/{d}/{d}\n')
 for c in range(4):
  im=Image.new('RGBA',(1024,1024));pix=im.load();palette=[(62,148,164),(158,78,52),(132,112,176),(86,144,78)];base=palette[n]
  for y in range(1024):
   for x in range(1024):
    groove=x%128<3 or y%128<3;wave=12*math.sin(x*.02+c)*math.cos(y*.025)+4*math.cos((x+y)*.19)
    line=25 if ((x+c*13)%80<3 and y%96>30) else 0
    rgb=[max(0,min(255,int(q+wave+line)))if not groove else int(q*.65)for q in base];pix[x,y]=(*rgb,255)
  im.save(art/f'tile{n}_{c}.png')
  meta(assets/f'textures/tile{n}_{c}.judastex','texture',f'tile{n}_{c}')
 meta(assets/f'meshes/form{n}.judasmesh','mesh',f'form{n}')
for file in ['DejaVuSans.ttf','LICENSE.txt']:shutil.copyfile(R/'projects/rendering_lab/Assets/fonts'/file,assets/'fonts'/file)
meta(assets/'fonts/DejaVuSans.ttf','font','font');meta(assets/'scripts/lab.js','script','script');meta(assets/'scripts/math.js','script','math');meta(assets/'ui/asset.judasui','ui','ui')
shutil.copyfile(R/'projects/rendering_lab/Assets/scripts/math.js',assets/'scripts/math.js')
project=json.loads((R/'projects/rendering_lab/rendering_lab.judasproj').read_text());p=project['data'];p.update(name='Judas / The Packed Museum',saveIdentity=hashlib.sha256(b'Judas.M93.asset-lab').hexdigest(),startupScene='Scenes/museum.judas',visualBudgetMiB=8,exportScenes=['Scenes/museum.judas','Scenes/full_detail.judas'],runtimeAssets=[identity('script'),identity('math'),identity('ui')]);p['input']=[i for i in p['input']if i['name']not in ['count_small','count_normal','count_dense','lights','shadows']]
for name,key in [('district0','1'),('district1','2'),('district2','3'),('district3','4'),('detail','F2'),('monitors','C')]:p['input'].append(dict(name=name,axis=False,bindings=[dict(control='key:'+key,scale=1.0,deadzone=0.0)]))
(root/'asset_lab.judasproj').write_text(json.dumps(project,indent=2)+'\n')
source=json.loads((R/'projects/rendering_lab/Scenes/gallery.judas').read_text());scene=copy.deepcopy(source);scene['data']['objects']=[];settings=scene['data']['settings'];settings.update(name='The Packed Museum / four asset districts',**{'world-origin':[6371000,1800000,-2200000],'sun-direction':[-.4,.85,.35],'sun-color':[.7,.72,.76],'ambient':[.18,.21,.26],'next-id':50000})
def obj(id,name,pos,fields,scale=[1,1,1],rotation=[1,0,0,0]):scene['data']['objects'].append(dict(id=id,name=name,fields=dict(position=pos,rotation=rotation,scale=scale,**fields)))
controller=copy.deepcopy(source['data']['objects'][0]);controller['name']='Asset lab controller';controller['fields']['script.0.asset']=identity('script');controller['fields']['ui.asset']=identity('ui');controller['fields']['ui.name']='asset_lab';scene['data']['objects'].append(controller)
base=copy.deepcopy(source['data']['objects'][1]['fields']);base={k:v for k,v in base.items()if k not in ['position','rotation','scale']}
centres=[(0,0),(180,0),(180,180),(0,180)]
for n,(cx,cz) in enumerate(centres):
 floor=copy.deepcopy(base);floor['render.half-extents']=[37,.3,37];floor['render.color']=[.14,.17,.21];obj(10+n,f'District {n+1} plinth',[cx,-.3,cz],floor)
 for i in range(144):
  x=cx+(i%12-5.5)*5;z=cz+(i//12-5.5)*5;render=copy.deepcopy(base);render['render']={'symbol':'mesh'};render['render.mesh-asset']=identity(f'form{n}');render['render.texture-asset']=identity(f'tile{n}_{i%4}');render['render.color']=[1,1,1];obj(100+n*200+i,f'Sculpture {n+1} / {i+1}',[x,0,z],render,[1,1+(i%3)*.1,1])
 # Ordinary static camera in a different district, deliberately slow cadence.
 camera=copy.deepcopy([x for x in source['data']['objects']if x['id']==40000][0]['fields']);camera={k:v for k,v in camera.items()if k not in ['position','rotation','scale']};camera.update(**{'camera.width':384,'camera.height':216,'camera.cadence':15,'camera.far':120,'camera.enabled':n==1});obj(40000+n,f'District {n+1} remote camera',[cx,13,cz+34],camera,rotation=[.9819564,-.1891075,0,0])
 monitor=copy.deepcopy(base);monitor['render.half-extents']=[5,2.8125,.15];monitor['render.color']=[1,1,1];monitor['render.texture-camera']=40000+(n+1)%4;obj(41000+n,f'Live district {(n+1)%4+1} screen',[cx,7,cz-35],monitor)
 # Paired entry pylons and a central walkway make the camera route legible.
 for side in [-1,1]:
  rail=copy.deepcopy(base);rail['render.half-extents']=[.2,2,4];rail['render.color']=[.8,.55,.22];obj(42000+n*2+(side+1)//2,'Entry marker',[cx+side*34,2,cz+30],rail)
(root/'Scenes/museum.judas').write_text(json.dumps(scene,indent=2)+'\n');full=copy.deepcopy(scene);full['data']['settings']['name']='The Packed Museum / full geometry control';full['data']['objects'][0]['fields']['script.0.properties']='{"fullDetail":true}'
for o in full['data']['objects']:
 if o['fields'].get('render')=={'symbol':'mesh'}:o['fields']['render.lod-multiplier']=0
(root/'Scenes/full_detail.judas').write_text(json.dumps(full,indent=2)+'\n')
ui=json.loads((R/'projects/rendering_lab/Assets/ui/rendering.judasui').read_text())
for e in ui['data']['elements']:
 if e['font']:e['font']=identity('font')
 if e['id']=='title':e['text']='JUDAS / THE PACKED MUSEUM'
 if e['id']=='help':e['text']='F5 tour / free flight | WASD + Q/E + mouse | 1–4 visit district | V split | C remote cameras\nF2 full detail / automatic LOD | F6/F7 save/load | R reload | Esc pause'
 if e['id']=='status':e['text']='Cooked texture mips / geometric error LOD / shared resource working set'
 if e['id']=='reload':e['text']='RELOAD MUSEUM'
(assets/'ui/asset.judasui').write_text(json.dumps(ui,indent=2)+'\n')
print(root)
