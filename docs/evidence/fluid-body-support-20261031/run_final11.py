from pathlib import Path
import os,subprocess,json,time,hashlib
base=Path(__file__).resolve().parent;root=base.parents[1];out=base/'phase11';out.mkdir(exist_ok=True);b=root/'.cache/2d-stage1-build'
checks=[('production-final','judas_production_fluid_tests',['--output',str(out/'production-final')]),('player-final','judas_player_fluid_integration_tests',['--output',str(out/'player-final')]),('containers-final','judas_fluid_container_integration_tests',['--supported-only','--output',str(out/'containers-final')]),('cavity-final','judas_fluid_cavity_impact_tests',[]),('demo-pool-final','judas_fluid_demo_integration_tests',['--scene','pool','--output',str(out/'demo-pool-final')]),('demo-planet-final','judas_fluid_demo_integration_tests',['--scene','planet','--output',str(out/'demo-planet-final')])]
results=[]
for label,target,args in checks:
 start=time.monotonic();cmd=[str(b/target),*args]
 with (out/(label+'.log')).open('w') as log:
  try: result=subprocess.run(cmd,cwd=root,env=dict(os.environ,JUDAS_ENGINE_ROOT=str(root)),stdout=log,stderr=subprocess.STDOUT,timeout=240);code=result.returncode
  except subprocess.TimeoutExpired:code=124
 results.append(dict(name=label,command=cmd,exit_code=code,seconds=time.monotonic()-start,binary_sha256=hashlib.sha256((b/target).read_bytes()).hexdigest()))
 (out/'FINAL_RESULTS.json').write_text(json.dumps(results,indent=2)+'\n');print(label,code,flush=True)
