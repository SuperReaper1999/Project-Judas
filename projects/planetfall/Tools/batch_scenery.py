"""Bake immutable presentation only; retain every normal physics actor."""
from pathlib import Path
import re,hashlib
import numpy as np
from prompt3d.geometry import Scene,box,rot_y
from prompt3d.library import load_scene
from prompt3d.export import to_glb
P=Path(__file__).resolve().parents[1];A=P/'Assets'
meta={re.search(r'id "(.*?)"',m.read_text()).group(1):m.with_name(m.name[:-10]) for m in (A/'models').glob('*.judasmeta')}
def rotation(q):
 w,x,y,z=q
 return np.array([[1-2*y*y-2*z*z,2*x*y-2*z*w,2*x*z+2*y*w],[2*x*y+2*z*w,1-2*x*x-2*z*z,2*y*z-2*x*w],[2*x*z-2*y*w,2*y*z+2*x*w,1-2*x*x-2*y*y]])
for file in [P/'Scenes'/f'{region}.judas' for region in ['earth','moon','mars','derelict']]:
 if file.read_text().lstrip().startswith('{'):continue
 content=file.read_text();scene=Scene('fixed_environment');count=0
 def replace(match):
  global count
  b=match.group();id=int(b.split()[1]);dynamic='body dynamic' in b
  if 'deformable.' in b or 26000<=id<=26004:return b
  if '  parent ' in b or '  scripts ' in b or dynamic or id<30 or id in [24000,24001,24002] or not ('  render box' in b or '  render mesh' in b):return b
  p=np.array(list(map(float,re.search(r'  position ([^\n]+)',b).group(1).split())))
  q=list(map(float,re.search(r'  rotation ([^\n]+)',b).group(1).split()));M=rotation(q)
  if '  render box' in b:
   half=list(map(float,re.search(r'render.half-extents ([^\n]+)',b).group(1).split()));col=list(map(float,re.search(r'render.color ([^\n]+)',b).group(1).split()));color='#'+''.join(f'{round(max(0,min(1,c))*255):02x}' for c in col);name='paint_'+color[1:];scene.material(name,color);scene.add('environment',name,box(*(2*v for v in half)),M,p)
  else:
   aid=re.search(r'render.mesh-asset "(.*?)"',b).group(1);path=meta.get(aid)
   if not path or path.suffix!='.glb':return b
   s=load_scene(path)
   for _,parts in s.merged():
    for mat,prim in parts:
     material=s.materials[mat];name=mat
     scene.material(name,material.color,metallic=material.metallic,roughness=material.roughness)
     scene.add('environment',name,prim,M,p)
  count+=1
  return re.sub(r'(?m)^  render(?:\.[^ ]+)? [^\n]*\n','',b)
 content=re.sub(r'object \d+ [\s\S]*?\nend\n',replace,content)
 dest=A/'models'/f'{file.stem}_scenery.glb';to_glb(scene,dest,textures=False);id=hashlib.md5(('planetfall/scenery/'+file.stem).encode()).hexdigest();dest.with_name(dest.name+'.judasmeta').write_text(f'JudasAssetMeta 1\nid "{id}"\ntype mesh\nsource "static presentation batch"\n')
 # Reuse fully specified render schema of an unchanged ordinary visual.
 template=re.search(r'object 22100 [\s\S]*?\nend\n',file.read_text()).group();template=re.sub(r'^object .*','object 39000 "Baked immutable landscape"',template);template=re.sub(r'  position [^\n]*','  position 0 0 0',template);template=re.sub(r'render.mesh-asset "[^"]*"',f'render.mesh-asset "{id}"',template)
 file.write_text(content+'\n'+template);print(file.stem,count,'immutable visuals batched')
