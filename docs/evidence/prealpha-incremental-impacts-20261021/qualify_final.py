from pathlib import Path
import subprocess,json,time,hashlib,os
root=Path.cwd();out=root/'.cache/contact-redesign-20261021/final-checks';out.mkdir(exist_ok=True)
targets=['judas_growing_impact_tests','judas_tracked_contact_rows_tests','judas_impact_solver_tests','judas_impact_timing_tests','judas_impact_robustness_tests','judas_support_continuation_tests','judas_collision_continuity_tests','judas_joint_tests','judas_kinematic_tests','judas_rigid_contact_tests','judas_character_motor_tests','judas_ragdoll_tests','judas_physics_continuation_tests','judas_contact_lifecycle_tests','judas_contact_cache_tests','judas_native_mesh_growth_tests','judas_collision_geometry_tests','judas_impact_event_schedule_tests','judas_origin_physics_tests','judas_origin_assembly_tests','judas_prealpha_lifecycle_tests']
with (out/'build.log').open('w')as f:
 r=subprocess.run(['cmake','--build','build','--target',*targets,'judas_editor','judas','judas_export','judas_character_lab_tests','judas_balance_lab_tests','judas_save_streaming_tests','-j2'],stdout=f,stderr=subprocess.STDOUT)
if r.returncode:raise SystemExit(r.returncode)
results=[]
for target in targets:
 binary=root/'build'/target;cmd=[str(binary)];start=time.monotonic()
 with(out/(target+'.log')).open('w')as f:r=subprocess.run(cmd,stdout=f,stderr=subprocess.STDOUT,timeout=180)
 results.append({'target':target,'argv':cmd,'exit':r.returncode,'seconds':time.monotonic()-start,'binary_sha256':hashlib.sha256(binary.read_bytes()).hexdigest()});(out/'RESULTS.json').write_text(json.dumps(results,indent=2)+'\n');print(target,r.returncode,flush=True)
print('failures',sum(x['exit']!=0 for x in results));raise SystemExit(int(any(x['exit']for x in results)))
