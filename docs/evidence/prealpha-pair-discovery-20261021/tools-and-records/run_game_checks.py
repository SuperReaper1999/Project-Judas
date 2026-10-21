#!/usr/bin/env python3
"""Explicitly execute the existing gameplay/streaming fixtures, serially."""
from pathlib import Path
import datetime
import hashlib
import json
import os
import subprocess
import time

ROOT = Path(__file__).resolve().parents[2]
HERE = Path(__file__).resolve().parent
OUT = HERE / 'integration'
if OUT.exists(): raise RuntimeError('Fresh output required')
OUT.mkdir()
env = os.environ.copy()
env['SDL_AUDIODRIVER'] = 'dummy'
for key,folder in [('XDG_DATA_HOME','userdata'),('XDG_CONFIG_HOME','config'),('XDG_CACHE_HOME','cache')]:
    env[key] = str(OUT/folder)
sha = lambda p: hashlib.sha256(Path(p).read_bytes()).hexdigest()
core = ROOT/'build/libjudas_engine.a'
before = sha(core)
cases=[]
for mode in ['write','read']:
    cases.append(('hearthworld-'+mode,[HERE/'app-drivers/judas_hearthworld_application_tests',ROOT/'projects/hearthworld/hearthworld.judasproj',mode,OUT/'hearthworld']))
for mode in ['write','read','read-b']:
    cases.append(('stream-'+mode,[HERE/'app-drivers/judas_save_streaming_tests',ROOT/'projects/streamed_range/streamed_range.judasproj',mode,OUT/'stream']))
report={'core_sha256':before,'results':[],'native_nvidia_required':True,'audio':'dummy; no listening qualification'}
for name,args in cases:
    args=list(map(str,args)); started=time.monotonic()
    with (OUT/(name+'.log')).open('w')as log:
        run=subprocess.run(args,cwd=ROOT,env=env,stdout=log,stderr=subprocess.STDOUT,timeout=180)
    text=(OUT/(name+'.log')).read_text(errors='replace')
    graphics=[line for line in text.splitlines()if line.startswith('Graphics:')]
    native=bool(graphics)and all('NVIDIA' in line and 'llvmpipe'not in line.lower()for line in graphics)
    row={'name':name,'argv':args,'utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'exit':run.returncode,'seconds':time.monotonic()-started,'native_nvidia':native,'graphics':graphics,'binary_sha256':sha(args[0]),'log_sha256':sha(OUT/(name+'.log'))}
    report['results'].append(row)
    report['core_unchanged']=sha(core)==before
    report['pass_all']=all(r['exit']==0 and r['native_nvidia']for r in report['results'])and report['core_unchanged']
    (OUT/'RESULTS.json').write_text(json.dumps(report,indent=2)+'\n')
    print(name,'exit',run.returncode,'native',native,flush=True)
    if not report['pass_all']:raise SystemExit(1)
