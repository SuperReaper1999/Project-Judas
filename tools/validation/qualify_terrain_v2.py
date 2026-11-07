#!/usr/bin/env python3
"""Exercise the owned terrain game, then export, relocate and cold-restore it."""
import argparse
import datetime
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import time


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    repo = Path(__file__).resolve().parents[2]
    build = args.build.resolve()
    out = args.output.resolve()
    if out.exists() or out.is_relative_to(repo):
        parser.error('Use a fresh directory outside the checkout; evidence is never overwritten.')
    out.mkdir(parents=True)
    rows = []

    def run(name, command, cwd, env=None):
        start = time.monotonic()
        with (out / (name + '.log')).open('w') as log:
            result = subprocess.run([str(c) for c in command], cwd=cwd, env=env,
                                    stdout=log, stderr=subprocess.STDOUT, timeout=180)
        rows.append(dict(name=name, command=[str(c) for c in command], cwd=str(cwd),
                         exit=result.returncode, seconds=time.monotonic() - start))
        if result.returncode:
            raise RuntimeError(f'{name} failed; see {out / (name + ".log")}')

    record = dict(passed=False, human_acceptance=False)
    try:
        record['build'] = json.loads(subprocess.check_output(
            [str(build / 'judas'), '--build-info-json'], text=True))
        project = out / 'editable'
        shutil.copytree(repo / 'projects/terrain_v2_workshop', project,
                        ignore=shutil.ignore_patterns('Saves', '*.lock'))
        run('native-cook', [build / 'judas_terrain_author', 'v2-native',
                           project / 'Authoring/highlands.judasterrain2',
                           project / 'Assets/terrain/highlands.judasmodel'], repo)
        products = project / 'Assets/terrain'
        receipt = json.loads((products / 'highlands.terrain-v2-receipt.json').read_text())
        if receipt['representation'] != 'native' or receipt['collision'] is not None:
            raise RuntimeError('Native receipt must distinguish the preview from collision ownership.')
        for key, filename in [('native', 'highlands.judasterrain'),
                              ('model', 'highlands.judasmodel'), ('preview', 'highlands.preview.tga')]:
            if hashlib.sha256((products / filename).read_bytes()).hexdigest() != receipt[key]:
                raise RuntimeError(f'Cooked {key} product does not match its receipt.')
        run('export', [build / 'judas_export', project / 'terrain_v2_workshop.judasproj',
                       out / 'original-package', build / 'judas'], repo)
        package = out / 'relocated game'
        (out / 'original-package').rename(package)
        work = out / 'unrelated working directory'
        work.mkdir()
        # No source files can accidentally satisfy a package dependency at play time.
        project.rename(out / 'parked editable')
        scripts = {
            'write': '''FRAMES 1800
FRAME_SECONDS 0.016666667
LOG_EVERY 120
ACTION tunnel 1 100 101
AXIS move_y 1 950 1310
SCREENSHOT 1150 TUNNEL_SCREENSHOT
ACTION save 1 1450 1451
WAIT_SERVICES save 10000 1453
''',
            'read': '''FRAMES 180
FRAME_SECONDS 0.016666667
LOG_EVERY 30
ACTION load 1 20 21
WAIT_SERVICES save 10000 30
WAIT_SERVICES scene 10000 35
EXPECT_SCENE Scenes/highlands.judas 45
''',
        }
        for name, text in scripts.items():
            frame = 1750 if name == 'write' else 150
            harness = out / (name + '.harness')
            text = text.replace('TUNNEL_SCREENSHOT', str(out / 'tunnel.png'))
            harness.write_text(text + f'SCREENSHOT {frame} {out / (name + ".png")}\n')
            env = dict(os.environ, JUDAS_TEST_SCRIPT=str(harness),
                       JUDAS_ENGINE_ROOT=str(package / 'engine'),
                       JUDAS_WINDOW_SIZE='1280x800', XDG_DATA_HOME=str(out / 'user-data'))
            run(name, [package / 'judas', package / 'game.judasproj'], work, env)
        restored = []
        for line in (out / 'read.log').read_text().splitlines():
            marker = 'TERRAIN_V2_RESTORED '
            if marker in line:
                restored.append(json.loads(line.split(marker, 1)[1]))
        if len(restored) != 1:
            raise RuntimeError('Expected exactly one actual terrain/game restore callback.')
        state = restored[0]
        terrain = state['terrain']
        if (not terrain['ready'] or terrain['busy'] or terrain['error'] or
                terrain['volumeChunks'] < 8 or state['position']['z'] > -8):
            raise RuntimeError('Cold restore lost the edited terrain or real tunnel traversal.')
        record.update(passed=True, restored=state)
    except Exception as error:
        record['error'] = str(error)
    record.update(finished_utc=datetime.datetime.now(datetime.timezone.utc).isoformat(), runs=rows)
    record['log_sha256'] = {f.name: hashlib.sha256(f.read_bytes()).hexdigest()
                            for f in sorted(out.glob('*.log'))}
    (out / 'RESULTS.json').write_text(json.dumps(record, indent=2) + '\n')
    print(json.dumps({k: v for k, v in record.items() if k not in ('build', 'log_sha256')}, indent=2))
    return not record['passed']


if __name__ == '__main__':
    raise SystemExit(main())
