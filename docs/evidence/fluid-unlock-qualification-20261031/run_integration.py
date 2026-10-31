import subprocess,time,json,os,hashlib
from pathlib import Path
out=Path(__file__).resolve().parent; root=out.parents[1]; build=root/'.cache/2d-stage1-build'
checks=[('production', ['judas_production_fluid_tests','--output',str(out/'production')]),('player',['judas_player_fluid_integration_tests','--output',str(out/'player'),'--smoke']),('player-fall',['judas_player_fluid_integration_tests','--output',str(out/'player-fall'),'--fall-only']),('cavity',['judas_fluid_cavity_impact_tests']),('smoothing',['judas_fluid_smoothing_budget_tests']),('containers',['judas_fluid_container_integration_tests','--output',str(out/'containers'),'--supported-only'])]
results=[]
for name,args in checks:
 command=[str(build/args[0]),*args[1:]]; start=time.monotonic()
 with (out/(name+'.log')).open('w') as log:
  try:r=subprocess.run(command,cwd=root,env=dict(os.environ,JUDAS_ENGINE_ROOT=str(root)),stdout=log,stderr=subprocess.STDOUT,timeout=240); code=r.returncode
  except subprocess.TimeoutExpired:code=124
 results.append(dict(name=name,command=command,exit_code=code,seconds=time.monotonic()-start,binary_sha256=hashlib.sha256(Path(command[0]).read_bytes()).hexdigest()))
 (out/'integration.json').write_text(json.dumps(results,indent=2)+'\n'); print(name,code,flush=True)
