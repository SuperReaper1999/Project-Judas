#!/usr/bin/env python3
"""Run reproducible M87 physics workloads sequentially, preserving raw receipts.

Never builds, changes engine settings between before/after, stages or edits Git.
Use separate output directories and the same arguments/binary driver source for
baseline and candidate. Do not run alongside compilation/VM CPU workloads.
"""
import argparse
import csv
import datetime
import hashlib
import json
import os
import pathlib
import platform
import shutil
import subprocess
import sys
import time

ROOT = pathlib.Path(__file__).resolve().parents[1]
WORKLOADS = ['free', 'pile', 'mesh', 'machines_active', 'machines_sleeping']


def utc():
    return datetime.datetime.now(datetime.timezone.utc).isoformat()


def sha(path):
    h = hashlib.sha256()
    with path.open('rb') as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b''):
            h.update(block)
    return h.hexdigest()


def command(*args):
    return subprocess.check_output(args, cwd=ROOT, text=True).strip()


def inventory(binary):
    # Explicitly tracked source plus this new driver, never unrelated untracked
    # archives/assets, protected research, secrets or build output directories.
    names = command('git', 'ls-files', 'src', 'CMakeLists.txt').splitlines()
    names += ['tests/PhysicsScalingBenchmarks.cpp', 'scripts/m87_benchmark.py']
    return {name: sha(ROOT / name) for name in sorted(set(names))
            if (ROOT / name).is_file()} | {'binary:' + str(binary): sha(binary)}


