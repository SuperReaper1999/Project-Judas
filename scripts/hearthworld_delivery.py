#!/usr/bin/env python3
"""Qualify the unchanged Hearthworld project in the editor and a moved export.

Uses isolated editor/user-data copies, normal Play/Stop automation and shipping
Application input/services. Existing evidence directories are never overwritten.
"""
from pathlib import Path
import argparse
import hashlib
import json
import os
import shutil
import subprocess
import tempfile
import time

ROOT = Path(__file__).resolve().parents[1]


def hashes(root):
    return {str(p.relative_to(root)): hashlib.sha256(p.read_bytes()).hexdigest()
            for p in sorted(root.rglob('*')) if p.is_file()}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    build, out = args.build.resolve(), args.output.resolve()
    out.mkdir(parents=True, exist_ok=False)
    project = ROOT / 'projects/hearthworld/hearthworld.judasproj'
    before = hashes(project.parent)
    env = {k: v for k, v in os.environ.items() if not k.startswith('JUDAS_')}
    env.update(SDL_AUDIODRIVER='dummy', JUDAS_WORLD_STATE='none',
               XDG_DATA_HOME=str(out / 'user-data'))
    runs = []

    def run(name, command, environment=None, cwd=ROOT):
        started = time.monotonic()
        with (out / (name + '.log')).open('w') as log:
            result = subprocess.run([str(x) for x in command], cwd=cwd,
                                    env=environment or env, stdout=log,
                                    stderr=subprocess.STDOUT, timeout=240)
        runs.append({'name': name, 'argv': list(map(str, command)), 'cwd': str(cwd),
                     'exit': result.returncode, 'wallSeconds': time.monotonic() - started})
        (out / 'RESULTS.json').write_text(json.dumps(runs, indent=2) + '\n')
        print(name, result.returncode, flush=True)
        if result.returncode:
            raise RuntimeError(name + ' failed; original log retained')

    editor_copy = out / 'editor-project'
    shutil.copytree(project.parent, editor_copy)
    editor_env = env.copy()
    editor_env.update(JUDAS_EDITOR_AUTOTEST=str(out / 'editor'),
                      JUDAS_EDITOR_AUTOTEST_AUTHORING='1',
                      XDG_CONFIG_HOME=str(out / 'editor-config'))
    run('editor', [build / 'judas_editor', editor_copy / project.name], editor_env)
    editor_log = (out / 'editor.log').read_text()
    for required in ('authored scene after play/stop is IDENTICAL', 'save ok'):
        if required not in editor_log:
            raise RuntimeError('Editor receipt missing: ' + required)
    if 'script asset=' in editor_log or '[editor autotest] FAIL' in editor_log:
        raise RuntimeError('Editor diagnostics require review')

    run('export', [build / 'judas_export', project, out / 'package', build / 'judas'])
    moved = Path(tempfile.mkdtemp(prefix='Judas-Hearthworld-'))
    unrelated = Path(tempfile.mkdtemp(prefix='Judas-Hearthworld-CWD-'))
    shutil.copytree(out / 'package', moved, dirs_exist_ok=True)
    package_hashes = hashes(out / 'package')
    assert package_hashes == hashes(moved), 'Relocation changed package bytes'
    assert package_hashes['judas'] == hashlib.sha256((build / 'judas').read_bytes()).hexdigest()
    for p in moved.rglob('*'):
        if p.is_file():
            p.chmod(0o555 if p.name == 'judas' else 0o444)
    for p in sorted((p for p in moved.rglob('*') if p.is_dir()),
                    key=lambda p: len(p.parts), reverse=True):
        p.chmod(0o555)
    moved.chmod(0o555)
    steps = out / 'moved.steps'
    steps.write_text(f'''FRAMES 390
FRAME_SECONDS 0.016666667
ENTITY 100
LOG_EVERY 0
ACTION pause 1 10 11
EXPECT_PAUSED 0 15
AXIS move_y -1 20 100
LOOK 60 -30 110
SCREENSHOT 140 {out}/standalone-home.png
ACTION camera_toggle 1 160 161
SCREENSHOT 180 {out}/standalone-third.png
ACTION inventory 1 200 201
EXPECT_PAUSED 1 205
SCREENSHOT 210 {out}/standalone-pack.png
ACTION inventory 1 230 231
EXPECT_PAUSED 0 235
ACTION pause 1 250 251
EXPECT_PAUSED 1 255
ACTION save 1 270 271
WAIT_SERVICES save 10000 280
ACTION pause 1 300 301
EXPECT_PAUSED 0 305
ACTION load 1 320 321
WAIT_SERVICES save 10000 330
EXPECT_SCENE Scenes/home.judas 340
SCREENSHOT 365 {out}/standalone-restored.png
''')
    runtime_env = env.copy()
    runtime_env.update(JUDAS_TEST_SCRIPT=str(steps), JUDAS_PROFILE='1')
    run('relocated', [moved / 'judas'], runtime_env, unrelated)
    log = (out / 'relocated.log').read_text()
    if 'script asset=' in log or 'Asset problem:' in log:
        raise RuntimeError('Relocated application diagnostics require review')
    assert package_hashes == hashes(moved), 'Runtime wrote into the read-only installation'
    assert before == hashes(project.parent), 'Qualification changed original game source'
    (out / 'PACKAGE_FILES.json').write_text(json.dumps(package_hashes, indent=2) + '\n')
    (out / 'LOCATIONS.json').write_text(json.dumps({
        'package': str(out / 'package'), 'moved': str(moved),
        'workingDirectory': str(unrelated), 'installationReadOnly': True,
        'projectSourceUnchanged': True}, indent=2) + '\n')
    print(moved, flush=True)
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
