#!/usr/bin/env python3
"""Run one immutable whole-application workload with external hardware sampling.

No source, content, quality, physics settings or frame clocks are changed here.
Use a fresh output folder per trial and compare matched configurations.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import time
from performance.metrics import summarize


def sample_process(pid):
    root = Path(f'/proc/{pid}')
    tasks = []
    for task in (root / 'task').iterdir():
        try:
            fields = (task / 'stat').read_text().rsplit(')', 1)[1].split()
            tasks.append({'tid': int(task.name), 'cpu_ticks': int(fields[11]) + int(fields[12]),
                          'name': (task / 'comm').read_text().strip()})
        except FileNotFoundError:
            pass
    return {'time_ns': time.monotonic_ns(), 'tasks': tasks, 'status': (root / 'status').read_text(),
            'system_cpu': [line for line in Path('/proc/stat').read_text().splitlines()
                           if line.startswith('cpu')]}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--binary', type=Path, required=True)
    parser.add_argument('--project', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--seconds', type=float, default=20)
    parser.add_argument('--resolution', default='1280x720')
    parser.add_argument('--cap', type=int, default=0)
    parser.add_argument('--key')
    parser.add_argument('--pulse')
    parser.add_argument('--no-profile', action='store_true')
    args = parser.parse_args()
    if args.out.exists():
        parser.error('output directory already exists')
    args.out.mkdir(parents=True)
    run = args.out / 'application'
    env = os.environ.copy()
    env.update(SDL_AUDIODRIVER='dummy', JUDAS_WINDOW_SIZE=args.resolution)
    command = [str(args.binary.resolve()), '--project', str(args.project.resolve()), '--out', str(run.resolve()),
               '--seconds', str(args.seconds), '--warmup-seconds', '2', '--cap', str(args.cap)]
    for option in ['key', 'pulse']:
        if getattr(args, option):
            command += ['--' + option, getattr(args, option)]
    if args.no_profile:
        command += ['--no-profile']
    with (args.out / 'gpu.csv').open('w') as gpu, (args.out / 'application.log').open('w') as log, \
            (args.out / 'os.jsonl').open('w') as os_log:
        gpu_process = subprocess.Popen(['nvidia-smi', '--query-gpu=timestamp,name,utilization.gpu,utilization.memory,memory.used,clocks.current.graphics,power.draw',
                                        '--format=csv', '--loop-ms=200'], stdout=gpu, stderr=subprocess.STDOUT)
        try:
            process = subprocess.Popen(command, env=env, stdout=log, stderr=subprocess.STDOUT)
            deadline = time.monotonic() + args.seconds + 120
            while process.poll() is None:
                if time.monotonic() > deadline:
                    process.terminate()
                    try:
                        process.wait(timeout=10)
                    except subprocess.TimeoutExpired:
                        process.kill()
                    raise RuntimeError('benchmark timed out')
                try:
                    os_log.write(json.dumps(sample_process(process.pid)) + '\n')
                except (FileNotFoundError, ProcessLookupError):
                    pass
                time.sleep(.2)
        finally:
            gpu_process.terminate()
            gpu_process.wait(timeout=10)
    metadata = {'command': command, 'exit': process.returncode, 'clock_ticks': os.sysconf('SC_CLK_TCK'),
                'resolution': args.resolution, 'binary_sha256': hashlib.sha256(args.binary.read_bytes()).hexdigest(),
                'project_sha256': hashlib.sha256(args.project.read_bytes()).hexdigest(),
                'gpu_utilization_scope': 'whole device including the desktop; process CPU sampled independently'}
    (args.out / 'invocation.json').write_text(json.dumps(metadata, indent=2))
    if process.returncode:
        raise RuntimeError(f'workload failed; see {args.out / "application.log"}')
    rows = [json.loads(line) for line in (run / 'frames.jsonl').open()]
    report = summarize([row['interval_ms'] for row in rows])
    report['physics_discarded_seconds'] = sum(row.get('discarded_seconds', 0) for row in rows)
    report['physics_cap_frames'] = sum(bool(row.get('cap_reached')) for row in rows)
    (args.out / 'summary.json').write_text(json.dumps(report, indent=2))
    print(json.dumps({'out': str(args.out), **report}), flush=True)


if __name__ == '__main__':
    main()
