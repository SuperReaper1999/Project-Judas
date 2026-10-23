#!/usr/bin/env python3
"""Run the isolated project-settings/water checks sequentially; preserve every run."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import time


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build', default='build-water')
    parser.add_argument('--graphics', action='store_true', help='Requires a working native GL desktop')
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    build = (root / args.build).resolve()
    parent = root / '.cache' / 'water-verification'
    parent.mkdir(parents=True, exist_ok=True)
    out = Path(tempfile.mkdtemp(prefix='run-', dir=parent))
    receipts = []

    def run(name, target, extra=(), env=None):
        executable = build / target
        command = [str(executable), *map(str, extra)]
        started = time.time()
        with (out / (name + '.log')).open('w') as log:
            result = subprocess.run(command, cwd=root, env=env, stdout=log,
                                    stderr=subprocess.STDOUT, timeout=180)
        receipts.append(dict(name=name, command=command, exit=result.returncode,
                             seconds=time.time()-started,
                             binary_sha256=hashlib.sha256(executable.read_bytes()).hexdigest()))
        (out / 'results.json').write_text(json.dumps(receipts, indent=2)+'\n')
        print(name, result.returncode, flush=True)
        if result.returncode:
            raise SystemExit(f'Failed; preserved output: {out}')

    for name, target in [
        ('policy', 'judas_project_water_tests'),
        ('settings-ui', 'judas_project_settings_editor_tests'),
        ('scene', 'judas_scene_tests'), ('project', 'judas_project_tests'),
        ('conserved-surface', 'judas_liquid_surface_tests'),
        ('mk2', 'judas_mk2_physics_tests'),
        ('balance-foundation', 'judas_physical_balance_foundation_tests'),
        ('balance-controller', 'judas_physical_balance_controller_tests'),
    ]:
        run(name, target)
    run('origin-environment', 'judas_origin_environment_tests', [out/'origin'])
    run('particle-control', 'judas_production_fluid_tests', ['--neutral', '--output', out/'particles'])
    if args.graphics:
        env = dict(os.environ)
        env.setdefault('SDL_VIDEODRIVER', 'x11')
        run('conserved-application', 'judas_liquid_application_tests', env=env)
        run('surface-application', 'judas_liquid_surface_application_tests', env=env)
        run('cold-write', 'judas_basic_water_application_tests', ['--cold-write', out/'cold'], env)
        saved = out/'cold'/'water.save'
        before = hashlib.sha256(saved.read_bytes()).hexdigest()
        run('cold-read', 'judas_basic_water_application_tests', ['--cold-read', out/'cold'], env)
        if hashlib.sha256(saved.read_bytes()).hexdigest() != before:
            raise SystemExit('Cold read modified the original save')
        run('profile', 'judas_basic_water_application_tests', ['--profile', out/'profile'], env)
        copied = out/'project'
        shutil.copytree(root/'projects/basic_water_lab', copied)
        run('editor', 'judas_editor', [copied/'basic_water_lab.judasproj'],
            dict(env, JUDAS_EDITOR_AUTOTEST=str(out/'editor'), XDG_CONFIG_HOME=str(out/'config')))
    print('Preserved output:', out)


if __name__ == '__main__':
    main()
