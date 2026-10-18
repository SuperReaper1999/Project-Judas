from pathlib import Path
import datetime,hashlib,json,os,platform,subprocess,time
root=Path.cwd();out=root/'.cache/prealpha-pass5/dense-production';out.mkdir(exist_ok=False)
targets=['judas_impact_candidate_cache_tests','judas_impact_timing_tests','judas_contact_cache_tests','judas_kinematic_tests','judas_physics_continuation_tests','judas_contact_lifecycle_tests','judas_character_motor_tests','judas_joint_tests','judas_particle_contact_batch_tests','judas_rigid_motion_tests','judas_impact_robustness_tests']
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
files=['CMakeLists.txt','tools/validation/suites.json','src/physics/ImpactCandidateCache.h','src/physics/WorldImpactEvents.cpp','tests/ImpactCandidateCacheTests.cpp','src/PhysicsWorld.h']
source={p:sha(root/p)for p in files};results=[]
record={'utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'head':subprocess.check_output(['git','rev-parse','HEAD'],text=True).strip(),'platform':platform.uname()._asdict(),'cpu':Path('/proc/cpuinfo').read_text().split('\n\n')[0],'source':source,'results':results}
def run(name,cmd,expected=0,timeout=180):
 log=out/(name+'.log');start=time.monotonic();row={'name':name,'argv':list(map(str,cmd)),'cwd':str(root),'expected_exit':expected,'utc':datetime.datetime.now(datetime.timezone.utc).isoformat()}
 with log.open('w')as f:
  try:r=subprocess.run(row['argv'],stdout=f,stderr=subprocess.STDOUT,timeout=timeout);row['exit']=r.returncode
  except subprocess.TimeoutExpired:row['exit']='timeout'
 row.update(seconds=time.monotonic()-start,log=log.name,log_sha256=sha(log));row['pass']=row['exit']==expected
 exe=Path(row['argv'][0]);row['binary_sha256']=sha(exe)if exe.is_file()else None;results.append(row);(out/'RESULTS.json').write_text(json.dumps(record,indent=2)+'\n');print(name,row['exit'],round(row['seconds'],3),flush=True)
 if not row['pass']:raise SystemExit(1)
run('build',['cmake','--build','build','--target','judas_physics_stress_live_benchmark','judas_physics_scaling_diagnostics','judas_impact_mesh_cost_tests','-j3'],timeout=600)
record['build_info']=json.loads((root/'build/judas-build-info-Release.json').read_text());record['archive_sha256']=sha(root/'build/libjudas_engine.a')
run('original_baseline_full',[root/'.cache/prealpha-pass5/baseline/judas_impact_mesh_cost_tests','60','','1.2'],expected=1)
run('production_full_preserved_failure',[root/'build/judas_impact_mesh_cost_tests','60','','1.2'],expected=1)
run('production_initial_arrival',[root/'build/judas_impact_mesh_cost_tests','60','','1.2','entry'])
run('workloads',['python3',root/'docs/evidence/prealpha-pass4/dense/reproduction/run_workloads.py','--live',root/'build/judas_physics_stress_live_benchmark','--scaling',root/'build/judas_physics_scaling_diagnostics','--output',out/'workloads'],timeout=180)
record['source_unchanged']=source=={p:sha(root/p)for p in files};assert record['source_unchanged'];record['finished_utc']=datetime.datetime.now(datetime.timezone.utc).isoformat();(out/'RESULTS.json').write_text(json.dumps(record,indent=2)+'\n')
