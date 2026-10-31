#!/usr/bin/env python3
"""One coordinated native qualification pass. Keeps known failures classified."""
import argparse
import json
import os
from pathlib import Path
import subprocess
import sys
from prealpha_validate import partition_channel_tests

ROOT=Path(__file__).resolve().parents[1]
GROUPS=['2d','core','mk2','water','network','lifecycle','onboarding','application','liquid','input','rendering']
EXTRA=['judas_joint_tests','judas_wheel_tests','judas_wheel_lifecycle_tests','judas_powertrain_tests','judas_powertrain_lifecycle_tests','judas_gravity_selection_tests','judas_player_swimming_tests']


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--build',type=Path,required=True);p.add_argument('--output',type=Path,required=True)
    p.add_argument('--jobs',type=int,default=1);p.add_argument('--configuration',default='Release')
    a=p.parse_args();a.build=a.build.resolve();output=a.output.resolve();output.mkdir(parents=True,exist_ok=False)
    manifest=json.loads((ROOT/'tools/validation/suites.json').read_text())
    tests=[t for t in manifest['tests'] if set(GROUPS)&set(t['groups']) and not t.get('manual')]
    targets=sorted({t['target'] for t in tests}|set(EXTRA));records=[];unavailable=[]
    def run(name,cmd):
        with (output/(name+'.log')).open('w',encoding='utf-8') as f:
            environment=dict(os.environ, JUDAS_ENGINE_ROOT=str(ROOT))
            child=subprocess.run(list(map(str,cmd)),cwd=(output if name.startswith("judas_") else ROOT),env=environment,stdout=f,stderr=subprocess.STDOUT)
        records.append({'name':name,'command':list(map(str,cmd)),'exit':child.returncode})
        (output/'RESULTS.json').write_text(json.dumps({'passed':all(r['exit']==0 for r in records),'checks':records,'knownFindings':manifest['known_findings'],'humanAcceptance':False,'unavailableTests':unavailable},indent=2)+'\n')
        if child.returncode:raise RuntimeError(name+' failed: '+str(output/(name+'.log')))
    run('build',['cmake','--build',a.build,'--config',a.configuration,'--parallel',str(a.jobs),'--target','judas',*targets])
    build=a.build/a.configuration if os.name=='nt' and (a.build/a.configuration).is_dir() else a.build
    identity=json.loads(subprocess.check_output([str(build/('judas.exe' if os.name=='nt' else 'judas')), '--build-info-json'],text=True))
    tests,unavailable=partition_channel_tests(tests,identity['channel'])
    native=[sys.executable,ROOT/'scripts/prealpha_validate.py','--build-dir',build,'--output',output/'gates','--timeout','180']
    for test in tests:native+=['--test',test['name']]
    run('native-gates',native)
    # Independent processes must reconstruct real pending motion, not a cached
    # same-process controller status. Producer paths are the manifest outputs.
    names={test['name'] for test in tests}
    for name,reader in [('one-way-platform','judas_one_way_platform_tests'),('grid-movement','judas_grid_movement_tests')]:
        if name in names:
            executable=build/(reader+('.exe' if os.name=='nt' else ''))
            run(name+'-cold',[executable,output/'gates'/name,'--cold'])
            if name=='grid-movement':run(name+'-cold-queue',[executable,output/'gates'/name,'--cold-queue'])
    for target in EXTRA:run(target,[build/(target+('.exe' if os.name=='nt' else ''))])
    run('release-python',[sys.executable,ROOT/'tests/test_release_manifest.py'])
    run('release-packaging',[sys.executable,ROOT/'tests/test_release_packaging.py'])
    run('identity-controls',[sys.executable,ROOT/'scripts/prealpha_build_identity_checks.py','--output',output/'identity.json'])
    run('packaging-controls',[sys.executable,ROOT/'scripts/prealpha_packaging_checks.py','--output',output/'packaging.json'])
    print('M100 coordinated qualification passed; known limitations remain classified separately')

if __name__=='__main__':main()
