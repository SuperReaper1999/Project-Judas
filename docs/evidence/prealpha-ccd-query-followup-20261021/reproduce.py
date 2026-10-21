#!/usr/bin/env python3
"""Plan by default. Rebuild the archived observer and execute one native workload."""
import argparse
import json
from pathlib import Path
import shlex
import shutil
import subprocess

HERE = Path(__file__).resolve().parent
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--repo', type=Path, default=HERE.parents[2])
parser.add_argument('--build', type=Path)
parser.add_argument('--core', type=Path)
parser.add_argument('--output', type=Path, required=True)
parser.add_argument('--mode', choices=['900', '1800', 'live35'], default='1800')
parser.add_argument('--camera', choices=['stationary', 'wasd', 'watch'], default='stationary')
parser.add_argument('--execute', action='store_true')
args = parser.parse_args()
root = args.repo.resolve()
build = (args.build or root / 'build').resolve()
output = args.output.resolve()
core = (args.core or build / 'libjudas_engine.a').resolve()
flags = {}
for line in (build / 'CMakeFiles/judas_physics_stress_live_benchmark.dir/flags.make').read_text().splitlines():
    if ' = ' in line:
        name, value = line.split(' = ', 1)
        flags[name] = shlex.split(value)
obj = output / 'observer.o'
compile_command = ['/usr/bin/c++', *flags['CXX_DEFINES'], *flags['CXX_INCLUDES'],
                   *flags['CXX_FLAGS'], '-c', str(HERE / 'driver/DiagnosticLive-v4.cpp'),
                   '-o', str(obj)]
link = shlex.split((build / 'CMakeFiles/judas_physics_stress_live_benchmark.dir/link.txt').read_text())
link = [x for x in link if not x.startswith('-Wl,--dependency-file=')]
original = 'CMakeFiles/judas_physics_stress_live_benchmark.dir/tests/PhysicsStressLiveBenchmark.cpp.o'
assert link.count(original) == 1, 'Inspect changed benchmark link layout'
link[link.index(original)] = str(obj)
link[link.index('libjudas_engine.a')] = str(core)
link[link.index('-o') + 1] = str(output / 'live')
run = ['python3', str(output / 'run_current.py'), '--binary', str(output / 'live'),
       '--core', str(core), '--output', str(output / 'run'), '--mode', args.mode,
       '--camera', args.camera, '--execute']
for name in ['WorldCCDQuery.h', 'WorldCCDDispatch.cpp', 'ContactGroupUpdates.h']:
    run += ['--extra-source', str(root / 'src/physics' / name)]
plan = {'compile': compile_command, 'link': link, 'cwd': str(build), 'run': run}
print(json.dumps(plan, indent=2), flush=True)
if not args.execute:
    raise SystemExit(0)
output.mkdir(parents=True, exist_ok=False)
(output / 'PLAN.json').write_text(json.dumps(plan, indent=2) + '\n')
runner = (HERE / 'run_current.py').read_text().replace('ROOT = HERE.parents[1]', 'ROOT = Path(' + repr(str(root)) + ')')
(output / 'run_current.py').write_text(runner)
(output / 'baseline').mkdir()
shutil.copy2(HERE / 'baseline/FROZEN.json', output / 'baseline/FROZEN.json')
for name, command in [('compile', compile_command), ('link', link), ('run', run)]:
    with (output / (name + '.log')).open('w') as log:
        subprocess.run(command, cwd=build, stdout=log, stderr=subprocess.STDOUT, check=True)
