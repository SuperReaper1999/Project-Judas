from pathlib import Path
import shutil,re,json
P=Path(__file__).resolve().parents[1];ROOT=P.parents[1];A=P/'Assets'
for folder in ['deformables','materials']:
 src=ROOT/'projects/structural_lab/Assets'/folder
 shutil.copytree(src,A/folder,dirs_exist_ok=True)
for f in (ROOT/'projects/coarse_crash_lab/Assets/deformables').glob('*'):shutil.copy2(f,A/'deformables'/f.name)
car=ROOT/'projects/coarse_crash_lab/Assets/prefabs/car.judasprefab'
shutil.copy2(car,A/'prefabs/damaged_rover.judasprefab');shutil.copy2(car.with_name(car.name+'.judasmeta'),A/'prefabs/damaged_rover.judasprefab.judasmeta')
for f in (ROOT/'projects/coarse_crash_lab/Assets/models').glob('*'):
 shutil.copy2(f,A/'models'/('crash_'+f.name))
bridge=(ROOT/'projects/structural_lab/Assets/prefabs/bridge.judasprefab').read_text();bridge=bridge[bridge.index('object '):].replace('object 1 "Eight-cell bridge"','object 39010 "Load-bearing Lantern road bridge"').replace('position 0 0 0','position -65 2.5 70',1)
# Ordinary physical approach ramps meet the authored bridge deck.
import math
angle=math.atan2(2.8,10);half=math.hypot(2.8,10)/2
ramps=''
for id,x,sign in [(39011,-74,1),(39012,-56,-1)]:
 ramps+=f'''object {id} "Bridge approach ramp"
  position {x} 1.4 70
  rotation {math.cos(angle/2)} 0 0 {sign*math.sin(angle/2)}
  scale 1 1 1
  render box
  render.half-extents {half} .12 1.6
  render.radius .5
  render.color .35 .4 .42
  render.alpha 1
  render.secondary-color .8 .8 .8
  render.secondary-alpha 1
  render.mesh-asset ""
  render.texture-asset ""
  body static box
  body.half-extents {half} .12 1.6
  body.mass 100
  body.friction .8
  body.restitution .02
  body.radius .5
  body.terrain ""
  body.initial-velocity 0 0 0
  body.pickable false
  body.managed false
  body.compound-count 0
  body.fluid-cavity-count 0
end

'''
for p in [P/'Scenes'/f'{region}.judas' for region in ['earth','moon','mars','derelict']]:p.write_text(p.read_text()+'\n'+bridge+ramps)
# Dynamic resources must be explicit export roots, not inferred from JS strings.
project=(P/'planetfall.judasproj').read_text();m=re.search(r'classification (".*")',project);c=json.loads(m.group(1)).replace('1 1 0 "Default" 1 1 0 "Default"','3 3 0 "Default" 1 "Assembly" 2 "Material" 1 1 0 "Default"');project=project[:m.start(1)]+json.dumps(c)+project[m.end(1):]
roots=[]
for meta in (A/'prefabs').glob('*.judasmeta'):roots.append(re.search(r'id "(.*?)"',meta.read_text()).group(1))
project+='runtime-assets '+json.dumps(' '.join(json.dumps(x) for x in sorted(set(roots))))+'\n';(P/'planetfall.judasproj').write_text(project)
