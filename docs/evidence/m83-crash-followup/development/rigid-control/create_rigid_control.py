#!/usr/bin/env python3
"""Isolate M81/M82 using ordinary authored content, never fake motion.

Same driver scripts, sources, ratios, wheels, mounting coordinates and visual
geometry as the preserved two-lane M83 fixture. Remove soft/carrying DOFs and
put their approximate mass onto each ordinary root; direct wheel mounts now
act on that root. All launch velocities remain the authored zeros.
"""
import hashlib
import json
from pathlib import Path
import re
import shutil
import sys

source=Path(sys.argv[1]).resolve()
destination=Path(sys.argv[2]).resolve()
destination.mkdir(parents=True,exist_ok=True)
source_scene=source/'Scenes/crash.judas'
original=source_scene.read_text()
shutil.copytree(source/'Assets',destination/'Assets',dirs_exist_ok=True)
(destination/'Scenes').mkdir(exist_ok=True)
blocks=re.split(r'(?m)(?=^object )',original)
result=[blocks[0]]
for block in blocks[1:]:
    identity=int(re.match(r'object (\d+)',block)[1])
    root=900 if 900<=identity<1000 else 1100 if 1100<=identity<1200 else None
    lines=[]
    for line in block.splitlines():
        key=line.strip().split(' ',1)[0]
        if root and root+10<=identity<root+14 and key.startswith('body.'):
            continue
        if root and root+10<=identity<root+14 and key=='body':
            continue
        if root and root+20<=identity<root+24 and key.startswith('deformable.'):
            continue
        if identity==root and key=='body.mass':
            line='  body.mass 188'
        if identity==root and key=='powertrain.graph':
            graph=json.loads(json.loads(line.strip().split(' ',1)[1]))
            for link in graph['links']:link.pop('connection',None)
            line='  powertrain.graph '+json.dumps(json.dumps(graph,separators=(',',':')))
        if root and root+1<=identity<root+5:
            index=identity-root-1
            if key=='wheel.chassis':line=f'  wheel.chassis {root}'
            if key=='wheel.mount':
                side=-1 if index%2==0 else 1
                z=-1.35 if index<2 else 1.05
                line=f'  wheel.mount {side*1.42} -0.05 {z}'
        lines.append(line)
    result.append('\n'.join(lines)+'\n')
text=''.join(result)
(destination/'Scenes/rigid-crash.judas').write_text(text)
project=(source/'damage_lab.judasproj').read_text()
project=re.sub(r'(?m)^name .*$', 'name "M83 ordinary rigid isolation control"',project)
project=re.sub(r'(?m)^startup-scene .*$', 'startup-scene "Scenes/rigid-crash.judas"',project)
project=re.sub(r'(?m)^world-manifest .*\n?', '',project)
(destination/'rigid_control.judasproj').write_text(project)
manifest={
 'source_scene':str(source_scene),
 'source_sha256':hashlib.sha256(original.encode()).hexdigest(),
 'control_scene_sha256':hashlib.sha256(text.encode()).hexdigest(),
 'generator_sha256':hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
 'source_script_sha256':hashlib.sha256((source/'Assets/scripts/controller.js').read_bytes()).hexdigest(),
 'notes':['188 kg root replaces 120 kg chassis + 32 kg carriers + about 36 kg material','Wheel mounting points are unchanged','Same 80 Nm / 6 kW electric source and 0.25 gear ratios','No imposed initial velocity, forces, ram or scripted failure','Physics cadence and wheel quality are unchanged'],
}
(destination/'CONTROL_MANIFEST.json').write_text(json.dumps(manifest,indent=2)+'\n')
print(destination/'Scenes/rigid-crash.judas')
