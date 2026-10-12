#!/usr/bin/env python3
from pathlib import Path
import hashlib,json,os,shlex,shutil,subprocess,time
r=Path.cwd();base=r/'.cache/m78-followup/release';o=r/'.cache/m78-cold-boundary/phased-publication';ev=r/'docs/evidence/m78-cold-boundary/phased-publication';records=[]
def call(cmd,cwd=r,name='command',env=None):
 t=time.perf_counter();p=subprocess.run(cmd,cwd=cwd,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=240);(ev/(name+'.log')).write_text(p.stdout);records.append({'name':name,'command':cmd,'exitCode':p.returncode,'seconds':time.perf_counter()-t});print(name,p.returncode,flush=True);return p
flags={}
for line in (base/'CMakeFiles/judas_engine.dir/flags.make').read_text().splitlines():
 if ' = 'in line:k,v=line.split(' = ',1);flags[k]=shlex.split(v)
cmd=['c++',*flags['CXX_DEFINES'],*flags['CXX_INCLUDES'],*flags['CXX_FLAGS'],'-c',str(o/'PhysicalBalance.cpp'),'-o',str(o/'PhysicalBalance.cpp.o')];assert call(cmd,name='compile').returncode==0
archive=o/'libjudas_engine.a';shutil.copyfile(base/'libjudas_engine.a',archive);assert call(['ar','r',str(archive),str(o/'PhysicalBalance.cpp.o')],name='archive').returncode==0
for target in ['judas_balance_lab_tests','judas_physical_balance_controller_tests']:
 cmd=shlex.split((base/f'CMakeFiles/{target}.dir/link.txt').read_text());cmd=[x for x in cmd if not x.startswith('-Wl,--dependency-file=')];cmd[cmd.index('libjudas_engine.a')]=str(archive);cmd[cmd.index('-o')+1]=str(o/target);assert call(cmd,cwd=base,name='link-'+target).returncode==0
ex=o/'judas_balance_lab_tests';env=os.environ.copy();env.update(SDL_VIDEODRIVER='offscreen',SDL_AUDIODRIVER='dummy',LIBGL_ALWAYS_SOFTWARE='1',XDG_DATA_HOME=str(o/'data'),XDG_CONFIG_HOME=str(o/'config'))
call([str(o/'judas_physical_balance_controller_tests')],name='controller',env=env)
exact=o/'exact-native';(exact/'slots').mkdir();shutil.copytree(r/'projects/balance_lab',exact/'project');shutil.copyfile(r/'docs/evidence/m78-continuation/repro/m77-native-in-progress-step-failed.save',exact/'slots/m77.save')
call([str(ex),str(exact),'cold-read'],name='exact-native-cold-read',env=env)
call([str(ex),str(o/'ordinary-step'),'step-trace'],name='ordinary-step',env=env)
for mode in ['cold-stand-write','cold-stand-read','cold-recovery-write','cold-recovery-read']:
 label='standing' if 'stand' in mode else 'recovery';call([str(ex),str(o/label),mode],name=mode,env=env)
(ev/'RESULTS.json').write_text(json.dumps({'scope':'Copied first-restored-phase mechanism diagnostic only; originaltests,gains,thresholds,cadence,budgetsunchanged; no productionchanges.','records':records,'archiveSHA256':hashlib.sha256(archive.read_bytes()).hexdigest(),'exeSHA256':hashlib.sha256(ex.read_bytes()).hexdigest()},indent=2)+'\n')
