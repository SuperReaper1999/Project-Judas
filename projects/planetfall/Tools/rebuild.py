"""Repeatable game-only authoring, preserving external asset sources."""
from pathlib import Path
import subprocess,os,re,hashlib
P=Path(__file__).resolve().parents[1];ROOT=P.parents[1];T=P/'Tools'
external=Path(os.environ.get('PROMPT3D_DIR','/home/conner/Documents/GitHub/ClaudesNewML-AI-Thing'))
env=dict(os.environ,PYTHONPATH=str(external));python=str(external/'.venv/bin/python')
for name in ['author_world.py','batch_scenery.py','adapt_rover.py','add_landmarks.py','earth_regions.py','author_derelict.py','streaming_setup.py']:
 subprocess.run([python,str(T/name)],env=env,check=True,cwd=ROOT)
aid=hashlib.md5(b'planetfall/player/conner').hexdigest();model=P/'Assets/models/conner_player.glb'
model.with_name(model.name+'.judasmeta').write_text(f'JudasAssetMeta 1\nid "{aid}"\ntype mesh\nsource "Conner supplied rig, game copy"\n')
for path in (P/'Scenes').glob('*.judas'):
 if path.stem=='salvage-rover':continue
 def avatar(m):
  b=re.sub(r'render.mesh-asset ".*?"',f'render.mesh-asset "{aid}"',m.group())
  if b.startswith('object 3004 '):b=re.sub(r'animation.clip ".*?"','animation.clip "Seat"',b)
  return b
 path.write_text(re.sub(r'object (?:11|3004) [\s\S]*?\nend\n',avatar,path.read_text()))
subprocess.run([str(ROOT/'build/judas_navigation_bake'),str(P/'planetfall.judasproj'),'--rebuild-dependents'],check=True,cwd=ROOT)
for meta in (P/'Assets/navigation').glob('*.judasnav.judasmeta'):
 if not meta.with_name(meta.name.removesuffix('.judasmeta')).exists():meta.unlink()

# Retire only unreferenced products owned by this game's generation pass.
refs=set()
for path in (P/'Scenes').glob('*.judas'):refs.update(re.findall(r'nav.surface.asset "([^\"]+)"',path.read_text()))
for meta in (P/'Assets/navigation').glob('*.judasmeta'):
 match=re.search(r'id "([^\"]+)"',meta.read_text())
 if match and match.group(1) not in refs:
  product=meta.with_name(meta.name.removesuffix('.judasmeta'))
  if product.exists():product.unlink()
  meta.unlink()
