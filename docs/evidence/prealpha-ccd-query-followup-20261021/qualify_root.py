from pathlib import Path
import json,subprocess,hashlib,shlex,time,datetime,os
r=Path(__file__).resolve().parents[2];p=Path(__file__).resolve().parent;old=r/'.cache/physics-hitch-followup-20261021';out=p/'qualification-root';out.mkdir()
sha=lambda f:hashlib.sha256(Path(f).read_bytes()).hexdigest()
core=r/'build/libjudas_engine.a';frozen=sha(core)
plans=json.loads((old/'qualification/RESULTS.json').read_text())['plans']
common=next(x['argv'] for x in plans if x['target']=='judas_native_mesh_growth_tests')
# Link every fresh fixture to the exact candidate engine, rather than compiling
# duplicate copies of world modules under the individual test-target flags.
common=[x for x in common if not x.endswith('.o')]
selected={'judas_rigid_contact_tests','judas_collision_geometry_tests','judas_support_guess_ownership_tests','judas_tracked_contact_rows_tests','judas_impact_event_schedule_tests','judas_parallel_narrowphase_tests','judas_impact_solver_tests','judas_impact_timing_tests','judas_impact_robustness_tests','judas_support_continuation_tests','judas_collision_continuity_tests','judas_growing_impact_tests','judas_native_mesh_growth_tests','judas_ragdoll_tests','judas_physics_continuation_tests','judas_contact_lifecycle_tests','judas_contact_cache_tests','judas_kinematic_tests','judas_character_motor_tests','judas_joint_tests','judas_origin_physics_tests','judas_wheel_tests','judas_powertrain_tests','judas_gravity_selection_tests'}
rows=[]
for plan in plans:
 if plan['target'] not in selected:continue
 name=plan['target'];folder=out/name;folder.mkdir();commands=[];objects=[]
 for c in plan['driver_compiles']:
  a=c['argv'][:];source=Path(a[a.index('-c')+1]);relative=source.relative_to(r)
  if relative.parts[0]=='src' and relative.as_posix()!='src/editor/EditorHistory.cpp':continue
  obj=folder/(source.name+'.o');a[a.index('-o')+1]=str(obj)
  with (folder/(source.name+'.compile.log')).open('w') as log:code=subprocess.run(a,cwd=r/'build',stdout=log,stderr=subprocess.STDOUT).returncode
  commands.append({'argv':a,'exit':code,'source_sha256':sha(source)})
  if code:raise RuntimeError('compile '+name)
  objects.append(str(obj))
 a=common[:];a[a.index('-o')+1]=str(folder/'test');a[a.index('-o'):a.index('-o')]=objects
 with (folder/'link.log').open('w') as log:code=subprocess.run(a,cwd=r/'build',stdout=log,stderr=subprocess.STDOUT).returncode
 commands.append({'argv':a,'exit':code})
 if code:raise RuntimeError('link '+name)
 begin=time.monotonic()
 with (folder/'run.log').open('w') as log:code=subprocess.run([folder/'test'],cwd=r,stdout=log,stderr=subprocess.STDOUT,timeout=180).returncode
 row={'target':name,'exit':code,'seconds':time.monotonic()-begin,'binary_sha256':sha(folder/'test'),'commands':commands,'utc':datetime.datetime.now(datetime.timezone.utc).isoformat()};rows.append(row)
 (out/'RESULTS.json').write_text(json.dumps({'core_sha256':frozen,'core_unchanged':sha(core)==frozen,'results':rows,'pass_all':all(x['exit']==0 for x in rows)},indent=2)+'\n');print(name,code,flush=True)
 if code:raise SystemExit(code)
