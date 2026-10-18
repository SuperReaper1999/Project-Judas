from pathlib import Path
import datetime,hashlib,json,os,platform,subprocess,time
root=Path.cwd();out=root/'.cache/prealpha-pass5/dense-linux';out.mkdir(exist_ok=False)
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
run('configure',['cmake','-S','.', '-B','build','-DCMAKE_BUILD_TYPE=Release','-DJUDAS_BUILD_CHANNEL=Experimental'])
run('build',['cmake','--build','build','--target',*targets,'-j3'],timeout=600)
record['build_info']=json.loads((root/'build/judas-build-info-Release.json').read_text());record['archive_sha256']=sha(root/'build/libjudas_engine.a')
assert source=={p:sha(root/p)for p in files},'Source changed during build'
for target in targets:run(target,[root/'build'/target])
run('missing_revision_negative',[root/'build/judas_impact_candidate_cache_tests','--omit-revision'],expected=1)
record['source_unchanged']=source=={p:sha(root/p)for p in files};record['finished_utc']=datetime.datetime.now(datetime.timezone.utc).isoformat();(out/'RESULTS.json').write_text(json.dumps(record,indent=2)+'\n')
