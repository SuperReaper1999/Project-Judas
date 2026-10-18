import importlib.util,json,shutil
from pathlib import Path
s=importlib.util.spec_from_file_location('final_run','.cache/prealpha-pass3/run_final.py');m=importlib.util.module_from_spec(s);s.loader.exec_module(m)
m.records=json.loads((m.OUT/'RESULTS.json').read_text())
a=m.run('export_physics_lab',[m.ROOT/'build/judas_export',m.ROOT/'projects/physics_stress_lab/physics_stress_lab.judasproj',m.OUT/'physics-lab-export',m.ROOT/'build/judas'])
if not a:raise SystemExit(1)
moved=Path('/tmp/Judas-PreAlphaPass3-Physics');assert not moved.exists();shutil.copytree(m.OUT/'physics-lab-export',moved)
script=m.OUT/'physics-lab.txt';script.write_text('FRAMES 90\nLOG_EVERY 0\nACTION watch 1 1 2\nSCREENSHOT 60 '+str(m.OUT/'physics-lab.png')+'\n')
raise SystemExit(0 if m.run('relocated_physics_lab',[moved/'judas'],extra={'JUDAS_TEST_SCRIPT':str(script)},cwd=Path('/tmp'),timeout=90) else 1)
