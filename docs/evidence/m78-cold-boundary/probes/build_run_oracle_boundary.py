#!/usr/bin/env python3
import pathlib,shlex,subprocess,hashlib,shutil,os,time,json
r=pathlib.Path.cwd();base=r/'.cache/m78-followup/release';owned=r/'.cache/m78-cold-boundary/oracle-boundary';ev=r/'docs/evidence/m78-cold-boundary';receipts=[]
h=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
d={}
for line in (base/'CMakeFiles/judas_engine.dir/flags.make').read_text().splitlines():
 if ' = 'in line:k,v=line.split(' = ',1);d[k]=shlex.split(v)
cmd=['c++',*d['CXX_DEFINES'],*d['CXX_INCLUDES'],'-I'+str(r/'third_party'),*d['CXX_FLAGS'],'-c',str(owned/'JointSolver.cpp'),'-o',str(owned/'JointSolver.cpp.o')];t=time.perf_counter();p=subprocess.run(cmd,text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT);(ev/'oracle-compile.log').write_text(p.stdout);receipts.append(dict(command=cmd,exitCode=p.returncode,seconds=time.perf_counter()-t));assert p.returncode==0,p.stdout
archive=owned/'libjudas_engine.a';shutil.copyfile(base/'libjudas_engine.a',archive);subprocess.run(['ar','r',str(archive),str(owned/'JointSolver.cpp.o')],check=True)
cmd=shlex.split((base/'CMakeFiles/judas_balance_lab_tests.dir/link.txt').read_text());cmd=[x for x in cmd if not x.startswith('-Wl,--dependency-file=')];cmd[cmd.index('CMakeFiles/judas_balance_lab_tests.dir/tests/BalanceLabTests.cpp.o')]=str(r/'.cache/m78-cold-boundary/row-journal/BalanceLabBoundaryObserver.cpp.o');cmd[cmd.index('libjudas_engine.a')]=str(archive);cmd[cmd.index('-o')+1]=str(owned/'observer');t=time.perf_counter();p=subprocess.run(cmd,cwd=base,text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT);(ev/'oracle-link.log').write_text(p.stdout);receipts.append(dict(command=cmd,exitCode=p.returncode,seconds=time.perf_counter()-t));assert p.returncode==0,p.stdout
for name,mode,discard in [('source','cold-write',False),('source-discard','cold-write',True),('native-restored','cold-read',False)]:
 out=owned/name
 if mode=='cold-read':
  (out/'slots').mkdir(parents=True);shutil.copytree(r/'projects/balance_lab',out/'project');shutil.copyfile(r/'docs/evidence/m78-continuation/repro/m77-native-in-progress-step-failed.save',out/'slots/m77.save')
 e=os.environ.copy();e.update(SDL_VIDEODRIVER='offscreen',SDL_AUDIODRIVER='dummy',LIBGL_ALWAYS_SOFTWARE='1',JUDAS_DIAGNOSTIC_CONTINUE_WRITE='1',JUDAS_BOUNDARY_ORACLE_FIRST_SOLVE='1',JUDAS_BOUNDARY_STEPS='1',XDG_DATA_HOME=str(owned/'data'),XDG_CONFIG_HOME=str(owned/'config'))
 if discard:e['JUDAS_BOUNDARY_DISCARD_FIRST_JOINT_SEEDS']='1'
 cmd=[str(owned/'observer'),str(out),mode];t=time.perf_counter();p=subprocess.run(cmd,env=e,text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT);(ev/('oracle-'+name+'-run.log')).write_text(p.stdout);receipts.append(dict(command=cmd,exitCode=p.returncode,seconds=time.perf_counter()-t,discard=discard,saveSHA256=h(out/'slots/m77.save')));print(name,p.returncode,flush=True);print('\n'.join(x for x in p.stdout.splitlines()if 'STATE_OBSERVER' in x or 'ACTOR 12' in x),flush=True);shutil.copyfile(out/'joint-boundary.jsonl',ev/('linux-oracle-'+name+'-joint-boundary.jsonl'))
identity=json.loads((owned/'IDENTITY.json').read_text());identity.update(steps=receipts,exeSHA256=h(owned/'observer'),archiveSHA256=h(archive));(ev/'linux-oracle-isolation.json').write_text(json.dumps(identity,indent=2)+'\n')
