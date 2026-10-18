#!/usr/bin/env python3
"""Reproduce the preserved failed oracle and independent physical-identity proof.

Requires an existing native Linux Release Unix Makefiles build containing
judas_origin_environment_tests and its libraries. Builds only this evidence TU;
never changes engine source, original tests, fixtures or thresholds.
"""
import argparse
import datetime
import hashlib
import json
import os
from pathlib import Path
import shlex
import subprocess
import time

parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--repo',required=True,type=Path)
parser.add_argument('--build',required=True,type=Path)
parser.add_argument('--output',required=True,type=Path)
a=parser.parse_args()
root=a.repo.resolve();build=a.build.resolve();out=a.output.resolve()
if out.exists():parser.error('--output must be genuinely fresh')
evidence=Path(__file__).resolve().parent.parent
pins=json.loads((evidence/'FROZEN_SOURCES.json').read_text())
for path,pin in pins.items():
    if hashlib.sha256((root/path).read_bytes()).hexdigest()!=pin['sha256']:
        parser.error('pinned fluid or original fixture source differs: '+path)
flags={}
for line in (build/'CMakeFiles/judas_origin_environment_tests.dir/flags.make').read_text().splitlines():
    if line.startswith('CXX_') and ' = ' in line:
        key,value=line.split(' = ',1);flags[key]=shlex.split(value)
out.mkdir(parents=True)
source=Path(__file__).resolve().parent/'AliasProbe.cpp'
obj=out/'AliasProbe.o';exe=out/'liquid_alias_probe'
compile_command=['/usr/bin/c++',*flags['CXX_DEFINES'],*flags['CXX_INCLUDES'],
    '-I'+str(root/'third_party'),*flags['CXX_FLAGS'],'-c',str(source),'-o',str(obj)]
link_command=shlex.split((build/'CMakeFiles/judas_origin_environment_tests.dir/link.txt').read_text())
link_command=[str(obj) if p=='CMakeFiles/judas_origin_environment_tests.dir/tests/OriginEnvironmentTests.cpp.o'
    else str(exe) if p=='judas_origin_environment_tests' else p
    for p in link_command if not p.startswith('-Wl,--dependency-file=')]
runs=[]
def run(label,command,extra,expected,cwd):
    env=dict(os.environ);env.update(extra);start=time.monotonic()
    with (out/(label+'.log')).open('w') as log:
        process=subprocess.run(command,cwd=cwd,env=env,stdout=log,stderr=subprocess.STDOUT)
    receipt={'label':label,'command':command,'environment':extra,'exit':process.returncode,
        'expected_exit':expected,'expected_outcome_matched':process.returncode==expected,
        'wall_seconds':time.monotonic()-start}
    runs.append(receipt)
    print(label+': '+str(process.returncode),flush=True)
    return receipt['expected_outcome_matched']
for label,command in [('compile-probe',compile_command),('link-probe',link_command)]:
    if not run(label,command,{},0,build):raise SystemExit('build failed; see output log')
original=build/'judas_origin_environment_tests'
for label,command,environment,expected in [
    ('original-half-turn',[str(original),'--bake-regression','knots',str(out/'original-half-turn-runtime')],{},1),
    ('original-separate-controls',[str(original),'--query-controls','knots',str(out/'original-controls-runtime')],{},0),
    ('original-normalization',[str(original),'--bake-regression','normalization',str(out/'original-normalization-runtime')],{},0),
    ('alias-half-turn',[str(exe),'--bake-regression','knots',str(out/'alias-half-turn-runtime')],{'JUDAS_LIQUID_ALIAS_PROBE':'1'},0),
    ('alias-radial',[str(exe),'--alias-radial',str(out/'alias-radial-runtime')],{'JUDAS_LIQUID_ALIAS_PROBE':'1'},0),
    ('absent-cap-negative',[str(exe),'--bake-regression','knots',str(out/'absent-cap-runtime')],{'JUDAS_LIQUID_ALIAS_PROBE':'1','JUDAS_LIQUID_NEGATIVE_EMPTY_MESH':'1'},1),
]:run(label,command,environment,expected,root)
artifacts={}
for p in [source,original,exe,build/'libjudas_engine.a']:
    artifacts[str(p)]={'bytes':p.stat().st_size,'sha256':hashlib.sha256(p.read_bytes()).hexdigest()}
result={'utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'runs':runs,'artifacts':artifacts,
    'scope':'Original failed contract preserved; independent geometry/identity proof is not a replacement acceptance gate.'}
(out/'RESULTS.json').write_text(json.dumps(result,indent=2)+'\n')
if not all(r['expected_outcome_matched'] for r in runs):raise SystemExit(1)
