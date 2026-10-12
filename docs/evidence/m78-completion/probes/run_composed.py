#!/usr/bin/env python3
"""Serial current-archive probes; retain every log and reject reused output roots."""
import subprocess,os,json,shlex,hashlib,time
from pathlib import Path
R=Path.cwd(); B=R/'build'; E=R/'docs/evidence/m78-completion'; C=R/'.cache/m78-completion'
C.mkdir(parents=True,exist_ok=True);(E/'runs').mkdir(parents=True,exist_ok=True)
sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
env=dict(os.environ,SDL_VIDEODRIVER='offscreen',SDL_AUDIODRIVER='dummy',JUDAS_ENGINE_ROOT=str(R))
report=json.loads((E/'RESULTS.json').read_text())
def run(name,cmd,cwd=R,timeout=180):
 log=E/'runs'/(name+'.log');assert not log.exists(),name
 t=time.monotonic();p=subprocess.run(cmd,cwd=cwd,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=timeout);log.write_text(p.stdout);row={'name':name,'command':list(map(str,cmd)),'cwd':str(cwd),'exitCode':p.returncode,'seconds':time.monotonic()-t,'logSHA256':sha(log),'summary':[x for x in p.stdout.splitlines()if 'SUMMARY' in x]};report['runs'].append(row);(E/'RESULTS.json').write_text(json.dumps(report,indent=2)+'\n');print(name,p.returncode,row['summary'],flush=True);return p.returncode
flags={}
for line in (B/'CMakeFiles/judas_impact_robustness_tests.dir/flags.make').read_text().splitlines():
 if ' = 'in line:k,v=line.split(' = ',1);flags[k]=shlex.split(v)
name='ComposedPhysicalCold';source=E/'probes'/(name+'.cpp');obj=C/(name+'.o');exe=C/name
assert run(name+'-v5-compile',['c++',*flags['CXX_DEFINES'],*flags['CXX_INCLUDES'],*flags['CXX_FLAGS'],'-c',str(source),'-o',str(obj)])==0
cmd=shlex.split((B/'CMakeFiles/judas_impact_robustness_tests.dir/link.txt').read_text());cmd=[str(obj)if x.endswith('/ImpactRobustnessTests.cpp.o')else x for x in cmd if not x.startswith('-Wl,--dependency-file=')];cmd[cmd.index('-o')+1]=str(exe);assert run(name+'-v5-link',cmd,B)==0
report['binaries'][name]={'sha256':sha(exe),'sourceSHA256':sha(source),'objectSHA256':sha(obj)}
for mode in ['write','read']:
 if run(name+'-v5-'+mode,[str(exe),mode,str(C/'composed-v5-fixture')]):break
