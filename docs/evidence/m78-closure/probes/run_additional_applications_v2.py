#!/usr/bin/env python3
"""Selected actual Application save/streaming and signal lifetime routes; unchanged original fixtures."""
from pathlib import Path
import hashlib,json,os,shlex,subprocess,time
r=Path.cwd();base=r/'.cache/m78-followup/release';build=r/'.cache/m78-closure/final-build';out=r/'.cache/m78-closure/selected-applications-v2';ev=r/'docs/evidence/m78-closure/selected-applications-v2'
out.mkdir(parents=True,exist_ok=False);ev.mkdir(parents=True,exist_ok=False)
h=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
identity=json.loads((r/'docs/evidence/m78-closure/final-build/BUILD.json').read_text());archive=build/'libjudas_engine.a';assert h(archive)==identity['archive']['sha256']
env=dict(os.environ,SDL_VIDEODRIVER='offscreen',SDL_AUDIODRIVER='dummy',LIBGL_ALWAYS_SOFTWARE='1',XDG_DATA_HOME=str(out/'data'),XDG_CONFIG_HOME=str(out/'config'));rows=[];binaries={}
def run(name,cmd,cwd=r):
 start=time.perf_counter();p=subprocess.run(cmd,cwd=cwd,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=180);log=ev/(name+'.log');log.write_text(p.stdout);rows.append(dict(name=name,command=cmd,exitCode=p.returncode,seconds=time.perf_counter()-start,logSHA256=h(log),summary=[s for s in p.stdout.splitlines()if 'SUMMARY'in s]));(ev/'RESULTS.json').write_text(json.dumps(dict(scope=__doc__,archiveSHA256=h(archive),sourceSHA256=identity['sourceSHA256'],binaries=binaries,results=rows),indent=2)+'\n');print(name,p.returncode,rows[-1]['summary'][-1:],flush=True)
 return p.returncode
for target,source in [('judas_save_streaming_tests','tests/SaveStreamingTests.cpp'),('judas_signal_application_tests','tests/SignalApplicationTests.cpp')]:
 flags={}
 for line in (base/f'CMakeFiles/{target}.dir/flags.make').read_text().splitlines():
  if ' = ' in line:k,v=line.split(' = ',1);flags[k]=shlex.split(v)
 obj=out/(target+'.o');assert run(target+'-driver-compile',['c++',*flags['CXX_DEFINES'],*flags['CXX_INCLUDES'],*flags['CXX_FLAGS'],'-c',str(r/source),'-o',str(obj)])==0
 cmd=shlex.split((base/f'CMakeFiles/{target}.dir/link.txt').read_text());cmd=[str(obj)if x.endswith('/'+Path(source).name+'.o')else x.replace('CMakeFiles/'+target+'.dir/src/','CMakeFiles/judas.dir/src/')if x.endswith('.cpp.o')else x for x in cmd];cmd=[str(archive)if x=='libjudas_engine.a'else x for x in cmd if not x.startswith('-Wl,--dependency-file=')];exe=out/target;cmd[cmd.index('-o')+1]=str(exe);assert run(target+'-link',cmd,base)==0;binaries[target]=dict(path=str(exe.relative_to(r)),sha256=h(exe),fixtureSourceSHA256=h(r/source))
for mode in ['write','read','read-b']:
 run('save-streaming-'+mode,[str(out/'judas_save_streaming_tests'),str(r/'projects/streamed_range/streamed_range.judasproj'),mode,str(out/'save-case')])
run('signals',[str(out/'judas_signal_application_tests'),str(r/'projects/signals_lab/signals_lab.judasproj')])
run('signals-load',[str(out/'judas_signal_application_tests'),str(r/'projects/signals_lab/signals_lab.judasproj'),'load'])
