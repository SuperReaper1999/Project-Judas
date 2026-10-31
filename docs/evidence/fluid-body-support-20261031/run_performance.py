from pathlib import Path
import subprocess,time,json,hashlib,datetime,platform
base=Path(__file__).resolve().parent;root=base.parents[1];out=base/'performance';out.mkdir(exist_ok=True)
build=root/'.cache/2d-stage1-build'
runs=[]
for pair in range(3):
 order=('baseline','current') if pair%2==0 else ('current','baseline')
 for kind in ('dry','wet'):
  for implementation in order:
   command=[str(base/('query-'+implementation)),str(base/('frozen-'+kind+'.txt'))]
   log=out/(kind+'-'+implementation+'-'+str(pair)+'.csv');start=time.monotonic()
   with log.open('w') as file:result=subprocess.run(command,cwd=root,stdout=file,stderr=subprocess.STDOUT)
   runs.append(dict(kind=kind,implementation=implementation,pair=pair,command=command,exit_code=result.returncode,seconds=time.monotonic()-start))
 for implementation in order:
  executable=base/'whole-step-baseline' if implementation=='baseline' else build/'judas_production_fluid_performance'
  prefix='whole-'+implementation+'-'+str(pair);resources=out/(prefix+'-resources.log')
  command=['/usr/bin/time','-v','-o',str(resources),str(executable),'--output',str(out/prefix)]
  start=time.monotonic()
  with (out/(prefix+'.log')).open('w') as file:result=subprocess.run(command,cwd=root,stdout=file,stderr=subprocess.STDOUT)
  runs.append(dict(kind='whole',implementation=implementation,pair=pair,command=command,exit_code=result.returncode,seconds=time.monotonic()-start))
 (out/'RUNS.json').write_text(json.dumps(runs,indent=2)+'\n');print('pair',pair,'complete',flush=True)
meta=dict(utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),host=platform.uname()._asdict(),note='No other agent compiler/native/functional workload during paired sampling; ordinary host background work not controlled.',inputs={name:hashlib.sha256((base/('frozen-'+name+'.txt')).read_bytes()).hexdigest() for name in ('dry','wet')})
(out/'ENVIRONMENT.json').write_text(json.dumps(meta,indent=2)+'\n')
