#!/usr/bin/env python3
"""Plan by default; --execute runs ONE serialized, unchanged stress workload.

No builds are performed. Caller must finish builds/other engine workloads first.
The lock coordinates this runner only, not unrelated build or game processes.
"""
import argparse
import csv
import datetime
import fcntl
import hashlib
import json
import os
from pathlib import Path
import signal
import subprocess
import time

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]


def sha(path):
    h = hashlib.sha256()
    with path.open('rb') as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b''):
            h.update(block)
    return h.hexdigest()


def inputs(extra=()):
    names = subprocess.check_output(['git', 'ls-files', '--', 'src', 'cmake', 'CMakeLists.txt',
                                     'tests/PhysicsStressLiveBenchmark.cpp'], cwd=ROOT, text=True).splitlines()
    result = {name: sha(ROOT / name) for name in names if (ROOT / name).is_file()}
    for path in extra:
        if not path.is_file():
            raise RuntimeError(f'explicit candidate input missing: {path}')
        result[str(path.resolve())] = sha(path)
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--binary', type=Path, required=True)
    parser.add_argument('--core', type=Path, default=HERE / 'baseline/libjudas_engine.a')
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--sample', action='store_true', help='Enable bounded main-thread PC sampler in sampler-only binary')
    parser.add_argument('--stage-capture', action='store_true', help='Capture previous completed existing profiler snapshots; requires --profile')
    parser.add_argument('--profile', action='store_true', help='Enable existing coarse engine profiler; use matched off/on runs')
    parser.add_argument('--period-us', type=int, default=2000)
    parser.add_argument('--mode', choices=['900', '1800', 'live35'], required=True)
    parser.add_argument('--timeout', type=float, default=600)
    parser.add_argument('--extra-source', type=Path, action='append', default=[],
                        help='Each intended untracked module/header; unrelated historical untracked files are excluded')
    parser.add_argument('--execute', action='store_true')
    args = parser.parse_args()
    args.binary, args.output, args.core = args.binary.resolve(), args.output.resolve(), args.core.resolve()
    if args.stage_capture and not args.profile:
        raise RuntimeError('stage capture requires --profile')
    frozen = json.loads((HERE / 'baseline/FROZEN.json').read_text())
    fixture = ROOT / 'projects/physics_stress_lab/Scenes/room1024.judas'
    if sha(fixture) != frozen['fixture']['sha256']:
        raise RuntimeError('original stress fixture changed')
    values = {'900': ['180', '900', '0', '0', '1', '0.016666667', '0', '0', '1'],
              '1800': ['240', '1800', '0', '0', '1', '0.016666667', '0', '0', '1'],
              'live35': ['35', '10000000', '0', '0', '0', '0', '0', '0', '1']}[args.mode]
    if args.period_us < 500 or args.period_us > 1000000:
        raise RuntimeError('bounded sampler period must be 500..1000000us')
    values[-3] = '1' if args.profile else '0'
    command = [str(args.binary), str(fixture), str(args.output / 'live'), *values]
    plan = {'argv': command, 'cwd': str(ROOT), 'mode': args.mode,
            'binary_sha256': sha(args.binary), 'core_sha256': sha(args.core),
            'fixture_sha256': sha(fixture), 'execute': args.execute,
            'accepted_baseline_head': frozen['accepted_head'], 'sample': args.sample,
            'sample_period_us': args.period_us if args.sample else None, 'engine_profiler': args.profile,
            'timing_scope': 'ordinary benchmark fixed-step spans; endpoint quality/CSV digest outside spans',
            'live_cadence_scope': 'real frame cadence only in live35; synthetic frames never FPS'}
    if not args.execute:
        print(json.dumps(plan, indent=2))
        return
    if args.output.exists():
        raise RuntimeError('fresh output required; refusing overwrite')
    env = os.environ.copy()
    for key in list(env):
        if key.startswith(('JUDAS_CPU_SAMPLE', 'JUDAS_DEMAND_AUDIT', 'JUDAS_AUDIT_',
                           'JUDAS_DIAGNOSTIC_', 'JUDAS_BOUNDARY_', 'M78_DIAGNOSTIC_')):
            del env[key]
    if env.get('SDL_VIDEODRIVER') == 'dummy' or env.get('LIBGL_ALWAYS_SOFTWARE') == '1':
        raise RuntimeError('native desktop graphics required; refusing dummy/software graphics override')
    if args.sample:
        env['JUDAS_CPU_SAMPLE'] = '1'
        env['JUDAS_CPU_SAMPLE_PERIOD_US'] = str(args.period_us)
    env.pop('JUDAS_STAGE_CAPTURE', None)
    if args.stage_capture:
        env['JUDAS_STAGE_CAPTURE'] = '1'
    env['SDL_AUDIODRIVER'] = 'dummy'
    # isolate writes without modifying project/runtime configuration
    for key, subdir in [('XDG_DATA_HOME', 'userdata'), ('XDG_CONFIG_HOME', 'config'), ('XDG_CACHE_HOME', 'cache')]:
        env[key] = str(args.output / subdir)
    lock = (HERE / '.workload.lock').open('w')
    fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
    args.output.mkdir(parents=True)
    before = inputs(args.extra_source)
    plan.update(start_utc=datetime.datetime.now(datetime.timezone.utc).isoformat(), source_inputs=before,
                environment={key: env.get(key) for key in ['DISPLAY', 'WAYLAND_DISPLAY', 'XDG_RUNTIME_DIR',
                    'SDL_VIDEODRIVER', 'SDL_AUDIODRIVER', 'LIBGL_ALWAYS_SOFTWARE', 'XDG_DATA_HOME', 'XDG_CONFIG_HOME', 'XDG_CACHE_HOME']})
    receipt = args.output / 'AS_RUN.json'
    receipt.write_text(json.dumps(plan, indent=2) + '\n')
    started = time.monotonic()
    timed_out = False
    with (args.output / 'run.log').open('w') as log:
        process = subprocess.Popen(['/usr/bin/time', '-v', '-o', str(args.output / 'rusage.log'), *command],
                                   cwd=ROOT, env=env, stdout=log, stderr=subprocess.STDOUT, start_new_session=True)
        try:
            code = process.wait(timeout=args.timeout)
        except subprocess.TimeoutExpired:
            timed_out = True
            os.killpg(process.pid, signal.SIGTERM)
            try:
                process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                os.killpg(process.pid, signal.SIGKILL)
                process.wait()
            code = process.returncode
    plan.update(exit=code, timed_out=timed_out, seconds=time.monotonic() - started,
                finish_utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),
                source_inputs_unchanged=inputs(args.extra_source) == before,
                binary_unchanged=sha(args.binary) == plan['binary_sha256'],
                core_unchanged=sha(args.core) == plan['core_sha256'],
                fixture_unchanged=sha(fixture) == plan['fixture_sha256'])
    steps = args.output / 'live/steps.csv'
    if steps.is_file():
        with steps.open() as stream:
            rows = list(csv.DictReader(stream))
        plan['completed_steps'] = len(rows)
        plan['nonfinite_sum'] = sum(int(row['nonfinite_fields']) for row in rows)
    quality = args.output / 'live/quality.json'
    if quality.is_file():
        plan['endpoint_quality'] = json.loads(quality.read_text())
    log_text = (args.output / 'run.log').read_text(errors='replace')
    graphics = [line for line in log_text.splitlines() if line.startswith(('Graphics:', 'NATIVE_GL '))]
    plan['graphics_evidence'] = graphics
    plan['native_nvidia_confirmed'] = bool(graphics) and all('NVIDIA' in line and not any(word in line.lower() for word in ('llvmpipe', 'softpipe', 'swiftshader')) for line in graphics)
    plan['not_an_automatic_physical_acceptance'] = True
    receipt.write_text(json.dumps(plan, indent=2) + '\n')
    print(json.dumps({key: value for key, value in plan.items() if key != 'source_inputs'}, indent=2))
    raise SystemExit(0 if code == 0 and not timed_out and all(plan[key] for key in
        ['source_inputs_unchanged', 'binary_unchanged', 'core_unchanged', 'fixture_unchanged', 'native_nvidia_confirmed']) else 1)


if __name__ == '__main__':
    main()
