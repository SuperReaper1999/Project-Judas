#!/usr/bin/env python3
"""Real exported game + cold process save/load; no private water-state injection."""
import argparse
import datetime
import hashlib
import json
import os
from pathlib import Path
import subprocess
import time

p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--runtime',type=Path,required=True)
p.add_argument('--exporter',type=Path,required=True)
p.add_argument('--project',type=Path,required=True)
p.add_argument('--output',type=Path,required=True,help='Fresh directory outside the engine checkout')
a=p.parse_args()
repo=Path(__file__).resolve().parents[2]
out=a.output.resolve()
if out.exists() or out.is_relative_to(repo):p.error('Choose a fresh directory outside the source checkout; existing evidence is never replaced.')
out.mkdir(parents=True)
results=[]
def run(name,command,cwd,env=None):
    started=time.monotonic()
    with (out/(name+'.log')).open('w') as log:
        result=subprocess.run(command,cwd=cwd,env=env,stdout=log,stderr=subprocess.STDOUT,timeout=120)
    row={'name':name,'command':command,'cwd':str(cwd),'exit':result.returncode,'seconds':time.monotonic()-started}
    results.append(row)
    if result.returncode:raise RuntimeError(f'{name} failed; see {out/(name+".log")}')
try:
    identity=json.loads(subprocess.check_output([str(a.runtime.resolve()),'--build-info-json'],text=True))
    run('export',[str(a.exporter.resolve()),str(a.project.resolve()),str(out/'original-package'),str(a.runtime.resolve())],repo)
    package=out/'relocated game';(out/'original-package').rename(package)
    working=out/'foreign-working-directory';working.mkdir()
    commands={
      'write':'FRAMES 180\nLOG_EVERY 30\nFRAME_SECONDS 0.016666667\nACTION control_toggle 1 20 21\nACTION save 1 80 81\nWAIT_SERVICES save 10000 100\n',
      'read':'FRAMES 180\nLOG_EVERY 30\nFRAME_SECONDS 0.016666667\nACTION load 1 20 21\nWAIT_SERVICES save 10000 40\nWAIT_SERVICES scene 10000 45\nEXPECT_SCENE Scenes/pool.judas 60\n',
      'scene-controls':'''FRAMES 200
LOG_EVERY 30
FRAME_SECONDS 0.016666667
ACTION rotated 1 10 11
WAIT_SERVICES scene 10000 15
EXPECT_SCENE Scenes/rotated.judas 20
ACTION radial 1 40 41
WAIT_SERVICES scene 10000 45
EXPECT_SCENE Scenes/radial.judas 50
ACTION original 1 70 71
WAIT_SERVICES scene 10000 75
EXPECT_SCENE Scenes/planet-basin.judas 80
ACTION pool 1 100 101
WAIT_SERVICES scene 10000 105
EXPECT_SCENE Scenes/pool.judas 110
ACTION reset 1 130 131
WAIT_SERVICES scene 10000 135
EXPECT_SCENE Scenes/pool.judas 140
ACTION pause 1 150 151
EXPECT_PAUSED 1 155
ACTION pause 1 170 171
EXPECT_PAUSED 0 175
'''}
    for name,script in commands.items():
        path=out/(name+'.harness');path.write_text(script+f'SCREENSHOT 160 {out/(name+".png")}\n')
        env=dict(os.environ,JUDAS_TEST_SCRIPT=str(path),JUDAS_GPU_DIAGNOSTICS='1',JUDAS_WINDOW_SIZE='1280x720',
                 JUDAS_ENGINE_ROOT=str(package/'engine'),XDG_DATA_HOME=str(out/'user-data'))
        run(name,[str(package/'judas'),str(package/'game.judasproj')],working,env)
    restored=[]
    for line in (out/'read.log').read_text().splitlines():
        marker='WATER_WORKSHOP_RESTORED '
        if marker in line:restored.append(json.loads(line.split(marker,1)[1]))
    if len(restored)!=1:raise RuntimeError('Exactly one genuine script restore callback must report its water state')
    state=restored[0];w=state['water']
    if not state['pump'] or w['addedQuantity']<=0 or w['removedQuantity']<=0 or w['paused'] or w['error']:
        raise RuntimeError('Cold restore lost game state/source receipts or retained a failed water region')
    if abs(w['quantity']-(w['initialQuantity']+w['addedQuantity']-w['removedQuantity']))>1e-6*max(1,w['quantity']):
        raise RuntimeError('Cold water/source ledger mismatch')
    record={'passed':True,'restored':state,'build':identity,'source_preview':True,'human_acceptance':False}
except Exception as e:
    record={'passed':False,'error':str(e),'source_preview':True,'human_acceptance':False}
finally:
    record.update(started_or_finished_utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),runs=results)
    record['files']={str(f.relative_to(out)):hashlib.sha256(f.read_bytes()).hexdigest() for f in sorted(out.glob('*.log'))}
    (out/'RESULTS.json').write_text(json.dumps(record,indent=2)+'\n')
print(json.dumps({k:v for k,v in record.items() if k not in ('build','files','restored')},indent=2))
raise SystemExit(not record['passed'])
