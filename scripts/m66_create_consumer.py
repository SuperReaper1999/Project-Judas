#!/usr/bin/env python3
"""Local, ordinary Three Games comparison project; no original/source edits.
Copies the accepted consumer as a baseline and adds the full-rig import lab.
Old reduced-rig trick clips are retained, never retargeted by name matching.
"""
from pathlib import Path
import shutil,json,hashlib,sys,re
ROOT=Path(__file__).resolve().parents[1]
OUT=Path(sys.argv[1] if len(sys.argv)>1 else ROOT/'.cache/m66/three-games-import')
if OUT.exists():raise SystemExit('Choose an empty output directory; existing projects are preserved.')
shutil.copytree(ROOT/'projects/post_m65_consumers',OUT)
lab=ROOT/'projects/import_lab'
for directory in ['Assets','Imports','Sources']:
 shutil.copytree(lab/directory,OUT/directory,dirs_exist_ok=True)
shutil.copytree(lab/'Scenes',OUT/'Scenes/import')
p=OUT/'post_m65_consumers.judasproj';text=p.read_text();text=text.replace('name "Three Games"','name "Three Games + M66 Import"');p.write_text(text)
# Add a comparison avatar beside the accepted Skate rider. It is visual content
# following the public presentation transform, not another controller/physics rig.
# The original reduced-rig game's tricks and authored resistance are untouched.
script=OUT/'Assets/scripts/m66-comparison.js';script.write_text('''import {world} from 'judas';
export default class {
 constructor({entity}){this.entity=entity;}
 start(){this.source=world.entity('12');this.motor=world.entity('10');}
 update(){if(!this.source?.valid||!this.entity.animation?.info.ready)return;
 const pose=this.source.presentedTransform;this.entity.transform={position:{x:pose.position.x+2,y:pose.position.y,z:pose.position.z},rotation:pose.rotation};
 const speed=Math.hypot(this.motor.velocity.x,this.motor.velocity.z),name=speed>.5?'Cruise':'Push';
 if(this.entity.animation.info.clip!==name)this.entity.animation.crossFade(name,.2);
 }
}
''')
id=hashlib.md5(b'M66 full-rig comparison script').hexdigest();Path(str(script)+'.judasmeta').write_text(f'JudasAssetMeta 1\nid "{id}"\ntype script\nsource ""\n')
model=json.loads((OUT/'Imports/skate_original.judasimport').read_text())['assetId']
scene=OUT/'Scenes/skate/park.judas';s=scene.read_text();existing=[int(x) for x in re.findall(r'^object (\d+)',s,re.M)];actor=max(existing)+100
s+=f'''\nobject {actor} "M66 full original rig comparison"
 position 2 0 0
 rotation 1 0 0 0
 scale 1 1 1
 render mesh
 render.half-extents 1 1 1
 render.radius 1
 render.color 1 1 1
 render.secondary-color 1 1 1
 render.alpha 1
 render.secondary-alpha 1
 render.mesh-asset "{model}"
 render.texture-asset ""
 animation.enabled true
 animation.play-on-start true
 animation.loop true
 animation.clip "Push"
 animation.speed 1
 animation.time 0
 scripts 1
 script.0.id 1
 script.0.asset "{id}"
 script.0.enabled true
 script.0.properties "{{}}"
end
''';scene.write_text(s)
print(OUT/'post_m65_consumers.judasproj');print('Import lab scene:',OUT/'Scenes/import/main.judas')
