from pathlib import Path
import subprocess,json,time
r=Path.cwd();o=r/'.cache/prealpha-final-qualification';s=o/'source'
assert subprocess.check_output(['git','rev-parse','HEAD'],cwd=s,text=True).strip()=='c76e92dc20b5765f61d6077bf618a8b94ac22462'
assert not subprocess.check_output(['git','status','--porcelain'],cwd=s,text=True).strip()
targets=['mk2_physics','mk2_rotation','mk2_rotation_cold','mk2_diagnostics','joint','wheel','wheel_lifecycle','powertrain','powertrain_lifecycle','character_motor','physical_balance_controller','physics_continuation','contact_lifecycle','prealpha_lifecycle','origin_assembly','origin_admission','stream_origin_precision','living_world_streaming','save_evolution','save_exclusion','network_session','network_boundary','project_water','project_fluid_policy','basic_water_application','input','spacecraft_control','spacecraft_flight','player_swimming','gravity_selection','impact_mesh_cost','hearthworld_application','balance_lab']
# Manifest-backed names are checked against the generated build before execution.
cmd=['python3',str(s/'tools/build_linux_baseline.py'),'--rootfs',str(r/'.cache/prealpha-elastic-linux/ubuntu24'),'--build',str(o/'build24'),'--jobs','2']
for t in targets:cmd+=['--extra-target','judas_'+t+'_tests']
(o/'BUILD_COMMAND.json').write_text(json.dumps(cmd,indent=2)+'\n');start=time.monotonic()
with (o/'build-wrapper.log').open('w') as f:code=subprocess.run(cmd,stdout=f,stderr=subprocess.STDOUT).returncode
(o/'BUILD_RESULT.json').write_text(json.dumps({'exit':code,'seconds':time.monotonic()-start})+'\n');print('Build exit',code,flush=True)
