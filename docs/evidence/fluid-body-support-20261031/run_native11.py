import subprocess,os,json,time
from pathlib import Path
base=Path(__file__).resolve().parent;root=base.parents[1];out=base/'native11';out.mkdir(exist_ok=True);b=root/'.cache/2d-stage1-build'
script=out/'tour.txt';script.write_text('FRAMES 1200\nFRAME_SECONDS 0.016666667\n')
original=(root/'assets/scenes/terrain.judas').read_text()
for height in (3,10):
 lines=original.splitlines();lake=False
 for i,line in enumerate(lines):
  if line.startswith('object '):lake=line.startswith('object 8 ')
  if lake and line.strip().startswith('position '):
   fields=line.split();fields[2]=str(float(fields[2])+height);lines[i]='  '+' '.join(fields);break
 (out/('terrain-plus'+str(height)+'.judas')).write_text('\n'.join(lines)+'\n')
cases=[('radial',root/'assets/scenes/terrain.judas'),('flat',root/'assets/scenes/terrain_flat_comparison.judas'),('raised3',out/'terrain-plus3.judas'),('raised10',out/'terrain-plus10.judas')]
results=[]
for name,scene in cases:
 env=dict(os.environ,DISPLAY=':0',XDG_RUNTIME_DIR='/run/user/1000',JUDAS_ENGINE_ROOT=str(root),JUDAS_WORLD_STATE='none',JUDAS_TEST_SCRIPT=str(script))
 command=[str(b/'judas'),str(scene)];start=time.monotonic()
 with (out/(name+'.log')).open('w') as log:
  result=subprocess.run(command,cwd=root,env=env,stdout=log,stderr=subprocess.STDOUT,timeout=180)
 results.append(dict(name=name,command=command,exit_code=result.returncode,seconds=time.monotonic()-start))
 (out/'RUNS.json').write_text(json.dumps(results,indent=2)+'\n');print(name,result.returncode,flush=True)
