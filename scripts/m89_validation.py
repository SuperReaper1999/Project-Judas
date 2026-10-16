#!/usr/bin/env python3
"""Focused M89 Linux reproduction; no full production suite or hardware claim.
Run from the repository with a working desktop GL session. TypeScript is tooling
only: pass an existing module path, or install it externally to this repository.
"""
import argparse, os, subprocess, json, shutil, tempfile
from pathlib import Path
R=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--output',type=Path,default=R/'.cache/m89/repro')
p.add_argument('--build',action='store_true');p.add_argument('--regressions',action='store_true')
p.add_argument('--performance',action='store_true');p.add_argument('--export',action='store_true')
p.add_argument('--typescript',default=os.environ.get('JUDAS_TYPESCRIPT'))
a=p.parse_args();out=a.output.resolve();out.mkdir(parents=True,exist_ok=True)
checks=['judas_local_multiplayer_tests','judas_local_device_adapter_tests','judas_local_multiplayer_script_tests','judas_local_multiplayer_render_tests','judas_local_multiplayer_application_tests']
regressions=[('judas_input_tests',[]),('judas_paired_input_tests',[]),('judas_paired_input_script_tests',[str(out/'paired-js')]),('judas_runtime_ui_tests',['--output',str(out/'ui')]),('judas_runtime_ui_application_tests',['--output',str(out/'ui-app')]),('judas_character_motor_tests',[]),('judas_render_camera_tests',['--output',str(out/'cameras')]),('judas_audio_tests',[]),('judas_breakwater_application_tests',[])]
results=[]
def run(name,cmd,env=None,cwd=R):
    print(name,flush=True)
    with (out/(name+'.log')).open('w')as log:
        r=subprocess.run([str(x)for x in cmd],cwd=cwd,env={**os.environ,**(env or {})},stdout=log,stderr=subprocess.STDOUT,timeout=1800)
    results.append({'name':name,'command':[str(x)for x in cmd],'cwd':str(cwd),'environment':env or {},'exit':r.returncode})
    (out/'commands.json').write_text(json.dumps(results,indent=2)+'\n')
    if r.returncode:raise SystemExit(f'{name} failed; original output retained')
if a.build:
    run('configure',['cmake','-S',R,'-B',R/'build','-DCMAKE_BUILD_TYPE=Release'])
    targets=checks+['judas','judas_export','judas_editor','judas_local_multiplayer_performance']+([n for n,_ in regressions]if a.regressions else[])
    run('build',['cmake','--build',R/'build','--target',*targets,'-j4'])
for name in checks:run(name,[R/'build'/name])
args=['node',R/'scripts/check_judasjs_api.mjs','--runtime',R/'.cache/m89/script-probe/surface.json']
if a.typescript:args+=['--typescript',a.typescript]
run('api',args)
if a.regressions:
    for n,args in regressions:run(n,[R/'build'/n,*args])
if a.performance:
    for count,mode in [(2,'shared'),(4,'shared'),(4,'split')]:
        name=f'performance-{count}-{mode}'
        run(name,[R/'build/judas_local_multiplayer_performance',count,mode,out/(name+'.json')],{'JUDAS_PROFILE':'1','JUDAS_PROFILE_OUTPUT':str(out/(name+'-m56.json'))})
if a.export:
    package=out/'CoreScramble'
    run('export',[R/'build/judas_export',R/'projects/core_scramble/core_scramble.judasproj',package,R/'build/judas'])
    # A genuinely moved, normal package starts from an unrelated directory.
    moved=Path(tempfile.mkdtemp(prefix='judas-m89-moved-'))/'CoreScramble';shutil.copytree(package,moved)
    pilot=out/'package.input';pilot.write_text('FRAMES 180\nLOG_EVERY 0\nACTION four 1 10 12\nACTION split 1 20 22\nSCREENSHOT 90 '+str(out/'moved.png')+'\nACTION pause 1 110 112\nACTION pause 1 140 142\n')
    run('moved-package',[moved/'judas'],{'JUDAS_ENGINE_ROOT':'/missing/development/path','JUDAS_TEST_SCRIPT':str(pilot)},Path('/tmp'))
    if not(out/'moved.png').is_file():raise SystemExit('Moved package did not capture frame')
    (out/'moved-package-location.txt').write_text(str(moved)+'\n')
print('Focused M89 reproduction passed; human/hardware acceptance remains separate.')
