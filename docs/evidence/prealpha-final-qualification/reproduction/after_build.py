from pathlib import Path
import subprocess,json,time,importlib.util,shutil
r=Path.cwd();o=r/'.cache/prealpha-final-qualification';source=o/'source';build=o/'build24'
while not (o/'BUILD_RESULT.json').exists():time.sleep(5)
assert json.loads((o/'BUILD_RESULT.json').read_text())['exit']==0,'Initial build failed'
spec=importlib.util.spec_from_file_location('baseline',source/'tools/build_linux_baseline.py');m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
prefix=m.environment_command(source,build,rootfs=r/'.cache/prealpha-elastic-linux/ubuntu24')
cmd=prefix+['cmake','--build','/build','--parallel','2','--target','judas_player_curved_locomotion_tests','judas_spacecraft_frame_integration_tests','judas_boundary_migration_tests','judas_powertrain_application_tests']
with (o/'extra-build.log').open('w') as f:code=subprocess.run(cmd,stdout=f,stderr=subprocess.STDOUT).returncode
assert code==0,'Extra build failed'
shutil.copyfile(o/'run_regressions.py',build/'run_regressions.py')
cmd=prefix+['xvfb-run','-a','python3','/build/run_regressions.py']
(o/'REGRESSION_COMMAND.json').write_text(json.dumps(cmd,indent=2)+'\n')
with (o/'regressions.log').open('w') as f:code=subprocess.run(cmd,stdout=f,stderr=subprocess.STDOUT).returncode
(o/'REGRESSION_RESULT.json').write_text(json.dumps({'exit':code})+'\n');print('Regression runner exit',code,flush=True)
