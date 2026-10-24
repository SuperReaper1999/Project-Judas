from pathlib import Path
import subprocess,hashlib,json,time,datetime,shutil,os
source=Path('/source');build=Path('/build');out=build/'qualification';out.mkdir(exist_ok=False)
env=dict(os.environ,SDL_VIDEODRIVER='x11',SDL_AUDIODRIVER='dummy',LIBGL_ALWAYS_SOFTWARE='1',JUDAS_WORLD_STATE='none')
results=[]
def run(name,target,args=()):
 cmd=[str(build/('judas_'+target+'_tests')),*map(str,args)];start=time.monotonic()
 with (out/(name+'.log')).open('w') as f:
  try:code=subprocess.run(cmd,cwd=source,env=env,stdout=f,stderr=subprocess.STDOUT,timeout=180).returncode
  except subprocess.TimeoutExpired:code='timeout'
 record=dict(name=name,command=cmd,exit=code,seconds=time.monotonic()-start,utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),binary_sha256=hashlib.sha256(Path(cmd[0]).read_bytes()).hexdigest())
 results.append(record);(out/'RESULTS.json').write_text(json.dumps(results,indent=2)+'\n');print(name,code,flush=True)
for t in ['mk2_physics','mk2_rotation','mk2_diagnostics','joint','wheel','wheel_lifecycle','powertrain','powertrain_lifecycle','character_motor','physical_balance_controller','physics_continuation','contact_lifecycle','prealpha_lifecycle','origin_assembly','origin_admission','stream_origin_precision','project_water','project_fluid_policy','input','spacecraft_control','spacecraft_flight','player_swimming','player_curved_locomotion']:
 run(t,t)
for name,target,args in [
 ('rotation-cold','mk2_rotation_cold',[out/'rotation-cold']),
 ('terrain-streaming','living_world_streaming',[out/'terrain-streaming']),
 ('save-evolution','save_evolution',[out/'save-evolution']),
 ('save-exclusion','save_exclusion',[out/'save-exclusion',source]),
 ('network-session','network_session',[out/'network-session']),
 ('network-boundary','network_boundary',[out/'network-boundary']),
 ('gravity-selection','gravity_selection',[out/'gravity-selection']),
 ('mesh-continuation','impact_mesh_cost',['60','','1.2']),
 ('spacecraft-session','spacecraft_frame_integration',['--output',out/'spacecraft-session']),
 ('modern-policy-boundary','boundary_migration',[]),
 ('basic-water','basic_water_application',['--profile',out/'water']),
 ('basic-water-cold','basic_water_application',['--cold-cycle',out/'water-cold']),
 ('m77-write','balance_lab',[out/'m77','cold-write']),
 ('m77-read','balance_lab',[out/'m77','cold-read'])]:run(name,target,args)
# Consumer project/save roots are owned copies; accepted projects stay read-only.
owned=build/'owned';owned.mkdir(exist_ok=True)
for project in ('hearthworld','powertrain_lab'):shutil.copytree(source/'projects'/project,owned/project,ignore=shutil.ignore_patterns('Saves','.cache'))
for name,target,args in [
 ('hearthworld-write','hearthworld_application',[owned/'hearthworld/hearthworld.judasproj','write',out/'hearthworld']),
 ('hearthworld-read','hearthworld_application',[owned/'hearthworld/hearthworld.judasproj','read',out/'hearthworld']),
 ('powertrain-write','powertrain_application',[owned/'powertrain_lab/powertrain_lab.judasproj','write',out/'powertrain']),
 ('powertrain-read','powertrain_application',[owned/'powertrain_lab/powertrain_lab.judasproj','read',out/'powertrain'])]:run(name,target,args)
print('TOTAL',len(results),'FAILED',sum(x['exit']!=0 for x in results),flush=True)
