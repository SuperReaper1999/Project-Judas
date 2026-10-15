#!/usr/bin/env python3
"""Matched Tuppence checks/timings using frozen before/after application binaries.

Runs sequentially on the current, unchanged ordinary game content. This captures
fixed-step/application work, not displayed FPS: the existing application fixture
renders selected screenshots and inspects script state outside physics timing.
"""
import argparse
import csv
import datetime
import hashlib
import json
import os
from pathlib import Path
import subprocess
import time

ROOT = Path(__file__).resolve().parents[1]


def sha(path):
    value = hashlib.sha256()
    with Path(path).open('rb') as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b''):
            value.update(block)
    return value.hexdigest()


def distribution(values):
    values.sort()
    return {name: values[int(q * (len(values) - 1))]
            for name, q in [('median', .5), ('p95', .95), ('maximum', 1)]}


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--baseline', type=Path, required=True)
    p.add_argument('--candidate', type=Path, required=True)
    p.add_argument('--output', type=Path, required=True)
    p.add_argument('--profile', type=int, choices=[0, 1], default=0)
    a = p.parse_args()
    a.output = a.output.resolve()
    if a.output.exists() and any(a.output.iterdir()):
        p.error('use a fresh output; original failures must be preserved')
    a.output.mkdir(parents=True, exist_ok=True)
    binaries = {'baseline': a.baseline.resolve(), 'candidate': a.candidate.resolve()}
    names = subprocess.check_output(['git', 'ls-files', 'src', 'projects/tuppence',
                                    'tests/BallPusherApplicationTests.cpp'], cwd=ROOT, text=True).splitlines()
    before = {name: sha(ROOT / name) for name in names}
    before.update({'binary:' + name: sha(path) for name, path in binaries.items()})
    receipt = {'utc': datetime.datetime.now(datetime.timezone.utc).isoformat(),
               'head': subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip(),
               'profile': a.profile, 'fingerprints': before, 'runs': [],
               'affinity': sorted(os.sched_getaffinity(0)) if hasattr(os, 'sched_getaffinity') else None,
               'note': 'Fixed-step CPU work; not GPU/displayed FPS. Before/after are separate sequential processes with identical project/fixture/input.'}
    traces = {}
    for mode, scene in [('play', 'cabinet'), ('coins', 'coins')]:
        for label, binary in binaries.items():
            dest = a.output / (label + '-' + mode)
            dest.mkdir()
            env = os.environ.copy()
            env.update(JUDAS_PROFILE=str(a.profile), JUDAS_PROFILE_OUTPUT=str(dest / 'profile.json'),
                       SDL_AUDIODRIVER='dummy')
            command = [str(binary), str(ROOT / 'projects/tuppence/Scenes' / (scene + '.judas')), str(dest), mode]
            start = time.monotonic()
            with (dest / 'stdout.log').open('w') as log:
                try:
                    code = subprocess.run(command, cwd=ROOT, env=env, stdout=log,
                        stderr=subprocess.STDOUT, timeout=240).returncode
                except subprocess.TimeoutExpired:
                    code = 'timeout'
            record = {'label': label, 'mode': mode, 'command': command, 'exit': code,
                      'seconds': time.monotonic() - start}
            csv_path = dest / 'steps.csv'
            if csv_path.is_file():
                rows = list(csv.DictReader(csv_path.open()))
                record['measured_fixed_frames'] = len(rows)
                if rows:
                    for column in ['physics_ms', 'fixed_ms', 'frame_work_ms']:
                        record[column] = distribution([float(row[column]) for row in rows])
                    columns = ['tick', 'balls', 'awake', 'pairs', 'contacts', 'events', 'queries']
                    traces[label, mode] = [[row[k] for k in columns] for row in rows]
            receipt['runs'].append(record)
            (a.output / 'results.json').write_text(json.dumps(receipt, indent=2) + '\n')
            print(label, mode, code, record.get('fixed_ms'), flush=True)
    after = {name: sha(ROOT / name) for name in names}
    after.update({'binary:' + name: sha(path) for name, path in binaries.items()})
    receipt['sources_and_binaries_unchanged'] = before == after
    receipt['physical_counter_trace_identical'] = {mode: traces.get(('baseline', mode)) == traces.get(('candidate', mode))
        and ('baseline', mode) in traces for mode in ['play', 'coins']}
    receipt['pass'] = all(run['exit'] == 0 and run.get('measured_fixed_frames', 0) > 0
                          for run in receipt['runs']) and before == after
    (a.output / 'results.json').write_text(json.dumps(receipt, indent=2) + '\n')
    return 0 if receipt['pass'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
