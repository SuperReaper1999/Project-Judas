#!/usr/bin/env python3
"""Bounded real-project regressions for this pass, not a full production certificate.
Requires the matching native build, GL context (xvfb is suitable) and original
tracked project fixtures. Never builds, downloads, edits fixtures or promotes.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import socket
import subprocess
import time

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--root', type=Path, default=Path(__file__).resolve().parents[3])
p.add_argument('--build-dir', type=Path, default=Path('build'))
p.add_argument('--output', type=Path, required=True)
p.add_argument('--phase', choices=['native', 'developer', 'ui', 'application', 'export'], required=True)
a = p.parse_args()
r = a.root.resolve()
b = (r / a.build_dir).resolve()
out = a.output.resolve()
out.mkdir(parents=True, exist_ok=False)
env = {k:v for k,v in os.environ.items() if not k.startswith('JUDAS_')}
env.update(SDL_AUDIODRIVER='dummy', JUDAS_WORLD_STATE='none', XDG_DATA_HOME=str(out/'user-data'))
records = []
def sha(path):
    with Path(path).open('rb') as f:return hashlib.file_digest(f, 'sha256').hexdigest()
def run(name, command, override=None, cwd=r, timeout=180):
    command = list(map(str, command))
    e = env.copy();e.update(override or {})
    entry = dict(name=name, argv=command, cwd=str(cwd), environmentOverrides=override or {},
                 startedUtc=time.strftime('%Y-%m-%dT%H:%M:%SZ', time.gmtime()))
    if Path(command[0]).is_file():entry['binarySha256'] = sha(command[0])
    start = time.monotonic()
    with (out/(name+'.log')).open('w') as log:
        try:
            result = subprocess.run(command, cwd=cwd, env=e, stdout=log,
                                    stderr=subprocess.STDOUT, timeout=timeout)
            entry['exit'] = result.returncode
        except subprocess.TimeoutExpired:
            entry.update(exit=None, timeout=True)
    entry['seconds'] = time.monotonic()-start
    records.append(entry)
    (out/'RESULTS.json').write_text(json.dumps(records,indent=2)+'\n')
    print(name, entry['exit'], flush=True)
    if entry['exit'] != 0:raise RuntimeError(name+' failed; log preserved')

if a.phase == 'native':
    for name in ['impact_robustness','kinematic','post_consumer_ragdoll','physical_balance_controller',
                 'joint','contact_cache','prefab']:
        run(name,[b/('judas_'+name+'_tests')])
    run('save_storage',[b/'judas_save_storage_tests',out/'m61-storage-prealpha'])
    run('ragdoll',[b/'judas_ragdoll_tests','--output',out/'ragdoll'])
    run('script_developer_tools',[b/'judas_script_developer_tools_tests'])
elif a.phase == 'developer':
    run('script_developer_tools',[b/'judas_script_developer_tools_tests'])
elif a.phase == 'ui':
    for name in ['text_localization','localization_application','ui_production']:
        run(name,[b/('judas_'+name+'_tests'),out/name])
    run('cookbook',[b/'judasjs_examples_tests',out/'cookbook'])
    run('api_drift',['node',r/'scripts/check_judasjs_api.mjs'])
elif a.phase == 'application':
    run('live_api',[b/'judasjs_examples_tests',out/'cookbook','surface'])
    run('api_drift',['node',r/'scripts/check_judasjs_api.mjs','--runtime',out/'cookbook/surface.json'])
    run('network_processes',['python3',r/'scripts/m99_demo_checks.py','--output',out/'two-process',
                             '--runner',b/'judas_network_script_tests','--skip-bindings'])
    with socket.socket() as s:s.bind(('127.0.0.1',0));port=s.getsockname()[1]
    run('network_application',[b/'judas_network_application_tests',
        r/'projects/connected_workshop/connected_workshop.judasproj',out/'network-app',port])
    lab = r/'projects/save_evolution_lab'
    for version,mode in [('v1','write'),('v2','read-v2'),('v3','read-v3'),
                         ('v2','mods'),('v2','native-failure')]:
        run('cold_'+mode,[b/'judas_save_evolution_application_tests',
            lab/version/'save_evolution.judasproj',mode,out/'evolution'])
    copy = out/'editor-project'
    shutil.copytree(r/'projects/connected_workshop',copy,ignore=shutil.ignore_patterns('Saves','__pycache__'))
    run('editor',[b/'judas_editor',copy/'connected_workshop.judasproj'],
        dict(JUDAS_EDITOR_AUTOTEST=str(out/'editor'),JUDAS_EDITOR_AUTOTEST_AUTHORING='1',
             XDG_CONFIG_HOME=str(out/'editor-config')))
    text = (out/'editor.log').read_text()
    assert 'authored scene after play/stop is IDENTICAL' in text and 'save ok' in text
    run('consumer_tuppence',[b/'judas_ball_pusher_application_tests',
        r/'projects/tuppence/Scenes/cabinet.judas',out/'tuppence','play'],timeout=180)
else:
    destination = out/'export'
    run('export',[b/'judas_export',r/'projects/connected_workshop/connected_workshop.judasproj',
        destination,b/'judas'])
    relocated = out/'relocated package ü with spaces'
    destination.rename(relocated)
    provenance = json.loads((relocated/'BUILD_PROVENANCE.json').read_text())
    assert provenance['runtime_sha256'] == sha(relocated/'judas')
    harness = out/'shipping.harness'
    harness.write_text('FRAMES 240\nFRAME_SECONDS 0.016666667\nLOG_EVERY 60\n'
                       'AXIS move_x 1 100 130\nSCREENSHOT 200 '+str(out/'shipping.png')+'\n')
    unrelated = out/'unrelated-cwd';unrelated.mkdir()
    run('relocated_runtime',[relocated/'judas'],dict(JUDAS_TEST_SCRIPT=str(harness)),cwd=unrelated)
    assert (out/'shipping.png').is_file()
print('Phase passed; human acceptance not established.')
