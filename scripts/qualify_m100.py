#!/usr/bin/env python3
"""One coordinated native qualification pass. Keeps known failures classified."""
import argparse
import json
import os
from pathlib import Path
import subprocess
import sys

ROOT=Path(__file__).resolve().parents[1]
EXTRA=['judas_joint_tests','judas_wheel_tests','judas_wheel_lifecycle_tests','judas_powertrain_tests','judas_powertrain_lifecycle_tests','judas_gravity_selection_tests','judas_character_motor_tests']
COLD_APPLICATIONS=('utility-ai-application','willow-town-application','willow-town-life')


def ai_cold_commands(tests, build, gates):
    """Read each producer's actual output in a separate native process."""
    extension='.exe' if os.name=='nt' else ''
    commands=[]
    for entry in tests:
        executable=build/(entry['target']+extension)
        if entry['name'] in COLD_APPLICATIONS:
            arguments=[argument.format(output=gates,root=ROOT) for argument in entry['args']]
            commands.append((entry['name']+'-cold',[executable,*arguments,'cold']))
        elif entry['name']=='ai-reservation-script':
            directory=gates/'ai-reservation-cold'
            for mode in ('write','read'):
                commands.append(('ai-reservation-'+mode,[executable,mode,directory]))
    return commands


def qualification_tests(manifest):
    return [test for test in manifest['tests'] if not test.get('manual')]


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--build',type=Path,required=True);p.add_argument('--output',type=Path,required=True)
    p.add_argument('--jobs',type=int,default=1);p.add_argument('--configuration',default='Release')
    p.add_argument('--require-alpha-v2',action='store_true',help='Require Release Alpha V2 with both Linux graphics backends')
    a=p.parse_args();a.build=a.build.resolve();output=a.output.resolve();output.mkdir(parents=True,exist_ok=False)
    manifest=json.loads((ROOT/'tools/validation/suites.json').read_text())
    tests=qualification_tests(manifest)
    targets=sorted({t['target'] for t in tests}|set(EXTRA));records=[]
    identity=None
    def receipt(complete=False):
        (output/'RESULTS.json').write_text(json.dumps({'passed':complete and all(r['exit']==0 for r in records),
            'build':identity,'checks':records,'knownFindings':manifest['known_findings'],'humanAcceptance':False},indent=2)+'\n')

    def run(name,cmd):
        with (output/(name+'.log')).open('w',encoding='utf-8') as f:
            environment=dict(os.environ, JUDAS_ENGINE_ROOT=str(ROOT))
            child=subprocess.run(list(map(str,cmd)),cwd=(output if name.startswith("judas_") else ROOT),env=environment,stdout=f,stderr=subprocess.STDOUT)
        records.append({'name':name,'command':list(map(str,cmd)),'exit':child.returncode})
        receipt()
        if child.returncode:raise RuntimeError(name+' failed: '+str(output/(name+'.log')))
    run('build',['cmake','--build',a.build,'--config',a.configuration,'--parallel',str(a.jobs),'--target','judas',*targets])
    build=a.build/a.configuration if os.name=='nt' and (a.build/a.configuration).is_dir() else a.build
    identity=json.loads(subprocess.check_output([str(build/('judas.exe' if os.name=='nt' else 'judas')), '--build-info-json'],text=True))
    if a.require_alpha_v2 and any(identity.get(key) != value for key,value in {
            'version':'2.0.0-alpha.1','maturity':'alpha','release_name':'Judas Engine Alpha v2.0.0',
            'channel':'Alpha','configuration':'Release','platform':'Linux','source_dirty':False,'vulkan':True}.items()):
        raise ValueError('Alpha V2 qualification requires a clean dual-backend Linux Release build')
    native=[sys.executable,ROOT/'scripts/prealpha_validate.py','--build-dir',build,'--output',output/'gates','--timeout','180','--expected-channel',identity['channel']]
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
    for name,command in ai_cold_commands(tests,build,output/'gates'):
        run(name,command)
    if 'vehicle-v2-dynamics' in names:
        run('vehicle-v2-cold',[build/('judas_vehicle_dynamics_tests.exe' if os.name=='nt' else 'judas_vehicle_dynamics_tests'),
                               'read',output/'gates'/'vehicle-v2-cold'])
    for target in EXTRA:run(target,[build/(target+('.exe' if os.name=='nt' else ''))])
    run('python-controls',[sys.executable,'-m','unittest','discover','-s',ROOT/'tests','-p','test_*.py','-v'])
    run('identity-controls',[sys.executable,ROOT/'scripts/prealpha_build_identity_checks.py','--output',output/'identity.json'])
    run('packaging-controls',[sys.executable,ROOT/'scripts/prealpha_packaging_checks.py','--output',output/'packaging.json'])
    receipt(complete=True)
    print('M100 coordinated qualification passed; known limitations remain classified separately')

if __name__=='__main__':main()
