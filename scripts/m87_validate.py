#!/usr/bin/env python3
"""Sequential M87 correctness receipts; never builds, changes Git or tunes physics.

Use m87_benchmark.py separately for matched performance measurements. Graphics
checks require a working desktop and are explicitly selected with --graphics.
"""
import argparse
import datetime
import hashlib
import json
import os
from pathlib import Path
import subprocess
import time

ROOT = Path(__file__).resolve().parents[1]
CORE = [
    'physics_scaling', 'joint_scaling', 'broadphase', 'collision_geometry',
    'rigid_contact', 'kinematic', 'contact_cache', 'contact_storage',
    'contact_lifecycle', 'joint', 'physics_continuation', 'physical_animation',
    'physical_balance_foundation', 'physical_balance_controller', 'character_motor',
    'post_consumer_ragdoll', 'impact_solver', 'impact_timing', 'impact_mesh_cost',
    'impact_robustness', 'sphere_impact_bounds', 'cooked_penetration',
    'origin_physics', 'mechanical_connection', 'section_material', 'wheel',
    'powertrain', 'deformable', 'fracture', 'profiler',
]


def sha(path):
    value = hashlib.sha256()
    with path.open('rb') as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b''):
            value.update(block)
    return value.hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build', type=Path, default=ROOT / 'build')
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--graphics', action='store_true')
    args = parser.parse_args()
    args.output = args.output.resolve()
    if args.output.exists() and any(args.output.iterdir()):
        parser.error('output must be absent or empty; preserve previous failures')
    args.output.mkdir(parents=True, exist_ok=True)
    commands = [] if args.graphics else [(f'judas_{name}_tests', []) for name in CORE]
    if args.graphics:
        commands = [('judas_physics_scaling_application_tests', [
            str(ROOT / 'projects/physics_stress_lab/physics_stress_lab.judasproj'),
            str(args.output / 'lab')])]
    receipt = {'head': subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip(),
               'started_utc': datetime.datetime.now(datetime.timezone.utc).isoformat(),
               'graphics': args.graphics, 'tests': [], 'human_acceptance': False}
    env = os.environ.copy()
    env['SDL_AUDIODRIVER'] = 'dummy'
    for name, extra in commands:
        binary = args.build.resolve() / name
        command = [str(binary), *extra]
        record = {'command': command, 'binary_sha256_before': sha(binary)}
        start = time.monotonic()
        with (args.output / (name + '.log')).open('w') as log:
            try:
                record['exit_code'] = subprocess.run(command, cwd=ROOT, env=env,
                    stdout=log, stderr=subprocess.STDOUT, timeout=300).returncode
            except subprocess.TimeoutExpired:
                record['exit_code'] = 'timeout'
        record.update(seconds=time.monotonic() - start, binary_sha256_after=sha(binary))
        record['pass'] = record['exit_code'] == 0 and record['binary_sha256_before'] == record['binary_sha256_after']
        receipt['tests'].append(record)
        (args.output / 'results.json').write_text(json.dumps(receipt, indent=2) + '\n')
        print(('PASS' if record['pass'] else 'FAIL'), name, round(record['seconds'], 3), flush=True)
    receipt['finished_utc'] = datetime.datetime.now(datetime.timezone.utc).isoformat()
    receipt['pass'] = all(test['pass'] for test in receipt['tests'])
    (args.output / 'results.json').write_text(json.dumps(receipt, indent=2) + '\n')
    return 0 if receipt['pass'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
