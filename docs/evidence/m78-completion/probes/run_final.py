#!/usr/bin/env python3
"""Serial focused final-source checks, no full-suite rerun or VM timing gate."""
from pathlib import Path
import subprocess,os,json,shlex,hashlib,time,shutil
R=Path.cwd();B=R/'build';E=R/'docs/evidence/m78-completion';C=R/'.cache/m78-completion'
C.mkdir(parents=True,exist_ok=True);(E/'runs').mkdir(parents=True,exist_ok=True)
sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
report=json.loads((E/'RESULTS.json').read_text())
env=dict(os.environ,SDL_VIDEODRIVER='offscreen',SDL_AUDIODRIVER='dummy',JUDAS_ENGINE_ROOT=str(R))
def run(name,cmd,cwd=R,timeout=180):
 log=E/'runs'/(name+'.log');assert not log.exists(),name;t=time.monotonic();p=subprocess.run(cmd,cwd=cwd,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=timeout);log.write_text(p.stdout);row={'name':name,'command':list(map(str,cmd)),'cwd':str(cwd),'exitCode':p.returncode,'seconds':time.monotonic()-t,'logSHA256':sha(log),'summary':[x for x in p.stdout.splitlines()if 'SUMMARY'in x or ('checks'in x and 'failures'in x)]};report['runs'].append(row);(E/'RESULTS.json').write_text(json.dumps(report,indent=2)+'\n');print(name,p.returncode,row['summary'][-3:],flush=True);return p.returncode
# Establish current archive and touched interaction binaries without a clean/full build.
assert run('current-focused-build',['cmake','--build',str(B),'--target','judas_balance_lab_tests','judas_physical_animation_tests','judas_world_persistence_tests','judas_character_motor_tests','judas_mod_content_tests','judas_impact_robustness_tests','-j2'],timeout=600)==0
report['archiveSHA256']=sha(B/'libjudas_engine.a')
for target,args in [('judas_world_persistence_tests',[]),('judas_character_motor_tests',[]),('judas_physical_animation_tests',[]),('judas_impact_robustness_tests',[]),('judas_balance_lab_tests',[str(C/'balance-standing'),'standing']),('judas_balance_lab_tests',[str(C/'balance-cold'),'cold-write']),('judas_balance_lab_tests',[str(C/'balance-cold'),'cold-read'])]:
 exe=B/target;report['binaries'][target]={'sha256':sha(exe)};run(target+('-'+args[-1]if args else ''),[str(exe),*args],timeout=240)
run('api-drift',['node','scripts/check_judasjs_api.mjs','--typescript',str(R/'.cache/m73/typescript/node_modules/typescript')])
# Recompile the exact preserved 900-step mesh/box one/four rig observer.
flags={}
for line in (B/'CMakeFiles/judas_impact_robustness_tests.dir/flags.make').read_text().splitlines():
 if ' = 'in line:k,v=line.split(' = ',1);flags[k]=shlex.split(v)
source=R/'docs/evidence/m78-followup/probes/concave_final.cpp';obj=C/'mesh-performance.o';exe=C/'mesh-performance'
assert run('mesh-performance-compile',['c++',*flags['CXX_DEFINES'],*flags['CXX_INCLUDES'],*flags['CXX_FLAGS'],'-c',str(source),'-o',str(obj)])==0
cmd=shlex.split((B/'CMakeFiles/judas_impact_robustness_tests.dir/link.txt').read_text());cmd=[str(obj)if x.endswith('/ImpactRobustnessTests.cpp.o')else x for x in cmd if not x.startswith('-Wl,--dependency-file=')];cmd[cmd.index('-o')+1]=str(exe);assert run('mesh-performance-link',cmd,B)==0
report['binaries']['mesh-performance']={'sha256':sha(exe),'sourceSHA256':sha(source),'objectSHA256':sha(obj)}
run('mesh-performance',[str(exe),str(C/'performance')],timeout=240)
for f in (C/'performance').iterdir():
 if f.suffix in ['.csv','.json']:shutil.copyfile(f,E/f.name)
rows=[json.loads(x)for x in (E/'runs/mesh-performance.log').read_text().splitlines()if x.startswith('{"case"')]
(E/'PERFORMANCE.json').write_text(json.dumps({'rows':rows,'platform':'native Linux host; offscreen presentation','gateMs':16.67,'fourRigP95Pass':next(x['p95Ms']for x in rows if x['case']=='mesh-4')<=16.67,'note':'Same preserved fixture, 900 fixed steps per case at 60 Hz. Maxima and CCD budgets retained. No Windows VM performance gate.'},indent=2)+'\n')
