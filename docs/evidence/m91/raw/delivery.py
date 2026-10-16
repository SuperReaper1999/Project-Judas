from pathlib import Path
import subprocess,os,json,hashlib,shutil,time
root=Path.cwd();out=root/'.cache/m91/delivery';out.mkdir(exist_ok=False)
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
source={str(p):sha(p)for p in [root/'src/PhysicsWorld.cpp',root/'src/ContactGeometryInternal.h',root/'src/RigidMotionBounds.h']}
records=[];env=os.environ.copy();env.update(SDL_AUDIODRIVER='dummy',JUDAS_ENGINE_ROOT=str(root))
def run(name,cmd,cwd=root,environment=env,timeout=180):
 start=time.monotonic()
 with(out/(name+'.log')).open('w')as log:
  try:code=subprocess.run(list(map(str,cmd)),cwd=cwd,env=environment,stdout=log,stderr=subprocess.STDOUT,timeout=timeout).returncode
  except subprocess.TimeoutExpired:code='timeout'
 records.append({'name':name,'command':list(map(str,cmd)),'cwd':str(cwd),'exit':code,'seconds':time.monotonic()-start});(out/'RESULTS.json').write_text(json.dumps({'sources':source,'runs':records},indent=2)+'\n');print(name,code,flush=True)
 if code!=0:raise RuntimeError(name+' failed; logs retained')
run('cold',['python3','scripts/m87_cold_validation.py','--run','--output','.cache/m91/cold'])
run('application',[root/'build/judas_physics_scaling_application_tests',root/'projects/physics_stress_lab/physics_stress_lab.judasproj',out/'application'])
copy=out/'editor-project';shutil.copytree(root/'projects/physics_stress_lab',copy)
envEditor=env.copy();envEditor.update(JUDAS_EDITOR_AUTOTEST=str(out/'editor'),JUDAS_EDITOR_AUTOTEST_PROFILER='1',JUDAS_PROFILE='1',JUDAS_PROFILE_OUTPUT=str(out/'editor-profile.json'),XDG_CONFIG_HOME=str(out/'xdg-config'),JUDAS_WORLD_STATE='none')
run('editor',[root/'build/judas_editor',copy/'physics_stress_lab.judasproj'],environment=envEditor)
run('export',[root/'build/judas_export',root/'projects/physics_stress_lab/physics_stress_lab.judasproj',out/'package',root/'build/judas'])
moved=Path('/tmp/judas-m91-moved');unrelated=Path('/tmp/judas-m91-unrelated');shutil.copytree(out/'package',moved);unrelated.mkdir(exist_ok=True)
originalFiles={str(p.relative_to(out/'package')):sha(p)for p in(out/'package').rglob('*')if p.is_file()};movedFiles={str(p.relative_to(moved)):sha(p)for p in moved.rglob('*')if p.is_file()}
if originalFiles!=movedFiles:raise RuntimeError('Package copy differs before launch')
(out/'PACKAGE_FILES.json').write_text(json.dumps(originalFiles,indent=2)+'\n')
packageEnv=env.copy();packageEnv.pop('JUDAS_ENGINE_ROOT',None);packageEnv.update(JUDAS_TEST_SCRIPT=str(root/'.cache/m91/moved.script'),JUDAS_WORLD_STATE='none',JUDAS_PROFILE='1',JUDAS_PROFILE_OUTPUT=str(out/'package-profile.json'))
run('moved-package',[moved/'judas'],cwd=unrelated,environment=packageEnv)
result={'sources':source,'sources_unchanged':source=={p:sha(Path(p))for p in source},'runs':records,'moved_prelaunch_bytes_identical':originalFiles==movedFiles,'human_acceptance':False,'windows':'not run; nonblocking','pass':all(r['exit']==0 for r in records)}
(out/'RESULTS.json').write_text(json.dumps(result,indent=2)+'\n')
