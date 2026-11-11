#!/usr/bin/env python3
"""Run one immutable whole-application workload with external hardware sampling.

No source, content, quality, physics settings or frame clocks are changed here.
Use a fresh output folder per trial and compare matched configurations.
"""
import argparse
import csv
import hashlib
import json
import os
from pathlib import Path
import subprocess
import time
from performance.metrics import summarize, simulation_loss


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
    parser.add_argument('--warmup-seconds', type=float, default=2,
                        help='loading exclusion; verify readiness separately for cold-restored workloads')
    parser.add_argument('--resolution', default='1280x720')
    parser.add_argument('--cap', type=int, default=0)
    parser.add_argument('--key')
    parser.add_argument('--pulse')
    parser.add_argument('--sequence',type=Path,help='ordered physical control events, relative to measured gameplay start')
    parser.add_argument('--no-profile', action='store_true')
    parser.add_argument('--workload-manifest', type=Path,
                        help='verify every frozen input before running; emitted by prepare_workloads.py')
    args = parser.parse_args()
    if not 0 <= args.warmup_seconds <= 30:
        parser.error('warmup must be finite and between 0 and 30 seconds')
    workload_sha256 = None
    if args.workload_manifest:
        manifest = json.loads(args.workload_manifest.read_text())
        root = args.workload_manifest.resolve().parent
        files = manifest['files']
        if not args.project.resolve().is_relative_to(root) or str(args.project.resolve().relative_to(root)) not in files:
            parser.error('selected project is not part of the frozen workload')
        for relative, expected in files.items():
            path = root / relative
            if (Path(relative).is_absolute() or '..' in Path(relative).parts or path.is_symlink()
                    or not path.resolve().is_relative_to(root)):
                parser.error('invalid benchmark manifest path')
            if hashlib.sha256(path.read_bytes()).hexdigest() != expected:
                parser.error('frozen workload changed: ' + relative)
        workload_sha256 = hashlib.sha256(json.dumps(files, sort_keys=True).encode()).hexdigest()
        if workload_sha256 != manifest['workload_sha256']:
            parser.error('workload manifest digest differs')
    if args.out.exists():
        parser.error('output directory already exists')
    args.out.mkdir(parents=True)
    run = args.out / 'application'
    env = os.environ.copy()
    env.update(SDL_AUDIODRIVER='dummy', JUDAS_WINDOW_SIZE=args.resolution)
    command = [str(args.binary.resolve()), '--project', str(args.project.resolve()), '--out', str(run.resolve()),
               '--seconds', str(args.seconds), '--warmup-seconds', str(args.warmup_seconds), '--cap', str(args.cap)]
    for option in ['key', 'pulse']:
        if getattr(args, option):
            command += ['--' + option, getattr(args, option)]
    if args.sequence:
        command += ['--sequence',str(args.sequence.resolve())]
    if args.no_profile:
        command += ['--no-profile']
    process = None
    with (args.out / 'gpu.csv').open('w') as gpu, (args.out / 'application.log').open('w') as log, \
            (args.out / 'os.jsonl').open('w') as os_log:
        gpu_process = subprocess.Popen(['nvidia-smi', '--query-gpu=timestamp,name,driver_version,utilization.gpu,utilization.memory,memory.used,clocks.current.graphics,power.draw',
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
            if process is not None and process.poll() is None:
                process.terminate()
                try:
                    process.wait(timeout=10)
                except subprocess.TimeoutExpired:
                    process.kill()
                    process.wait(timeout=10)
            gpu_process.terminate()
            gpu_process.wait(timeout=10)
    with (args.out / 'gpu.csv').open() as source:
        rows = list(csv.DictReader(source))
    hardware = {key.strip(): value.strip() for key, value in rows[0].items()} if rows else {}
    native = json.loads((run / 'run.json').read_text()) if (run / 'run.json').exists() else {}
    cap_policy = native.get('cap_policy', 'legacy-millisecond-floor-delay' if args.cap else 'none')
    metadata = {'command': command, 'exit': process.returncode, 'clock_ticks': os.sysconf('SC_CLK_TCK'),
                'resolution': args.resolution, 'graphics_policy': env.get('JUDAS_GL_PROFILE', 'auto'),
                'gpu_model': hardware.get('name', 'unavailable'),
                'gpu_driver': hardware.get('driver_version', 'unavailable'),
                'cap_hz': args.cap, 'capture_policy': cap_policy, 'profile': not args.no_profile,
                'duration_seconds': args.seconds, 'warmup_seconds': args.warmup_seconds, 'controls': {'key': args.key, 'pulse': args.pulse,
                             'sequence_sha256': hashlib.sha256(args.sequence.read_bytes()).hexdigest() if args.sequence else None,
                             'environment': {name: value for name, value in sorted(env.items())
                                             if name.startswith('JUDAS_MK2_') or name in
                                             ['JUDAS_ANIMATION_LANES', 'JUDAS_PROFILE_DEPTH', 'JUDAS_CCD_PAIR_PROFILE', 'JUDAS_LIGHT_LISTS', 'JUDAS_SHADOW_REUSE']}},
                'cpu_model': next((line.split(':', 1)[1].strip() for line in Path('/proc/cpuinfo').read_text().splitlines() if line.startswith('model name')), 'unknown'),
                'memory_total': Path('/proc/meminfo').read_text().splitlines()[0],
                'binary_sha256': hashlib.sha256(args.binary.read_bytes()).hexdigest(),
                'project_sha256': hashlib.sha256(args.project.read_bytes()).hexdigest(),
                'workload_sha256': workload_sha256,
                'native_renderer': native.get('renderer'),
                'native_capture_schema': native.get('schema'),
                'gpu_utilization_scope': 'whole device including the desktop; process CPU sampled independently'}
    (args.out / 'invocation.json').write_text(json.dumps(metadata, indent=2))
    if process.returncode:
        raise RuntimeError(f'workload failed; see {args.out / "application.log"}')
    rows = [json.loads(line) for line in (run / 'frames.jsonl').open()]
    report = summarize([row['interval_ms'] for row in rows])
    report.update(simulation_loss(rows, profiled=not args.no_profile))
    if not args.no_profile:
        report['diagnostic_dropped_events'] = rows[-1]['scope_dropped_total'] - rows[0]['scope_dropped_total']
        report['diagnostic_truncated_records'] = rows[-1]['records_truncated_total'] - rows[0]['records_truncated_total']
    (args.out / 'summary.json').write_text(json.dumps(report, indent=2))
    print(json.dumps({'out': str(args.out), **report}), flush=True)


if __name__ == '__main__':
    main()
