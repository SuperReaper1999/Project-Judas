#!/usr/bin/env python3
"""Original fixture/controller checks for copied completed-publication continuation."""
import pathlib,shlex,subprocess,hashlib,shutil,os,time,json
r=pathlib.Path.cwd();base=r/'.cache/m78-followup/release';owned=r/'.cache/m78-cold-boundary/new-capture-publication';ev=r/'docs/evidence/m78-cold-boundary';receipts=[]
h=lambda p:hashlib.sha256(p.read_bytes()).hexdigest();flags={}
for line in(base/'CMakeFiles/judas_engine.dir/flags.make').read_text().splitlines():
 if ' = 'in line:k,v=line.split(' = ',1);flags[k]=shlex.split(v)
def run(cmd,name,**kw):
 t=time.perf_counter();p=subprocess.run(cmd,text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,**kw);(ev/('new-capture-publication-'+name+'.log')).write_text(p.stdout);receipts.append(dict(command=cmd,exitCode=p.returncode,seconds=time.perf_counter()-t));return p
for name in ['WorldPersistence.cpp','PhysicsWorld.cpp','PhysicalBalance.cpp']:
 p=run(['c++',*flags['CXX_DEFINES'],*flags['CXX_INCLUDES'],*flags['CXX_FLAGS'],'-c',str(owned/name),'-o',str(owned/(name+'.o'))],name+'-compile');print(name,p.returncode,flush=True);assert p.returncode==0,p.stdout
archive=owned/'libjudas_engine.a';shutil.copyfile(base/'libjudas_engine.a',archive);subprocess.run(['ar','r',str(archive),*[str(owned/(x+'.o'))for x in ['WorldPersistence.cpp','PhysicsWorld.cpp','PhysicalBalance.cpp']]],check=True)
for target,exe in [('judas_balance_lab_tests','original-fixture'),('judas_physical_balance_controller_tests','original-controller')]:
 cmd=shlex.split((base/('CMakeFiles/'+target+'.dir/link.txt')).read_text());cmd=[x for x in cmd if not x.startswith('-Wl,--dependency-file=')];cmd[cmd.index('libjudas_engine.a')]=str(archive);cmd[cmd.index('-o')+1]=str(owned/exe);p=run(cmd,exe+'-link',cwd=base);assert p.returncode==0,p.stdout
env=os.environ.copy();env.update(SDL_VIDEODRIVER='offscreen',SDL_AUDIODRIVER='dummy',LIBGL_ALWAYS_SOFTWARE='1',M78_DIAGNOSTIC_PERSIST_DRIVE_IMPULSES='1',XDG_DATA_HOME=str(owned/'data'),XDG_CONFIG_HOME=str(owned/'config'))
for k in list(env):
 if k.startswith('JUDAS_BOUNDARY_')or k.startswith('JUDAS_DIAGNOSTIC_'):del env[k]
p=run([str(owned/'original-controller')],'controller',env=env);print('controller',p.returncode,flush=True);print(p.stdout[-1000:],flush=True)
for mode in ['cold-write','cold-read']:
 out=owned/'new-capture';p=run([str(owned/'original-fixture'),str(out),mode],mode,env=env);receipts[-1]['saveSHA256']=h(out/'slots/m77.save');print(mode,p.returncode,flush=True);print('\n'.join(x for x in p.stdout.splitlines()if 'ACTOR 12'in x or'SUMMARY'in x),flush=True);shutil.copyfile(out/'cold-write-state.json',ev/('linux-new-capture-publication-'+mode+'-state.json'))
old=owned/'legacy-a106';(old/'slots').mkdir(parents=True);shutil.copytree(r/'projects/balance_lab',old/'project');shutil.copyfile(r/'docs/evidence/m78-continuation/repro/m77-native-in-progress-step-failed.save',old/'slots/m77.save');p=run([str(owned/'original-fixture'),str(old),'cold-read'],'legacy-a106',env=env);receipts[-1]['saveSHA256']=h(old/'slots/m77.save');print('legacy-a106',p.returncode,flush=True)
identity=json.loads((owned/'IDENTITY.json').read_text());identity.update(steps=receipts,archiveSHA256=h(archive),exeSHA256=h(owned/'original-fixture'),controllerExeSHA256=h(owned/'original-controller'));(ev/'linux-new-capture-publication-isolation.json').write_text(json.dumps(identity,indent=2)+'\n')
