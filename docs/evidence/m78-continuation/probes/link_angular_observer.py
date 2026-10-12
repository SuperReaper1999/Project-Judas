#!/usr/bin/env python3
import pathlib,shlex,subprocess,json,hashlib,sys
r=pathlib.Path.cwd();b=r/'.cache/m78-followup/release';ev=r/'docs/evidence/m78-continuation'
probe=sys.argv[1];target='judas_physical_balance_controller_tests'
source=ev/'probes'/(probe+'.cpp');h=lambda p:hashlib.sha256(p.read_bytes()).hexdigest();flags={}
for line in (b/'CMakeFiles'/ (target+'.dir')/'flags.make').read_text().splitlines():
 if ' = ' in line:
  k,v=line.split(' = ',1);flags[k]=shlex.split(v)
for variant in sys.argv[2:]:
 folder=r/'.cache/m78-continuation/variants'/variant
 obj=folder/(probe+'.o');exe=folder/probe;records=[]
 command=['c++',*flags.get('CXX_DEFINES',[]),'-I'+str(folder),*flags.get('CXX_INCLUDES',[]),*flags.get('CXX_FLAGS',[]),'-c',str(source),'-o',str(obj)]
 link=shlex.split((b/'CMakeFiles'/ (target+'.dir')/'link.txt').read_text())
 link=[str(obj) if x.endswith('PhysicalBalanceControllerTests.cpp.o') else str(folder/'libjudas_engine.a') if x=='libjudas_engine.a' else str(exe) if x==target else x for x in link if not x.startswith('-Wl,--dependency-file')]
 for stage,cmd in [('compile',command),('link',link)]:
  log=ev/(variant+'-'+probe+'-'+stage+'.log')
  if log.exists():raise RuntimeError('Preserve prior '+str(log))
  with log.open('wb') as f:result=subprocess.run(cmd,cwd=b,stdout=f,stderr=subprocess.STDOUT)
  records.append({'command':cmd,'cwd':str(b),'exit':result.returncode});assert result.returncode==0
 (ev/(variant+'-'+probe+'-build.json')).write_text(json.dumps({'probe':probe,'variant':variant,'builds':records,'sourceSHA256':h(source),'engineSHA256':h(folder/'libjudas_engine.a'),'exeSHA256':h(exe)},indent=2)+'\n')
 print(variant,probe,flush=True)
