#!/usr/bin/env python3
"""Bounded direct solver controls, not the dense host benchmark."""
from pathlib import Path
import hashlib
import json
import shlex
import subprocess

OWNED = Path(__file__).resolve().parent
ROOT = OWNED.parents[2]
BUILD = ROOT / 'build'
flags = {}
for line in (BUILD / 'CMakeFiles/judas_engine.dir/flags.make').read_text().splitlines():
    if ' = ' in line:
        key, value = line.split(' = ', 1)
        flags[key] = shlex.split(value)
obj = OWNED / 'OrderControls.cpp.o'
subprocess.run(['/usr/bin/c++', *flags['CXX_DEFINES'], *flags['CXX_INCLUDES'], *flags['CXX_FLAGS'],
                '-I'+str(OWNED), '-c', str(OWNED / 'OrderControls.cpp'), '-o', str(obj)], check=True)
base = shlex.split((BUILD / 'CMakeFiles/judas_impact_solver_tests.dir/link.txt').read_text())
results = {}
for name, archive in [('baseline', OWNED.parent/'baseline-e30/libjudas_engine.a'),
                      ('ordered', OWNED/'libjudas_engine.a')]:
    for suite in ['impact', 'controls']:
        binary = OWNED / (suite + '-' + name)
        link = [x for x in base if not x.startswith('-Wl,--dependency-file=')]
        if suite == 'controls':
            link = [str(obj) if x.endswith('tests/ImpactSolverTests.cpp.o') else x for x in link]
            link = [x for x in link if not x.endswith('tests/ValidationMain.cpp.o')]
        link = [str(archive) if x == 'libjudas_engine.a' else x for x in link]
        link[link.index('-o') + 1] = str(binary)
        subprocess.run(link, cwd=BUILD, check=True)
        result = subprocess.run([binary], cwd=ROOT, text=True, capture_output=True)
        (OWNED / (suite + '-' + name + '.log')).write_text(result.stdout + result.stderr)
        results[suite + '-' + name] = {'exit': result.returncode,
                                      'sha256': hashlib.sha256(binary.read_bytes()).hexdigest()}
        print(suite, name, result.returncode, result.stdout.splitlines()[-1:])
        result.check_returncode()
base_text = (OWNED/'controls-baseline.log').read_text().splitlines()
order_text = (OWNED/'controls-ordered.log').read_text().splitlines()
results['free_flight_exact_output_parity'] = ([x for x in base_text if x.startswith('free ')] ==
                                            [x for x in order_text if x.startswith('free ')])
results['anchored_order_changes_physical_result'] = ([x for x in base_text if x.startswith('anchored ')] !=
                                                    [x for x in order_text if x.startswith('anchored ')])
(OWNED/'CONTROLS.json').write_text(json.dumps(results, indent=2)+'\n')
assert results['free_flight_exact_output_parity']
assert results['anchored_order_changes_physical_result']
