from pathlib import Path
import subprocess,json,time,importlib.util,shutil
r=Path.cwd();o=r/'.cache/prealpha-final-qualification';b=o/'build24'
while not (o/'FOLLOWUP_BUILD_RESULT.json').exists():time.sleep(5)
assert json.loads((o/'FOLLOWUP_BUILD_RESULT.json').read_text())['exit']==0
s=(o/'run_regressions.py').read_text().replace("out=build/'qualification'","out=build/'qualification-followup'")
# Keep this a narrow affected rerun, not another full production suite.
s=s.replace("'origin_admission',",'').replace("'project_water',",'').replace("'project_fluid_policy',",'').replace("'input',",'').replace("'spacecraft_control',",'').replace("'spacecraft_flight',",'').replace("'player_curved_locomotion'", "'player_curved_locomotion'")
for line in [" ('terrain-streaming','living_world_streaming',[out/'terrain-streaming']),\n"," ('save-evolution','save_evolution',[out/'save-evolution']),\n"," ('save-exclusion','save_exclusion',[out/'save-exclusion',source]),\n"," ('network-boundary','network_boundary',[out/'network-boundary']),\n"," ('modern-policy-boundary','boundary_migration',[]),\n"," ('basic-water','basic_water_application',['--profile',out/'water']),\n"]:
 s=s.replace(line,'')
s=s.replace("owned=build/'owned'","owned=build/'owned-followup'")
(b/'run_followup.py').write_text(s)
spec=importlib.util.spec_from_file_location('baseline',o/'source/tools/build_linux_baseline.py');m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
cmd=m.environment_command(o/'source',b,rootfs=r/'.cache/prealpha-elastic-linux/ubuntu24')+['xvfb-run','-a','python3','/build/run_followup.py']
(o/'FOLLOWUP_COMMAND.json').write_text(json.dumps(cmd,indent=2)+'\n')
with (o/'followup-regressions.log').open('w') as f:code=subprocess.run(cmd,stdout=f,stderr=subprocess.STDOUT).returncode
(o/'FOLLOWUP_RESULT.json').write_text(json.dumps({'exit':code})+'\n');print('Follow-up runner',code)
