#!/usr/bin/env python3
"""Fresh-process save/stream composition and real modded Application/package ownership."""
from pathlib import Path
import os,json,hashlib,shlex,subprocess,time,shutil
R=Path.cwd();B=R/'.cache/m78-followup/release';C=R/'.cache/m78-next-risk';E=R/'docs/evidence/m78-next-risk';archive=R/'.cache/m78-lifecycle/lif003-build/libjudas_engine.a'
summary=json.loads((E/'RESULTS.json').read_text());rows=summary['runs'];binaries=summary['binaries'];sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
env=dict(os.environ,SDL_VIDEODRIVER='offscreen',SDL_AUDIODRIVER='dummy',LIBGL_ALWAYS_SOFTWARE='1',JUDAS_ENGINE_ROOT=str(R),XDG_DATA_HOME=str(C/'data'),XDG_CONFIG_HOME=str(C/'config'))
for k in list(env):
 if k.startswith(('M78_DIAGNOSTIC_','JUDAS_DIAGNOSTIC_','JUDAS_BOUNDARY_')):del env[k]
def run(name,cmd,cwd=R,extra=None,timeout=180):
 log=E/'runs'/(name+'.log');assert not log.exists(),name
 started=time.monotonic();p=subprocess.run(cmd,cwd=cwd,env=dict(env,**(extra or {})),stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=timeout);log.write_text(p.stdout);row={'name':name,'command':list(map(str,cmd)),'cwd':str(cwd),'environment':{k:v for k,v in dict(env,**(extra or {})).items()if k in ['SDL_VIDEODRIVER','SDL_AUDIODRIVER','LIBGL_ALWAYS_SOFTWARE','JUDAS_ENGINE_ROOT','XDG_DATA_HOME','XDG_CONFIG_HOME']},'exitCode':p.returncode,'seconds':time.monotonic()-started,'logSHA256':sha(log),'summary':[s for s in p.stdout.splitlines()if 'SUMMARY'in s or ('checks'in s and ('failures'in s or 'integration:'in s))]};rows.append(row);summary.update(binaries=binaries,runs=rows);(E/'RESULTS.json').write_text(json.dumps(summary,indent=2)+'\n');print(name,p.returncode,row['summary'][-2:],flush=True);return p.returncode
source=R/'docs/evidence/m78-next-risk/probes/ComposedStreamSavePhysical.cpp';target='judas_save_streaming_tests';flags={}
for line in (B/f'CMakeFiles/{target}.dir/flags.make').read_text().splitlines():
 if ' = 'in line:k,v=line.split(' = ',1);flags[k]=shlex.split(v)
obj=C/'composed-physical.o';assert run('composed-physical-compile',['c++','-I'+str(R/'tests'),*flags['CXX_DEFINES'],*flags['CXX_INCLUDES'],*flags['CXX_FLAGS'],'-c',str(source),'-o',str(obj)])==0
cmd=shlex.split((B/f'CMakeFiles/{target}.dir/link.txt').read_text());cmd=[str(obj)if x.endswith('/SaveStreamingTests.cpp.o')else x.replace('CMakeFiles/'+target+'.dir/src/','CMakeFiles/judas.dir/src/')if x.endswith('.cpp.o')else x for x in cmd];cmd=[str(archive)if x=='libjudas_engine.a'else x for x in cmd if not x.startswith('-Wl,--dependency-file=')];exe=C/'composed-physical';cmd[cmd.index('-o')+1]=str(exe);assert run('composed-physical-link',cmd,B)==0;binaries['composed-physical']={'path':str(exe.relative_to(R)),'sha256':sha(exe),'source':str(source.relative_to(R)),'sourceSHA256':sha(source),'objectSHA256':sha(obj)}
project=C/'stream-fixture-03/fixture/project/streamed_range.judasproj'
for mode in ['write','read','read-b']:
 if run('composed-physical-'+mode,[str(exe),str(project),mode,str(C/'stream-save-physical')]):raise SystemExit('Preserved composed failure')
