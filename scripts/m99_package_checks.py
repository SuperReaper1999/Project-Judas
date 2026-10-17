#!/usr/bin/env python3
"""Validate relocated M99 content, a true headless server and the exported runtime.

APK content is exercised by native Linux clients here. That is a content/protocol
compatibility check, not a claim that Android native code ran on a phone.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import socket
import subprocess
import sys
import time
import zipfile

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--package', type=Path, required=True)
    parser.add_argument('--apk', type=Path, required=True)
    parser.add_argument('--relocated', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    output, relocated = args.output.resolve(), args.relocated.resolve()
    output.mkdir(parents=True, exist_ok=False)
    shutil.copytree(args.package.resolve(), relocated)
    for source, name in [('launch_local.py', 'launch_local.py'), ('README.md', 'PLAY_MULTIPLAYER.md')]:
        shutil.copyfile(ROOT / 'projects/connected_workshop' / source, relocated / name)
    content = output / 'apk-content'
    with zipfile.ZipFile(args.apk) as archive:
        for entry in archive.infolist():
            if not entry.filename.startswith('assets/content/') or entry.is_dir():
                continue
            name = Path(entry.filename).relative_to('assets/content')
            if '..' in name.parts or name.is_absolute():
                raise RuntimeError('unsafe APK content entry')
            destination = content / name
            destination.parent.mkdir(parents=True, exist_ok=True)
            destination.write_bytes(archive.read(entry))
    environment = {k: v for k, v in os.environ.items() if not k.startswith('JUDAS_')}
    environment.update(SDL_AUDIODRIVER='dummy', JUDAS_WORLD_STATE='none',
                       XDG_DATA_HOME=str(output / 'user-data'))
    results = []

    def record():
        (output / 'RESULTS.json').write_text(json.dumps(results, indent=2) + '\n')

    def run(name, command, extra=None, cwd='/tmp'):
        env = environment.copy()
        env.update(extra or {})
        started = time.monotonic()
        with (output / (name + '.log')).open('w') as log:
            result = subprocess.run(list(map(str, command)), cwd=cwd, env=env,
                                    stdout=log, stderr=subprocess.STDOUT, timeout=45)
        results.append(dict(name=name, argv=list(map(str, command)), cwd=str(cwd),
                            exit=result.returncode, seconds=time.monotonic() - started))
        record()
        if result.returncode:
            raise RuntimeError(name + ' failed; exact log retained')

    # Same authored game, genuine two-process public-API traffic, relocated Linux
    # content versus the immutable content actually included inside this APK.
    run('linux-apk-content', [sys.executable, ROOT / 'scripts/m99_demo_checks.py',
        '--output', output / 'cross-content', '--project', relocated / 'game.judasproj',
        '--client-project', content / 'game.judasproj', '--skip-bindings'])

    with socket.socket() as temporary:
        temporary.bind(('127.0.0.1', 0))
        port = temporary.getsockname()[1]
    server_env = environment.copy()
    server_env.pop('DISPLAY', None)
    server_env.pop('WAYLAND_DISPLAY', None)
    server_env['SDL_VIDEODRIVER'] = 'invalid'
    command = [str(ROOT / 'build/judas_server'), str(relocated / 'game.judasproj'), str(port), '9']
    with (output / 'dedicated.log').open('w') as log:
        server = subprocess.Popen(command, cwd='/tmp', env=server_env, stdout=log, stderr=subprocess.STDOUT)
        try:
            deadline = time.monotonic() + 4
            while True:
                if server.poll() is not None or time.monotonic() >= deadline:
                    raise RuntimeError('dedicated server did not start; original log retained')
                try:
                    with socket.create_connection(('127.0.0.1', port), timeout=.1):
                        break
                except OSError:
                    time.sleep(.02)
            run('dedicated-client', [ROOT / 'build/judas_network_script_tests', '--probe',
                relocated / 'game.judasproj', port, output / 'dedicated-client.json'])
            code = server.wait(timeout=12)
            results.append(dict(name='dedicated', argv=command, cwd='/tmp', exit=code,
                                display='absent', videoDriver='invalid'))
            record()
            if code:
                raise RuntimeError('dedicated host failed; original log retained')
        finally:
            if server.poll() is None:
                server.terminate()
                server.wait(timeout=3)

    harness = output / 'shipping.harness'
    harness.write_text('FRAMES 600\nFRAME_SECONDS 0.016666667\nLOG_EVERY 100\n'
                       'AXIS move_x 1 100 130\nSCREENSHOT 500 ' + str(output / 'shipping.png') + '\n')
    run('shipping-runtime', [relocated / 'judas', '--host', port],
        {'JUDAS_TEST_SCRIPT': str(harness)})
    assert (output / 'shipping.png').is_file()
    hashes = {str(path): hashlib.sha256(path.read_bytes()).hexdigest()
              for path in [args.apk.resolve(), relocated / 'judas', ROOT / 'build/judas_server']}
    (output / 'BINARY_SHA256.json').write_text(json.dumps(hashes, indent=2) + '\n')
    print('Relocated runtime, headless host and Linux/APK content compatibility passed.')


if __name__ == '__main__':
    main()
