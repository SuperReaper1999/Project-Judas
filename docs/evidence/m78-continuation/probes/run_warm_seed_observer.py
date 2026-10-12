#!/usr/bin/env python3
import pathlib,shlex,subprocess,hashlib,shutil,os,time,json
r=pathlib.Path.cwd();base=r/'.cache/m78-followup/release';owned=r/'.cache/m78-continuation/cold-warm-seed-observer';ev=r/'docs/evidence/m78-continuation';rows=[]
h=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
def flags(target):
 d={}
 for line in (base/f'CMakeFiles/{target}.dir/flags.make').read_text().splitlines():
  if ' = 'in line:k,v=line.split(' = ',1);d[k]=shlex.split(v)
 return d
for name,target in [('PhysicsWorld.cpp','judas_engine'),('JointSolver.cpp','judas_engine'),('BalanceLabWarmSeedObserver.cpp','judas_balance_lab_tests')]:
 d=flags(target);cmd=['c++',*d['CXX_DEFINES'],*d['CXX_INCLUDES'],'-I'+str(r/'third_party'),*d['CXX_FLAGS'],'-c',str(owned/name),'-o',str(owned/(name+'.o'))];t=time.perf_counter();p=subprocess.run(cmd,text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT);(ev/('warm-seed-'+name+'-compile.log')).write_text(p.stdout);rows.append(dict(command=cmd,exitCode=p.returncode,seconds=time.perf_counter()-t));assert p.returncode==0,p.stdout;print('compiled',name,flush=True)
archive=owned/'libjudas_engine.a';shutil.copyfile(base/'libjudas_engine.a',archive);subprocess.run(['ar','r',str(archive),str(owned/'PhysicsWorld.cpp.o'),str(owned/'JointSolver.cpp.o')],check=True)
cmd=shlex.split((base/'CMakeFiles/judas_balance_lab_tests.dir/link.txt').read_text());cmd=[x for x in cmd if not x.startswith('-Wl,--dependency-file=')];cmd[cmd.index('CMakeFiles/judas_balance_lab_tests.dir/tests/BalanceLabTests.cpp.o')]=str(owned/'BalanceLabWarmSeedObserver.cpp.o');cmd[cmd.index('libjudas_engine.a')]=str(archive);cmd[cmd.index('-o')+1]=str(owned/'observer');t=time.perf_counter();p=subprocess.run(cmd,cwd=base,text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT);(ev/'warm-seed-link.log').write_text(p.stdout);rows.append(dict(command=cmd,exitCode=p.returncode,seconds=time.perf_counter()-t));assert p.returncode==0,p.stdout
for name in ['unchanged','seeds-off']:
 out=owned/name;e=os.environ.copy();e.update(SDL_VIDEODRIVER='offscreen',SDL_AUDIODRIVER='dummy',LIBGL_ALWAYS_SOFTWARE='1',JUDAS_DIAGNOSTIC_CONTINUE_WRITE='1',XDG_DATA_HOME=str(r/'.cache/m78-continuation/data'),XDG_CONFIG_HOME=str(r/'.cache/m78-continuation/config'))
 if name=='seeds-off':e['JUDAS_DIAGNOSTIC_WARM_SEEDS_OFF']='1'
 cmd=[str(owned/'observer'),str(out),'cold-write'];t=time.perf_counter();p=subprocess.run(cmd,env=e,text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT);(ev/('warm-seed-'+name+'-run.log')).write_text(p.stdout);rows.append(dict(command=cmd,exitCode=p.returncode,seconds=time.perf_counter()-t,seedDisabled=name=='seeds-off'));print(name,p.returncode);print('\n'.join(x for x in p.stdout.splitlines()if 'WARM_SEED' in x or 'STATE_OBSERVER' in x or 'ACTOR 12' in x))
identity=json.loads((owned/'IDENTITY.json').read_text());identity.update(steps=rows,exeSHA256=h(owned/'observer'),archiveSHA256=h(archive));(ev/'warm-seed-isolation.json').write_text(json.dumps(identity,indent=2)+'\n')
