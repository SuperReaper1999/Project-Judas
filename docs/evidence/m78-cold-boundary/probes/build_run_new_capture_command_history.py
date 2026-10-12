#!/usr/bin/env python3
"""Unchanged original fixture, copied passive+command persistence splits."""
import pathlib, shlex, subprocess, hashlib, shutil, os, time, json
r=pathlib.Path.cwd();base=r/'.cache/m78-followup/release';owned=r/'.cache/m78-cold-boundary/new-capture-command-history';ev=r/'docs/evidence/m78-cold-boundary';receipts=[]
h=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
flags={}
for line in (base/'CMakeFiles/judas_engine.dir/flags.make').read_text().splitlines():
 if ' = 'in line:key,value=line.split(' = ',1);flags[key]=shlex.split(value)
def run(command,name,**kwargs):
 t=time.perf_counter();p=subprocess.run(command,text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,**kwargs);(ev/('new-capture-command-'+name+'.log')).write_text(p.stdout);receipts.append(dict(command=command,exitCode=p.returncode,seconds=time.perf_counter()-t));return p
for name in ['WorldPersistence.cpp','PhysicsWorld.cpp']:
 command=['c++',*flags['CXX_DEFINES'],*flags['CXX_INCLUDES'],*flags['CXX_FLAGS'],'-c',str(owned/name),'-o',str(owned/(name+'.o'))];p=run(command,name+'-compile');assert p.returncode==0,p.stdout
archive=owned/'libjudas_engine.a';shutil.copyfile(base/'libjudas_engine.a',archive);subprocess.run(['ar','r',str(archive),str(owned/'WorldPersistence.cpp.o'),str(owned/'PhysicsWorld.cpp.o')],check=True)
command=shlex.split((base/'CMakeFiles/judas_balance_lab_tests.dir/link.txt').read_text());command=[x for x in command if not x.startswith('-Wl,--dependency-file=')];command[command.index('libjudas_engine.a')]=str(archive);command[command.index('-o')+1]=str(owned/'original-fixture');p=run(command,'link',cwd=base);assert p.returncode==0,p.stdout
for variant,impulses in [('commands-no-impulses',False),('commands-with-impulses',True)]:
 out=owned/variant;env=os.environ.copy();env.update(SDL_VIDEODRIVER='offscreen',SDL_AUDIODRIVER='dummy',LIBGL_ALWAYS_SOFTWARE='1',M78_DIAGNOSTIC_PERSIST_DRIVE_IMPULSES='1'if impulses else'0',XDG_DATA_HOME=str(owned/'data'),XDG_CONFIG_HOME=str(owned/'config'))
 for k in list(env):
  if k.startswith('JUDAS_BOUNDARY_')or k.startswith('JUDAS_DIAGNOSTIC_'):del env[k]
 for mode in ['cold-write','cold-read']:
  p=run([str(owned/'original-fixture'),str(out),mode],variant+'-'+mode,env=env);receipts[-1].update(includeDriveImpulses=impulses,saveSHA256=h(out/'slots/m77.save'));print(variant,mode,p.returncode,flush=True);print('\n'.join(x for x in p.stdout.splitlines()if'ACTOR 12'in x or'SUMMARY'in x),flush=True);shutil.copyfile(out/'cold-write-state.json',ev/('linux-new-capture-'+variant+'-'+mode+'-state.json'))
identity=json.loads((owned/'IDENTITY.json').read_text());identity.update(steps=receipts,archiveSHA256=h(archive),exeSHA256=h(owned/'original-fixture'));(ev/'linux-new-capture-command-isolation.json').write_text(json.dumps(identity,indent=2)+'\n')
