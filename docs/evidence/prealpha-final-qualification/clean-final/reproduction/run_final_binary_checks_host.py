import importlib.util,subprocess,shutil,sys
from pathlib import Path
sys.dont_write_bytecode=True
r=Path.cwd();o=r/'.cache/prealpha-final-qualification'
shutil.copyfile(o/'final_binary_checks.py',o/'build24/final_binary_checks.py')
spec=importlib.util.spec_from_file_location('baseline',o/'source/tools/build_linux_baseline.py')
m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
cmd=m.environment_command(o/'source',o/'build24',rootfs=r/'.cache/prealpha-elastic-linux/ubuntu24')+['xvfb-run','-a','python3','/build/final_binary_checks.py']
with (o/'final-binary-checks.log').open('w') as f:
 code=subprocess.run(cmd,stdout=f,stderr=subprocess.STDOUT).returncode
print('Final-binary runner',code)
