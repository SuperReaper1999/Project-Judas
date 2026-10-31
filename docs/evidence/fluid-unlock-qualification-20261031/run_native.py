import os,subprocess,time,json
from pathlib import Path
out=Path(__file__).resolve().parent;root=out.parents[1];b=root/'.cache/2d-stage1-build'
env=dict(os.environ,DISPLAY=':0',XDG_RUNTIME_DIR='/run/user/1000',JUDAS_WORLD_STATE='none',JUDAS_ENGINE_ROOT=str(root),JUDAS_TEST_SCRIPT=str(out/'long-scene.txt'))
results=[]
for name,scene in [('radial-long','terrain.judas'),('flat-long','terrain_flat_comparison.judas')]:
 command=[str(b/'judas'),str(root/'assets/scenes'/scene)];start=time.monotonic()
 with (out/(name+'.log')).open('w') as log:
  r=subprocess.run(command,cwd=root,env=env,stdout=log,stderr=subprocess.STDOUT,timeout=180)
 results.append(dict(name=name,command=command,exit_code=r.returncode,seconds=time.monotonic()-start))
 (out/'native.json').write_text(json.dumps(results,indent=2)+'\n');print(name,r.returncode,flush=True)
