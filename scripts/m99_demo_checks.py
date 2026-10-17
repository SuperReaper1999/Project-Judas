#!/usr/bin/env python3
"""Actual JS/API checks and two independent headless Connected Workshop processes.

The native runner uses the ordinary authored game, public JS calls, session
transport and authoritative simulation step. Only initial physical arrangement
and deterministic input are test-owned. No alternate gameplay implementation.
"""
from pathlib import Path
import argparse
import hashlib
import json
import os
import socket
import subprocess
import time

ROOT = Path(__file__).resolve().parents[1]


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--runner', type=Path, default=ROOT / 'build/judas_network_script_tests')
    parser.add_argument('--project', type=Path, default=ROOT / 'projects/connected_workshop/connected_workshop.judasproj')
    parser.add_argument('--client-project', type=Path, help='Different relocated/exported content for a compatibility check')
    parser.add_argument('--skip-bindings', action='store_true')
    args = parser.parse_args()
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    runner = args.runner.resolve()
    project = args.project.resolve()
    client_project = args.client_project.resolve() if args.client_project else project
    environment = {k: v for k, v in os.environ.items() if not k.startswith('JUDAS_')}
    environment.update(SDL_AUDIODRIVER='dummy', JUDAS_WORLD_STATE='none', XDG_DATA_HOME=str(output / 'user-data'))
    results = {'runner': str(runner), 'runner_sha256': digest(runner), 'project': str(project),
               'client_project':str(client_project), 'source_files': {str(p.relative_to(project.parent)): digest(p)
                                for p in sorted(project.parent.rglob('*')) if p.is_file()}, 'runs': []}

    def record():
        (output / 'RESULTS.json').write_text(json.dumps(results, indent=2) + '\n')

    record()
    if not args.skip_bindings:
        command = [str(runner), str(output / 'bindings-fixture')]
        started = time.monotonic()
        with (output / 'bindings.log').open('w') as log:
            result = subprocess.run(command, cwd=ROOT, env=environment, stdout=log, stderr=subprocess.STDOUT, timeout=60)
        results['runs'].append({'name': 'actual-js-bindings', 'argv': command,
                                'exit': result.returncode, 'seconds': time.monotonic() - started})
        record()
        if result.returncode:
            raise RuntimeError('Actual public binding checks failed; original log retained')

    with socket.socket() as temporary:
        temporary.bind(('127.0.0.1', 0))
        port = temporary.getsockname()[1]
    processes = {}
    logs = {}
    started = time.monotonic()
    try:
        for role in ['host', 'client']:
            if role == 'client':
                deadline = time.monotonic() + 10
                while 'READY host' not in (output / 'host.log').read_text():
                    if processes['host'].poll() is not None or time.monotonic() > deadline:
                        raise RuntimeError('Host failed before listening; failure retained')
                    time.sleep(.02)
            command = [str(runner), '--demo', role, str(client_project if role == 'client' else project), str(port), str(output / (role + '.json'))]
            logs[role] = (output / (role + '.log')).open('w')
            processes[role] = subprocess.Popen(command, cwd=ROOT, env=environment,
                                                stdout=logs[role], stderr=subprocess.STDOUT)
            results['runs'].append({'name': 'game-' + role, 'argv': command})
        for role, process in processes.items():
            status = process.wait(timeout=30)
            next(r for r in results['runs'] if r['name'] == 'game-' + role)['exit'] = status
            record()
    finally:
        for process in processes.values():
            if process.poll() is None:
                process.terminate()
                try:
                    process.wait(timeout=3)
                except subprocess.TimeoutExpired:
                    process.kill()
                    process.wait()
        for log in logs.values():
            log.close()
        results['process_seconds'] = time.monotonic() - started
        record()
    if any(r.get('exit') != 0 for r in results['runs']):
        raise RuntimeError('Game process failed; native trace and original log retained')
    host = json.loads((output / 'host.json').read_text())
    client = json.loads((output / 'client.json').read_text())
    assert host['connected'] and client['connected'], 'Actual wire connection missing'
    assert host['state']['delivered'] >= 1, 'Real game did not score physical cargo'
    guest_score = next(e['state']['delivered'] for e in client['replicated'] if e['entity'] == 100)
    assert guest_score == host['state']['delivered'], 'Replicated gameplay state differs'
    endpoint_error = sum((a - b) ** 2 for a, b in zip(host['crate'], client['crate'])) ** .5
    assert endpoint_error < .10, 'Stationary actual physical endpoints differ by >= 10 cm'
    results['observed'] = {'host_delivered': host['state']['delivered'], 'client_delivered': guest_score,
                           'crate_endpoint_error_m': endpoint_error,
                           'host_sent_bytes': host['sent'], 'host_received_bytes': host['received'],
                           'client_sent_bytes': client['sent'], 'client_received_bytes': client['received'],
                           'host_snapshot_count': host['snapshots'], 'client_snapshot_count': client['snapshots']}
    record()
    print(json.dumps(results['observed'], indent=2))


if __name__ == '__main__':
    main()
