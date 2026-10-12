#!/usr/bin/env python3
"""Affected checks on the frozen combined candidate; never overwrite receipts."""
import json
import pathlib
import shutil
import subprocess
import sys

root = pathlib.Path(__file__).resolve().parents[4]
evidence = root / 'docs/evidence/m78-continuation'
build = root / '.cache/m78-followup/release'
out = root / '.cache/m78-continuation/midpoint-final'
out.mkdir(exist_ok=False)
nav = out / 'navigation'
shutil.copytree(root / '.cache/m78-followup/dependency-final-project', nav)
cases = []
def case(name, target, *args, expected=0):
    cases.append((name, [str(build / ('judas_' + target + '_tests')),
                        *map(str, args)], expected))

for name in ('contact_lifecycle', 'collision_geometry', 'physical_balance_foundation',
             'physical_animation', 'joint', 'ragdoll', 'contact_cache', 'contact_storage',
             'rigid_contact', 'impact_solver', 'impact_timing', 'kinematic',
             'world_persistence', 'character_motor', 'spacecraft_control',
             'spacecraft_flight', 'prefab', 'streaming'):
    case(name, name)
case('gravity_selection', 'gravity_selection', out / 'gravity')
case('uniform_gravity', 'uniform_gravity_scene', '--fixtures',
     root / 'docs/evidence/ftft3/fixtures', '--output', out / 'uniform-gravity')
case('lifecycle', 'lifecycle_integration', '--output', out / 'lifecycle')
case('touch_events', 'touch_event', '--output', out / 'touch')
case('character_application', 'character_lab', out / 'character')
case('character_cold_write', 'character_lab', out / 'character-cold', 'cold-write')
case('character_cold_read', 'character_lab', out / 'character-cold', 'cold-read')
case('scalability', 'scalability', out / 'scalability')
case('navigation', 'navigation_dependency', nav / 'landscape_fork.judasproj',
     'Imports/landscape.judasterrain')
for mode in ('variants', 'recovery', 'lifecycle', 'regions', 'radial', 'denied',
             'blocked-recovery', 'low-grip', 'fault', 'cookbook'):
    case('balance_' + mode, 'balance_lab', out / ('balance-' + mode), mode)
for prefix in ('cold', 'cold-stand', 'cold-recovery'):
    for mode in ('write', 'read'):
        case('balance_' + prefix + '_' + mode, 'balance_lab', out / ('balance-' + prefix),
             prefix + '-' + mode)
case('kinematic_application', 'kinematic_lab')
case('kinematic_cold_write', 'kinematic_lab', '--write', out / 'kinematic-cold')
case('kinematic_cold_read', 'kinematic_lab', '--read', out / 'kinematic-cold')
case('kinematic_streaming', 'kinematic_lab', '--stream')
case('cavity_impact', 'fluid_cavity_impact')
case('fluid_rigid_coupling', 'fluid_rigid_coupling')
case('fluid_original_failure', 'production_fluid', '--case', 'rotated_half',
     '--output', out / 'fluid-original', expected=1)

results = []
for name, command, expected in cases:
    label = 'midpoint-final-' + name
    runner = [sys.executable, str(evidence / 'probes/run_check.py'), '--name', label,
              '--timeout', '600', '--', *command]
    result = subprocess.run(runner, cwd=root)
    receipt = json.loads((evidence / (label + '.json')).read_text())
    results.append(dict(name=label, expectedExit=expected, actualExit=receipt['exitCode'],
                        receipt=label + '.json', matchesExpectation=receipt['exitCode'] == expected))
    print(json.dumps(results[-1]), flush=True)
    if receipt['exitCode'] != expected:
        break
summary = evidence / 'LINUX_FINAL_MATRIX.json'
if summary.exists():
    raise SystemExit('Preserve prior final matrix')
summary.write_text(json.dumps(dict(planned=len(cases), completed=len(results), results=results), indent=2) + '\n')
raise SystemExit(0 if len(results) == len(cases) and all(r['matchesExpectation'] for r in results) else 1)
