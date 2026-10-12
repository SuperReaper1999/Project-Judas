#!/usr/bin/env python3
import pathlib,shlex,subprocess,hashlib,shutil,os,time,json
r=pathlib.Path.cwd();base=r/'.cache/m78-followup/release';owned=r/'.cache/m78-cold-boundary/row-journal';ev=r/'docs/evidence/m78-cold-boundary';rows=[]
h=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
def flags(target):
 d={}
 for line in (base/f'CMakeFiles/{target}.dir/flags.make').read_text().splitlines():
  if ' = 'in line:k,v=line.split(' = ',1);d[k]=shlex.split(v)
 return d
for name,target in [('JointSolver.cpp','judas_engine'),('BalanceLabBoundaryObserver.cpp','judas_balance_lab_tests')]:
 d=flags(target);cmd=['c++',*d['CXX_DEFINES'],*d['CXX_INCLUDES'],'-I'+str(r/'third_party'),*d['CXX_FLAGS'],'-c',str(owned/name),'-o',str(owned/(name+'.o'))];t=time.perf_counter();p=subprocess.run(cmd,text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT);(ev/('journal-'+name+'-compile.log')).write_text(p.stdout);rows.append(dict(command=cmd,exitCode=p.returncode,seconds=time.perf_counter()-t));assert p.returncode==0,p.stdout;print('compiled',name,flush=True)
archive=owned/'libjudas_engine.a';shutil.copyfile(base/'libjudas_engine.a',archive);subprocess.run(['ar','r',str(archive),str(owned/'JointSolver.cpp.o')],check=True)
cmd=shlex.split((base/'CMakeFiles/judas_balance_lab_tests.dir/link.txt').read_text());cmd=[x for x in cmd if not x.startswith('-Wl,--dependency-file=')];cmd[cmd.index('CMakeFiles/judas_balance_lab_tests.dir/tests/BalanceLabTests.cpp.o')]=str(owned/'BalanceLabBoundaryObserver.cpp.o');cmd[cmd.index('libjudas_engine.a')]=str(archive);cmd[cmd.index('-o')+1]=str(owned/'observer');t=time.perf_counter();p=subprocess.run(cmd,cwd=base,text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT);(ev/'journal-link.log').write_text(p.stdout);rows.append(dict(command=cmd,exitCode=p.returncode,seconds=time.perf_counter()-t));assert p.returncode==0,p.stdout
for name,mode in [('source','cold-write'),('native-restore','cold-read')]:
 out=owned/name
 if mode=='cold-read':
  (out/'slots').mkdir(parents=True);shutil.copytree(r/'projects/balance_lab',out/'project');shutil.copyfile(r/'docs/evidence/m78-continuation/repro/m77-native-in-progress-step-failed.save',out/'slots/m77.save')
 e=os.environ.copy();e.update(SDL_VIDEODRIVER='offscreen',SDL_AUDIODRIVER='dummy',LIBGL_ALWAYS_SOFTWARE='1',JUDAS_DIAGNOSTIC_CONTINUE_WRITE='1',XDG_DATA_HOME=str(owned/'data'),XDG_CONFIG_HOME=str(owned/'config'))
 cmd=[str(owned/'observer'),str(out),mode];t=time.perf_counter();p=subprocess.run(cmd,env=e,text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT);(ev/('journal-'+name+'-run.log')).write_text(p.stdout);rows.append(dict(command=cmd,exitCode=p.returncode,seconds=time.perf_counter()-t,saveSHA256=h(out/'slots/m77.save')));print(name,p.returncode);print('\n'.join(x for x in p.stdout.splitlines()if 'STATE_OBSERVER' in x or 'ACTOR 12' in x));shutil.copyfile(out/'joint-boundary.jsonl',ev/('linux-'+name+'-joint-boundary.jsonl'))
identity=json.loads((owned/'IDENTITY.json').read_text());identity.update(steps=rows,exeSHA256=h(owned/'observer'),archiveSHA256=h(archive));(ev/'linux-journal-isolation.json').write_text(json.dumps(identity,indent=2)+'\n')
