from pathlib import Path
import re,hashlib
from prompt3d.library import load_scene
from prompt3d.export import to_glb
P=Path(__file__).resolve().parents[1];src=P/'Assets/models/rover_body.glb';s=load_scene(src)
for mats in s.groups.values():
 for name in list(mats):
  if 'tyre' in name or 'chrome' in name:del mats[name]
out=P/'Assets/models/rover_shell.glb';to_glb(s,out,textures=False);aid=hashlib.md5(b'planetfall/rover-shell').hexdigest();out.with_name(out.name+'.judasmeta').write_text(f'JudasAssetMeta 1\nid "{aid}"\ntype mesh\nsource "Prompt3D exterior / wheels separate"\n')
for scene in [P/'Scenes'/f'{region}.judas' for region in ['earth','moon','mars','derelict']]:
 text=scene.read_text()
 def root(m):return re.sub(r'(?m)^  render(?:\.[^ ]+)? [^\n]*\n','',m.group())
 text=re.sub(r'object 25000 [\s\S]*?\nend\n',root,text)
 fields=f'''object 25060 "Pickup exterior / separate physical wheel representation"
  parent 25000
  position 0 -0.8 0
  rotation .707106781 0 .707106781 0
  scale .55 .7 .9
  render mesh
  render.half-extents .5 .5 .5
  render.radius .5
  render.color 1 1 1
  render.alpha 1
  render.secondary-color .8 .8 .8
  render.secondary-alpha 1
  render.mesh-asset "{aid}"
  render.texture-asset ""
end
'''
 scene.write_text(text+'\n'+fields)
