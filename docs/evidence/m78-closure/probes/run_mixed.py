#!/usr/bin/env python3
"""Exact mixed motor/cold physics regression plus actual physical endpoint witness."""
from pathlib import Path
import hashlib,json,os,shlex,shutil,subprocess,time
r=Path.cwd();base=r/'.cache/m78-followup/release';build=r/'.cache/m78-closure/final-build';out=r/'.cache/m78-closure/mixed-final';ev=r/'docs/evidence/m78-closure/mixed-final'
out.mkdir(parents=True,exist_ok=False);ev.mkdir(parents=True,exist_ok=False)
h=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
identity=json.loads((r/'docs/evidence/m78-closure/final-build/BUILD.json').read_text());archive=build/'libjudas_engine.a'
for p,v in identity['sourceSHA256'].items():assert h(r/p)==v,p
assert h(archive)==identity['archive']['sha256']
env=dict(os.environ,SDL_VIDEODRIVER='offscreen',SDL_AUDIODRIVER='dummy',LIBGL_ALWAYS_SOFTWARE='1',XDG_DATA_HOME=str(out/'data'),XDG_CONFIG_HOME=str(out/'config'))
for k in list(env):
 if k.startswith(('M78_DIAGNOSTIC_','JUDAS_DIAGNOSTIC_','JUDAS_BOUNDARY_')):del env[k]
rows=[];binaries={}
def run(name,cmd,extra={},cwd=r):
 start=time.perf_counter();p=subprocess.run(cmd,cwd=cwd,env=dict(env,**extra),stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=180);log=ev/(name+'.log');log.write_text(p.stdout)
 rows.append(dict(name=name,command=cmd,extraEnvironment=extra,exitCode=p.returncode,seconds=time.perf_counter()-start,logSHA256=h(log),summary=[s for s in p.stdout.splitlines()if 'SUMMARY'in s or 'MIXED_BOUNDARY'in s]));(ev/'RESULTS.json').write_text(json.dumps(dict(scope=__doc__,sourceSHA256=identity['sourceSHA256'],archiveSHA256=h(archive),binaries=binaries,results=rows),indent=2)+'\n');print(name,p.returncode,rows[-1]['summary'][-2:],flush=True)
 if p.returncode:raise SystemExit('Preserved unexpected '+name)
for name,target,obj in [('publication','judas_world_persistence_tests',r/'.cache/m78-closure/mechanism-before-v2/publication-isolation.o'),('mixed-balance','judas_balance_lab_tests',r/'.cache/m78-closure/mechanism-before-v2/mixed-balance.o')]:
 cmd=shlex.split((base/f'CMakeFiles/{target}.dir/link.txt').read_text());cmd=[str(obj)if x.endswith('.cpp.o') else str(archive)if x=='libjudas_engine.a'else x for x in cmd if not x.startswith('-Wl,--dependency-file=')];exe=out/name;cmd[cmd.index('-o')+1]=str(exe);run(name+'-link',cmd,cwd=base);binaries[name]=dict(path=str(exe.relative_to(r)),sha256=h(exe))
run('publication',[str(out/'publication')])
for phase in ['write','read']:run('mixed-cold-'+phase,[str(out/'mixed-balance'),str(out/'mixed-pair'),'cold-'+phase])
old=json.loads((r/'docs/evidence/m78-cold-boundary/final-current-standing/RECEIPT.json').read_text());source=r/'docs/evidence/m78-closure/review-physics/MixedCurrentPhysicalEndpointObserver.cpp';assert h(source)=='df72a228637da53eb48d0628272af1734590331d2875e92b787a1d0ef6b5b4d4'
cmd=old['steps'][0]['command'];cmd[cmd.index('-c')+1]=str(source);cmd[cmd.index('-o')+1]=str(out/'observer.o');run('observer-compile',cmd)
cmd=old['steps'][1]['command'];cmd=[str(out/'observer.o')if x.endswith('/observer.o')else str(archive)if x.endswith('/libjudas_engine.a')else x for x in cmd];exe=out/'observer';cmd[cmd.index('-o')+1]=str(exe);run('observer-link',cmd,cwd=base);binaries['observer']=dict(path=str(exe.relative_to(r)),sha256=h(exe))
case=out/'actual-physical'
run('actual-write',[str(exe),str(case),'cold-write'],{'JUDAS_DIAGNOSTIC_CONTINUE_WRITE':'1'});written=json.loads((case/'physical-final.json').read_text());shutil.copyfile(case/'physical-final.json',ev/'physical-uninterrupted-final.json')
run('actual-read',[str(exe),str(case),'cold-read']);restored=json.loads((case/'physical-final.json').read_text());shutil.copyfile(case/'physical-final.json',ev/'physical-restored-final.json')
for p in ['physical-before-save.json','physical-immediately-restored.json','actual-drive-sidecar.json']:shutil.copyfile(case/p,ev/p)
result=json.loads((ev/'RESULTS.json').read_text());result['actualEndpointExactWithinPair']=written==restored;result['actualEndpointObserverSHA256']=h(source);result['saveSHA256']=h(case/'slots/m77.save');(ev/'RESULTS.json').write_text(json.dumps(result,indent=2)+'\n');print('endpoint identical within pair:',written==restored)
