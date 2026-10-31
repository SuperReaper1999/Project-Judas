import subprocess,json,time,hashlib,platform
from pathlib import Path
root=Path(__file__).resolve().parents[2]; out=Path(__file__).resolve().parent; build=root/'.cache/2d-stage1-build'
checks=[('hydrostatics', ['judas_fluid_hydrostatics_tests']),('world',['judas_fluid_world_tests']),('boundary',['judas_fluid_boundary_contact_tests']),('rigid',['judas_fluid_rigid_coupling_tests']),('swimming',['judas_player_swimming_tests']),('project-water',['judas_project_water_tests']),('policy',['judas_project_fluid_policy_tests'])]
results=[]
for name,args in checks:
 command=[str(build/args[0]),*args[1:]]; start=time.monotonic()
 with (out/(name+'.log')).open('w') as log:
  try:r=subprocess.run(command,cwd=root,stdout=log,stderr=subprocess.STDOUT,timeout=180); code=r.returncode
  except subprocess.TimeoutExpired:code=124
 results.append(dict(name=name,command=command,exit_code=code,seconds=time.monotonic()-start,binary_sha256=hashlib.sha256(Path(command[0]).read_bytes()).hexdigest()))
 (out/'checks.json').write_text(json.dumps(results,indent=2)+'\n')
 print(name,code,flush=True)
