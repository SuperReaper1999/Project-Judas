#!/usr/bin/env python3
"""Narrow rerun of reviewed proper guards and actual game sample parity."""
import pathlib,shlex,subprocess,hashlib,shutil,os,time,json
r=pathlib.Path.cwd();base=r/'.cache/m78-followup/release';owned=r/'.cache/m78-cold-boundary/proposed-continuation';src=owned/'src';ev=r/'docs/evidence/m78-cold-boundary';receipts=[]
h=lambda p:hashlib.sha256(p.read_bytes()).hexdigest();flags={}
for line in(base/'CMakeFiles/judas_engine.dir/flags.make').read_text().splitlines():
 if ' = 'in line:k,v=line.split(' = ',1);flags[k]=shlex.split(v)
def run(cmd,name,**kw):
 t=time.perf_counter();p=subprocess.run(cmd,text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,**kw);(ev/('reviewed-continuation-'+name+'.log')).write_text(p.stdout);receipts.append(dict(command=cmd,exitCode=p.returncode,seconds=time.perf_counter()-t));return p
p=run(['c++','-I'+str(src),*flags['CXX_DEFINES'],*flags['CXX_INCLUDES'],*flags['CXX_FLAGS'],'-c',str(src/'PhysicsWorld.cpp'),'-o',str(owned/'PhysicsWorld.cpp.o')],'PW-compile');print('PW',p.returncode,flush=True);assert p.returncode==0,p.stdout
archive=owned/'libjudas_engine-reviewed.a';shutil.copyfile(base/'libjudas_engine.a',archive);subprocess.run(['ar','r',str(archive),*[str(owned/(x+'.o'))for x in ['WorldPersistence.cpp','PhysicsWorld.cpp','PhysicalBalance.cpp']]],check=True)
link=shlex.split((base/'CMakeFiles/judas_balance_lab_tests.dir/link.txt').read_text());link=[x for x in link if not x.startswith('-Wl,--dependency-file=')];link[link.index('libjudas_engine.a')]=str(archive)
for source,exe in [(r/'tests/PhysicsContinuationTests.cpp','focused-reviewed'),(owned/'SampleParityObserver.cpp','sample-parity')]:
 p=run(['c++','-I'+str(src),*flags['CXX_DEFINES'],*flags['CXX_INCLUDES'],'-I'+str(r/'third_party'),*flags['CXX_FLAGS'],'-c',str(source),'-o',str(owned/(source.name+'.o'))],exe+'-compile');assert p.returncode==0,p.stdout
 cmd=link.copy();cmd[cmd.index('CMakeFiles/judas_balance_lab_tests.dir/tests/BalanceLabTests.cpp.o')]=str(owned/(source.name+'.o'));cmd[cmd.index('-o')+1]=str(owned/exe);p=run(cmd,exe+'-link',cwd=base);assert p.returncode==0,p.stdout
cmd=link.copy();cmd[cmd.index('-o')+1]=str(owned/'original-reviewed');p=run(cmd,'original-link',cwd=base);assert p.returncode==0,p.stdout
env=os.environ.copy();env.update(SDL_VIDEODRIVER='offscreen',SDL_AUDIODRIVER='dummy',LIBGL_ALWAYS_SOFTWARE='1',XDG_DATA_HOME=str(owned/'data-reviewed'),XDG_CONFIG_HOME=str(owned/'config-reviewed'))
for k in list(env):
 if k.startswith('JUDAS_BOUNDARY_')or k.startswith('JUDAS_DIAGNOSTIC_')or k.startswith('M78_DIAGNOSTIC_'):del env[k]
p=run([str(owned/'focused-reviewed')],'focused-unit',env=env);print('focused',p.returncode,p.stdout,flush=True)
for mode in ['cold-write','cold-read']:
 out=owned/'reviewed-new-capture';p=run([str(owned/'original-reviewed'),str(out),mode],mode,env=env);receipts[-1]['saveSHA256']=h(out/'slots/m77.save');print(mode,p.returncode,flush=True);print('\n'.join(x for x in p.stdout.splitlines()if'ACTOR 12'in x or'SUMMARY'in x or'FAIL'in x),flush=True)
for mode in ['cold-write','cold-read']:
 out=owned/'reviewed-sample-parity';observed=env.copy();observed['JUDAS_DIAGNOSTIC_CONTINUE_WRITE']='1';p=run([str(owned/'sample-parity'),str(out),mode],'samples-'+mode,env=observed);print('samples',mode,p.returncode,flush=True)
 for path in out.glob('physical-*.json'):shutil.copyfile(path,ev/('linux-reviewed-samples-'+mode+'-'+path.name))
 for path in out.glob('diagnostic-*.jsonl'):shutil.copyfile(path,ev/('linux-reviewed-samples-'+mode+'-'+path.name))
 receipts[-1]['saveSHA256']=h(out/'slots/m77.save')
identity=json.loads((owned/'IDENTITY-reviewed.json').read_text());identity.update(steps=receipts,archiveSHA256=h(archive),focusedSourceSHA256=h(r/'tests/PhysicsContinuationTests.cpp'),observerSourceSHA256=h(owned/'SampleParityObserver.cpp'));(ev/'linux-reviewed-continuation-isolation.json').write_text(json.dumps(identity,indent=2)+'\n')
