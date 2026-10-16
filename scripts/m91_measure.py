#!/usr/bin/env python3
"""Matched M91 native application timings; no build, quality tuning or Git writes.

Use the SAME observer source linked against the frozen baseline engine and final
engine. A killed/incomplete run stays censored; its unfinished step has no timing.
All cases execute sequentially. Do not compile or run other benchmarks alongside.
"""
import argparse
import csv
import datetime
import hashlib
import json
import math
import os
from pathlib import Path
import platform
import signal
import subprocess
import time

ROOT = Path(__file__).resolve().parents[1]

def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()

def distribution(values):
    values = sorted(values)
    if not values:
        return None
    return {k: values[max(0, math.ceil(q * len(values)) - 1)]
            for k, q in [('median', .5), ('p95_nearest_rank', .95), ('maximum', 1)]}

def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--before', type=Path, required=True)
    p.add_argument('--after', type=Path, default=ROOT/'build/judas_physics_stress_live_benchmark')
    p.add_argument('--scene', type=Path, required=True)
    p.add_argument('--output', type=Path, required=True)
    p.add_argument('--frames', nargs='+', type=int, default=[155, 240])
    p.add_argument('--timeout', type=float, default=90)
    p.add_argument('--repeat', type=int, default=2)
    p.add_argument('--labels', nargs='+', choices=['before', 'after'], default=['before', 'after'])
    p.add_argument('--profile', type=int, choices=[0, 1], default=0)
    a = p.parse_args()
    a.output = a.output.resolve()
    if a.output.exists() and any(a.output.iterdir()):
        p.error('use a fresh directory; never overwrite failures')
    a.output.mkdir(parents=True, exist_ok=True)
    binaries = {'before': a.before.resolve(), 'after': a.after.resolve()}
    names = subprocess.check_output(['git', 'ls-files', 'src', 'CMakeLists.txt'], cwd=ROOT, text=True).splitlines()
    names += ['tests/PhysicsStressLiveBenchmark.cpp', 'scripts/m91_measure.py']
    sources = {n: sha(ROOT/n) for n in sorted(set(names))}
    a.scene = a.scene.resolve()
    binary_hashes = {k: sha(v) for k, v in binaries.items()}
    receipt = {'utc_start': datetime.datetime.now(datetime.timezone.utc).isoformat(),
               'head': subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip(),
               'platform': platform.platform(), 'lscpu': subprocess.check_output(['lscpu'], text=True),
               'affinity': sorted(os.sched_getaffinity(0)), 'sources': sources,
               'binary_sha256': binary_hashes, 'scene': str(a.scene), 'scene_sha256': sha(a.scene),
               'profile': bool(a.profile), 'selected_labels': a.labels, 'requested_frames': a.frames, 'timeout_seconds': a.timeout,
               'notes': ['Synthetic fixed cadence 0.016666667; not displayed FPS.',
                         'Native Release application; no swap wait, no tour, hidden rendering.',
                         'Endpoint quality observation is outside fixed-step timings.',
                         'Native peak RSS includes driver/renderer; it is not VRAM.',
                         'Censored distributions describe completed steps only, never the pending slow step.'], 'runs': []}
    (a.output/'source.patch').write_text(subprocess.check_output(['git', 'diff', '--', 'src', 'CMakeLists.txt', 'tests/PhysicsStressLiveBenchmark.cpp'], cwd=ROOT, text=True))
    env = os.environ.copy()
    env.update(JUDAS_ENGINE_ROOT=str(ROOT), SDL_AUDIODRIVER='dummy', LC_ALL='C')
    for repeat in range(a.repeat):
        for frames in a.frames:
            for label in (['before', 'after'] if repeat % 2 == 0 else ['after', 'before']):
                if label not in a.labels:
                    continue
                name = f'{label}-{frames}-r{repeat}'
                dest = a.output/name
                cmd = [str(binaries[label]), str(a.scene), str(dest), str(a.timeout+30),
                       str(frames), '0', '0', '1', '0.016666667', str(a.profile), '0', '1']
                record = {'name': name, 'command': cmd, 'utc': datetime.datetime.now(datetime.timezone.utc).isoformat()}
                start = time.monotonic()
                with (a.output/(name+'.log')).open('w') as log:
                    process = subprocess.Popen(cmd, cwd=ROOT, env=env, stdout=log, stderr=subprocess.STDOUT, start_new_session=True)
                    timed_out = False
                    while True:
                        pid, status, usage = os.wait4(process.pid, os.WNOHANG)
                        if pid:
                            process.returncode = os.waitstatus_to_exitcode(status)
                            break
                        if time.monotonic()-start > a.timeout:
                            timed_out = True
                            os.killpg(process.pid, signal.SIGKILL)
                            _, status, usage = os.wait4(process.pid, 0)
                            process.returncode = os.waitstatus_to_exitcode(status)
                            break
                        time.sleep(.05)
                record.update(exit_code=process.returncode, timed_out=timed_out,
                              wall_seconds=time.monotonic()-start, native_peak_rss_bytes=usage.ru_maxrss*1024)
                path = dest/'steps.csv'
                rows = list(csv.DictReader(path.open())) if path.exists() else []
                record['completed_steps'] = len(rows)
                record['completed_requested_run'] = process.returncode == 0 and len(rows) >= frames
                if rows:
                    values = [float(r['fixed_ms']) for r in rows]
                    record.update(fixed_ms=distribution(values), sum_fixed_ms=sum(values),
                                  steps_over_60hz_budget=sum(v>1000/60 for v in values),
                                  impact_events=sum(int(r['impact_events']) for r in rows),
                                  impact_queries=sum(int(r['impact_queries']) for r in rows),
                                  nonfinite_fields=sum(int(r['nonfinite_fields']) for r in rows),
                                  final_digest=rows[-1]['trajectory_digest'])
                if (dest/'quality.json').exists():
                    record['endpoint_quality'] = json.loads((dest/'quality.json').read_text())
                receipt['runs'].append(record)
                (a.output/'RESULTS.json').write_text(json.dumps(receipt, indent=2)+'\n')
                print(name, 'COMPLETE' if record['completed_requested_run'] else 'CENSORED/FAILED',
                      len(rows), record.get('fixed_ms'), flush=True)
    receipt['sources_unchanged'] = sources == {n: sha(ROOT/n) for n in sources}
    receipt['binaries_unchanged'] = binary_hashes == {k: sha(v) for k, v in binaries.items()}
    receipt['scene_unchanged'] = receipt['scene_sha256'] == sha(a.scene)
    receipt['utc_end'] = datetime.datetime.now(datetime.timezone.utc).isoformat()
    (a.output/'RESULTS.json').write_text(json.dumps(receipt, indent=2)+'\n')
    return 0 if receipt['sources_unchanged'] and receipt['binaries_unchanged'] and receipt['scene_unchanged'] else 1

if __name__ == '__main__':
    raise SystemExit(main())
