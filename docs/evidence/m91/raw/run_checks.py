from pathlib import Path
import subprocess,json,hashlib,os,time,datetime
root=Path('/home/conner/Documents/GitHub/Project-Judas');out=root/'.cache/m91/native';out.mkdir(exist_ok=False)
names=['collision_continuity','contact_interval','rigid_motion_bounds','rigid_motion_geometry','rigid_motion','primitive_contact_boolean','contact_sat_workspace','collision_geometry','rigid_contact','contact_cache','contact_storage','contact_lifecycle','kinematic','physics_continuation','impact_solver','impact_timing','impact_robustness','impact_mesh_cost','sphere_impact_bounds','physics_scaling','parallel_narrowphase','joint_scaling','joint','character_motor','physical_balance_controller','post_consumer_ragdoll','origin_physics','wheel','powertrain','fluid_cavity_impact','mechanical_connection','section_material','deformable','fracture']
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
files=subprocess.check_output(['git','ls-files','src','CMakeLists.txt','tests'],cwd=root,text=True).splitlines()+['tests/CollisionContinuityTests.cpp','tests/ContactIntervalTests.cpp'];sources={n:sha(root/n)for n in sorted(set(files)) if(root/n).is_file()}
x={'utc_start':datetime.datetime.now(datetime.timezone.utc).isoformat(),'head':subprocess.check_output(['git','rev-parse','HEAD'],cwd=root,text=True).strip(),'source_fingerprints':sources,'engine_archive_sha256':sha(root/'build/libjudas_engine.a'),'human_acceptance':False,'platform':'native Linux','runs':[]}
env=os.environ.copy();env.update(SDL_AUDIODRIVER='dummy',JUDAS_ENGINE_ROOT=str(root))
for n in names:
 b=root/'build'/('judas_'+n+'_tests');record={'name':n,'command':[str(b)],'binary_sha256':sha(b)};start=time.monotonic()
 with(out/(n+'.log')).open('w')as log:
  try:record['exit']=subprocess.run([str(b)],cwd=root,env=env,stdout=log,stderr=subprocess.STDOUT,timeout=240).returncode
  except subprocess.TimeoutExpired:record['exit']='TIMEOUT'
 record.update(seconds=time.monotonic()-start,binary_unchanged=record['binary_sha256']==sha(b));x['runs'].append(record);(out/'RESULTS.json').write_text(json.dumps(x,indent=2)+'\n');print(n,record['exit'],round(record['seconds'],3),flush=True)
x['sources_unchanged']=sources=={n:sha(root/n)for n in sources};x['pass']=x['sources_unchanged'] and all(r['exit']==0 and r['binary_unchanged']for r in x['runs']);x['utc_end']=datetime.datetime.now(datetime.timezone.utc).isoformat();(out/'RESULTS.json').write_text(json.dumps(x,indent=2)+'\n')
raise SystemExit(0 if x['pass'] else 1)
