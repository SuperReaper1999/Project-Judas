#!/usr/bin/env python3
"""Focused final-source Linux checks. Does not build, change fixtures or certify hardware."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import socket
import subprocess
import time

ROOT = Path(__file__).resolve().parents[1]
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--output', type=Path, required=True)
a = p.parse_args()
out = a.output.resolve()
out.mkdir(parents=True, exist_ok=False)
env = {k:v for k,v in os.environ.items() if not k.startswith('JUDAS_')}
env.update(SDL_AUDIODRIVER='dummy', JUDAS_WORLD_STATE='none', XDG_DATA_HOME=str(out/'user-data'))
runs = []
def run(name, command, overrides=None):
    e = env.copy()
    e.update(overrides or {})
    start = time.monotonic()
    with (out/(name+'.log')).open('w') as log:
        result = subprocess.run(list(map(str,command)), cwd=ROOT, env=e, stdout=log,
                                stderr=subprocess.STDOUT, timeout=180)
    item = dict(name=name, argv=list(map(str,command)), exit=result.returncode,
                seconds=time.monotonic()-start)
    if Path(command[0]).is_file():
        item['binarySha256'] = hashlib.sha256(Path(command[0]).read_bytes()).hexdigest()
    runs.append(item)
    (out/'RESULTS.json').write_text(json.dumps(runs,indent=2)+'\n')
    print(name, result.returncode, flush=True)
    if result.returncode:
        raise RuntimeError(name+' failed; original log preserved')

for name in ['network_transport','logical_input','character_motor','local_multiplayer',
             'local_multiplayer_script','input','script','physics_continuation','script_presentation']:
    run(name,[ROOT/'build'/('judas_'+name+'_tests')])
run('network_session',[ROOT/'build/judas_network_session_tests',out/'native-fixture'])
run('network_boundary',[ROOT/'build/judas_network_boundary_tests',out/'boundary-fixture'])
run('network_render',[ROOT/'build/judas_network_render_tests',out/'renderer-fixture'])
run('network_public_js',[ROOT/'build/judas_network_script_tests',out/'script-fixture'])
with socket.socket() as s:
    s.bind(('127.0.0.1',0))
    port=s.getsockname()[1]
run('game_application',[ROOT/'build/judas_network_application_tests',
    ROOT/'projects/connected_workshop/connected_workshop.judasproj',out/'application',str(port)])
run('scene_transition',[ROOT/'build/judas_scene_transition_tests','--output',out/'transitions'])
run('save_evolution',[ROOT/'build/judas_save_evolution_tests',out/'migration-storage'])
lab=ROOT/'projects/save_evolution_lab'
for version,mode in [('v1','write'),('v2','read-v2'),('v3','read-v3')]:
    run('offline_evolution_'+mode,[ROOT/'build/judas_save_evolution_application_tests',
        lab/version/'save_evolution.judasproj',mode,out/'evolution-app'])
run('live_api',[ROOT/'build/judasjs_examples_tests',out/'cookbook','surface'])
run('api_drift',['node',ROOT/'scripts/check_judasjs_api.mjs','--typescript',
    ROOT/'.cache/m90/types/node_modules/typescript','--runtime',out/'cookbook/surface.json'])
copy=out/'editor-project'
shutil.copytree(ROOT/'projects/connected_workshop',copy,ignore=shutil.ignore_patterns('Saves','__pycache__'))
run('editor',[ROOT/'build/judas_editor',copy/'connected_workshop.judasproj'],
    {'JUDAS_EDITOR_AUTOTEST':str(out/'editor'),'JUDAS_EDITOR_AUTOTEST_AUTHORING':'1',
     'XDG_CONFIG_HOME':str(out/'editor-config')})
text=(out/'editor.log').read_text()
assert 'authored scene after play/stop is IDENTICAL' in text and 'save ok' in text
print('Focused checks passed; human acceptance remains pending.')
