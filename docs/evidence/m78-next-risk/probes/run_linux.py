#!/usr/bin/env python3
"""Serial real Application probes, exact current archive, fresh isolated roots."""
import os,json,hashlib,shlex,subprocess,time
from pathlib import Path
R=Path.cwd();B=R/'.cache/m78-followup/release';C=R/'.cache/m78-next-risk';E=R/'docs/evidence/m78-next-risk'
C.mkdir(exist_ok=False);(E/'runs').mkdir(exist_ok=False)
archive=R/'.cache/m78-lifecycle/lif003-build/libjudas_engine.a';rows=[];binaries={}
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
env=dict(os.environ,SDL_VIDEODRIVER='offscreen',SDL_AUDIODRIVER='dummy',LIBGL_ALWAYS_SOFTWARE='1',JUDAS_ENGINE_ROOT=str(R),XDG_DATA_HOME=str(C/'data'),XDG_CONFIG_HOME=str(C/'config'))
for k in list(env):
 if k.startswith(('M78_DIAGNOSTIC_','JUDAS_DIAGNOSTIC_','JUDAS_BOUNDARY_')):del env[k]
def run(name,cmd,cwd=R,extra=None,timeout=180):
 started=time.monotonic();p=subprocess.run(cmd,cwd=cwd,env=dict(env,**(extra or {})),stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=timeout);log=E/'runs'/(name+'.log');log.write_text(p.stdout);row={'name':name,'command':list(map(str,cmd)),'cwd':str(cwd),'environment':{k:v for k,v in dict(env,**(extra or {})).items()if k in ['SDL_VIDEODRIVER','SDL_AUDIODRIVER','LIBGL_ALWAYS_SOFTWARE','JUDAS_ENGINE_ROOT','XDG_DATA_HOME','XDG_CONFIG_HOME']},'exitCode':p.returncode,'seconds':time.monotonic()-started,'logSHA256':sha(log),'summary':[s for s in p.stdout.splitlines()if 'SUMMARY'in s or ('checks'in s and ('failures'in s or 'integration:'in s))]};rows.append(row);(E/'RESULTS.json').write_text(json.dumps({'startingHEAD':subprocess.check_output(['git','rev-parse','HEAD'],cwd=R,text=True).strip(),'archiveSHA256':sha(archive),'binaries':binaries,'runs':rows},indent=2)+'\n');print(name,p.returncode,row['summary'][-2:],flush=True);return p.returncode
for target,source in [('judas_save_streaming_tests','tests/SaveStreamingTests.cpp'),('judas_mod_application_tests','tests/ModApplicationTests.cpp'),('judas_async_application_tests','tests/AsyncApplicationTests.cpp')]:
 flags={}
 for line in (B/f'CMakeFiles/{target}.dir/flags.make').read_text().splitlines():
  if ' = ' in line:k,v=line.split(' = ',1);flags[k]=shlex.split(v)
 obj=C/(target+'.o');cmd=['c++',*flags['CXX_DEFINES'],*flags['CXX_INCLUDES'],*flags['CXX_FLAGS'],'-c',str(R/source),'-o',str(obj)];assert run(target+'-compile',cmd)==0
 cmd=shlex.split((B/f'CMakeFiles/{target}.dir/link.txt').read_text());cmd=[str(obj)if x.endswith('/'+Path(source).name+'.o')else x.replace('CMakeFiles/'+target+'.dir/src/','CMakeFiles/judas.dir/src/')if x.endswith('.cpp.o')else x for x in cmd];cmd=[str(archive)if x=='libjudas_engine.a'else x for x in cmd if not x.startswith('-Wl,--dependency-file=')];exe=C/target;cmd[cmd.index('-o')+1]=str(exe);assert run(target+'-link',cmd,B)==0;binaries[target]={'path':str(exe.relative_to(R)),'sha256':sha(exe),'source':source,'sourceSHA256':sha(R/source),'objectSHA256':sha(obj)}
# No old fixture is deleted: all test-owned relative paths resolve within this new cwd.
owned=C/'async-owned';owned.mkdir()
for case in ['stale-generation','project-switch','project-close','missing-recovery','shutdown-outstanding']:
 if run('async-'+case,[str(C/'judas_async_application_tests'),'--case',case,'--output',str(C/'async-results'/case)],owned):raise SystemExit('Preserved async failure; investigate before proceeding')
