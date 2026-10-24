from pathlib import Path
import time,shutil,subprocess,importlib.util,json
r=Path.cwd();o=r/'.cache/prealpha-final-qualification';w=o/'sdk-work'
while not (w/'COMPLETE.json').exists():time.sleep(5)
fixtures=w/'fixtures';fixtures.mkdir()
for name in ('modding_lab','liquid_reservoir_demo'):shutil.copytree(o/'source/projects'/name,fixtures/name,ignore=shutil.ignore_patterns('Saves','.cache'))
shutil.copyfile(o/'source/projects/material_lab/Assets/environment/studio.hdr',fixtures/'studio.hdr');shutil.copyfile(o/'extra_tools.py',w/'extra-tools.py')
spec=importlib.util.spec_from_file_location('baseline',o/'source/tools/build_linux_baseline.py');m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
cmd=m.environment_command(o/'sdk',w,rootfs=r/'.cache/prealpha-elastic-linux/ubuntu24')+['python3','/build/extra-tools.py']
with (w/'extra-tools.log').open('w') as f:code=subprocess.run(cmd,stdout=f,stderr=subprocess.STDOUT).returncode
(o/'EXTRA_TOOL_RESULT.json').write_text(json.dumps({'exit':code,'command':cmd},indent=2)+'\n');print('Extra tools exit',code,flush=True)