def host():
    info = {'platform': platform.platform(), 'python': sys.version,
            'cpu_count': os.cpu_count()}
    for tool, args in [('lscpu', []), ('free', ['-b']), ('uname', ['-a'])]:
        if shutil.which(tool):
            result = subprocess.run([tool] + args, capture_output=True, text=True)
            info[tool] = result.stdout
    info['affinity'] = sorted(os.sched_getaffinity(0)) if hasattr(os, 'sched_getaffinity') else None
    return info


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--binary', type=pathlib.Path, default=ROOT / 'build/judas_physics_scaling_benchmarks')
    p.add_argument('--output', type=pathlib.Path, required=True)
    p.add_argument('--counts', nargs='+', type=int, default=[64, 256, 1024])
    p.add_argument('--machine-counts', nargs='+', type=int, default=[16, 64, 256])
    p.add_argument('--workloads', nargs='+', choices=WORKLOADS, default=WORKLOADS)
    p.add_argument('--warm', type=int, default=60)
    p.add_argument('--sleep-warm', type=int, default=300)
    p.add_argument('--steps', type=int, default=120)
    p.add_argument('--repeat', type=int, default=1)
    p.add_argument('--timeout', type=float, default=180)
    p.add_argument('--profile', type=int, choices=[0, 1], default=1)
    p.add_argument('--sleep', type=int, choices=[0, 1], default=1)
    p.add_argument('--rotated', action='store_true')
    p.add_argument('--negative-controls', action='store_true')
    p.add_argument('--compare', type=pathlib.Path, help='existing baseline run.json; never modifies it')
    a = p.parse_args()
    a.binary = a.binary.resolve()
    a.output = a.output.resolve()
    if not a.binary.is_file():
        p.error('binary does not exist: ' + str(a.binary))
    if a.output.exists() and any(a.output.iterdir()):
        p.error('output must be absent or empty; old failures are never overwritten')
    a.output.mkdir(parents=True, exist_ok=True)
    before = inventory(a.binary)
    receipt = {'schema': 'judas.m87.benchmark_run.v1', 'started_utc': utc(),
               'head': command('git', 'rev-parse', 'HEAD'),
               'branch': command('git', 'branch', '--show-current'),
               'tracked_status': command('git', 'status', '--short', '--untracked-files=no'),
               'argv': sys.argv, 'host': host(), 'fingerprints': before, 'cases': [],
               'settings': {'counts': a.counts, 'machine_counts': a.machine_counts,
                            'workloads': a.workloads, 'warm': a.warm, 'sleep_warm': a.sleep_warm,
                            'steps': a.steps, 'profile': a.profile, 'sleep': a.sleep,
                            'rotated': a.rotated, 'repeat': a.repeat},
               'notes': ['Raw CSV keeps warmup and measured steps separately.',
                         'M56 JSON retains its bounded history/spikes, not every step; CSV retains every step.',
                         'Native process RSS includes profiler/allocator and collision cooking.',
                         'No rendering, GPU or engine-wide frame-rate claim is made.']}
    (a.output / 'source.patch').write_text(command('git', 'diff', '--', 'src', 'CMakeLists.txt'))
    cache = ROOT / 'build/CMakeCache.txt'
    if cache.is_file():
        receipt['cmake_configuration'] = [line for line in cache.read_text().splitlines()
                                           if line.startswith(('CMAKE_BUILD_TYPE:', 'CMAKE_CXX_COMPILER:',
                                                               'CMAKE_CXX_FLAGS', 'CMAKE_GENERATOR:'))]
    cases = [(w, n * 4 if w.startswith('machines_') else n, r, [], False)
             for w in a.workloads
             for n in (a.machine_counts if w.startswith('machines_') else a.counts)
             for r in range(a.repeat)]
    if a.negative_controls:
        cases += [('pile', 20, 0, ['--negative-no-contact'], True),
                  ('pile', 20, 0, ['--negative-no-body-contact'], True),
                  ('free', 20, 0, ['--negative-freeze'], True),
                  ('machines_sleeping', 20, 0, ['--negative-freeze'], True)]
    for workload, count, repeat, extra, expected_failure in cases:
        name = f'{workload}-{count}-r{repeat}' + ('-negative-' + extra[0][11:] if extra else '')
        dest = a.output / name
        warm = a.sleep_warm if workload == 'machines_sleeping' else a.warm
        cmd = [str(a.binary), '--workload', workload, '--count', str(count), '--warm', str(warm),
               '--steps', str(a.steps), '--profile', str(a.profile), '--sleep', str(a.sleep),
               '--output', str(dest)] + extra + (['--rotated'] if a.rotated else [])
        start = time.monotonic()
        print(f'Running {name}', flush=True)
        try:
            with (a.output / (name + '.log')).open('w') as log:
                result = subprocess.run(cmd, cwd=ROOT, stdout=log, stderr=subprocess.STDOUT, timeout=a.timeout)
            exit_code = result.returncode
        except subprocess.TimeoutExpired:
            exit_code = 'timeout'
        summary = json.loads((dest / 'summary.json').read_text()) if (dest / 'summary.json').is_file() else None
        # A crash/timeout does not prove the intended negative control worked.
        passed = (exit_code == 1 and summary is not None and not summary['pass']) if expected_failure else (exit_code == 0 and summary is not None and summary['pass'])
        case = {'name': name, 'command': cmd, 'elapsed_seconds': time.monotonic() - start,
                'exit_code': exit_code, 'expected_failure': expected_failure,
                'accepted_result': passed, 'summary': summary}
        receipt['cases'].append(case)
        (a.output / 'run.json').write_text(json.dumps(receipt, indent=2) + '\n')
        if summary:
            timing = summary['input_and_step_ms']
            print(f"  {'PASS' if passed else 'FAIL'} median {timing['median']:.4f}, p95 {timing['p95']:.4f}, max {timing['maximum']:.4f} ms", flush=True)
        else:
            print('  FAIL no complete summary; raw partial CSV/log preserved', flush=True)
    receipt['finished_utc'] = utc()
    after = inventory(a.binary)
    receipt['source_and_binary_unchanged_during_run'] = before == after
    receipt['pass'] = all(c['accepted_result'] for c in receipt['cases']) and before == after
    (a.output / 'run.json').write_text(json.dumps(receipt, indent=2) + '\n')
    with (a.output / 'comparison.csv').open('w', newline='') as stream:
        writer = csv.writer(stream)
        writer.writerow(['case', 'dynamic_bodies', 'median_ms', 'p95_ms', 'maximum_ms',
                         'peak_rss_bytes', 'trajectory_digest', 'physical_pass'])
        for c in receipt['cases']:
            s = c['summary']
            if s:
                t = s['input_and_step_ms']
                writer.writerow([c['name'], s['dynamic_bodies'], t['median'], t['p95'], t['maximum'],
                                 s['process_peak_rss_bytes'], s['trajectory_digest'], s['pass']])
    if a.compare:
        baseline = json.loads(a.compare.read_text())
        comparable = baseline['settings'] == receipt['settings']
        comparison = {'identical_settings': comparable, 'baseline': str(a.compare.resolve()),
                      'baseline_head': baseline['head'], 'candidate_head': receipt['head'], 'cases': []}
        previous = {c['name']: c for c in baseline['cases']}
        for c in receipt['cases']:
            b = previous.get(c['name'], {}).get('summary')
            s = c['summary']
            if b and s:
                comparison['cases'].append({'name': c['name'], 'identical_trajectory_digest': b['trajectory_digest'] == s['trajectory_digest'],
                                            'baseline_ms': b['input_and_step_ms'], 'candidate_ms': s['input_and_step_ms'],
                                            'both_physical_pass': b['pass'] and s['pass']})
        (a.output / 'before_after.json').write_text(json.dumps(comparison, indent=2) + '\n')
    return 0 if receipt['pass'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
