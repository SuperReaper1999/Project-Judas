#!/usr/bin/env python3
"""Run matched public whole-engine workloads serially and detect p99 regressions.

Prepare inputs with prepare_workloads.py. Never run this beside compilation or
other benchmarks. This checks relative performance; product budgets, exceptional
hitches and discarded simulation time remain separate acceptance requirements.
"""
import argparse
import hashlib
import json
import math
from pathlib import Path
import shutil
import subprocess
import sys
from compare_captures import load
from performance.comparison import compare

CASES = {
    'lightweight': ('developer_lab/developer_lab.judasproj', []),
    'town': ('willow_town/Willow_Town.judasproj', []),
    'populated-rendering': ('renderer_v2_lab/renderer_v2_lab.judasproj', []),
    'animation': ('animation_studio/animation_studio.judasproj', []),
    'animation-ik': ('animation-ik/animation_studio.judasproj', []),
    'particles': ('particle_v2_lab/particle_v2_lab.judasproj', []),
    'basic-water': ('basic_water_lab/basic_water_lab.judasproj', []),
    'terrain-excavation': ('terrain_v2_workshop/terrain_v2_workshop.judasproj', ['--pulse', 'key:T']),
    'terrain-large': ('terrain-large/terrain_v2_workshop.judasproj', ['--key', 'key:W']),
    'vehicle': ('vehicle-cycle/Vehicle_V2_Workshop.judasproj', ['--key', 'key:W']),
    'fleet': ('vehicle-fleet-cycle/Vehicle_V2_Workshop.judasproj', ['--key', 'key:W']),
    'combined': ('combined/terrain_v2_workshop.judasproj', []),
}


def verify_state(capture, case):
    """Game-owned observable outcomes, independent of a fast exit code."""
    if case not in ('terrain-excavation', 'combined'):
        return
    prefix = 'WHOLE_ENGINE_TERRAIN ' if case == 'terrain-excavation' else 'WHOLE_ENGINE_COMBINED '
    rows = [json.loads(line.split(prefix, 1)[1])
            for line in (capture / 'application.log').read_text().splitlines() if prefix in line]
    if case == 'terrain-excavation':
        valid = any(r['revision'] > 1 and r['volumeChunks'] > 0 and r['remaining'] == 0
                    and r['ready'] and not r['busy'] for r in rows)
    else:
        policy = json.loads((capture / 'invocation.json').read_text())['graphics_policy']
        backend = 'cpu' if policy == 'portable' else 'gpu'
        valid = bool(rows) and rows[-1]['actors'] == 32 and rows[-1]['live'] == 5000
        valid = valid and rows[-1]['backend'] == backend and rows[-1]['contacts'] > 0
        valid = valid and rows[-1]['supported'] == 32 and 2.5 <= rows[-1]['minY'] <= rows[-1]['maxY'] <= 4
    if not valid:
        raise RuntimeError(f'{case}: required live workload state was not demonstrated in {capture}')
    (capture / 'WORKLOAD_STATE.json').write_text(json.dumps(dict(case=case, passed=True, observations=rows), indent=2) + '\n')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--baseline', type=Path, required=True)
    parser.add_argument('--candidate', type=Path, required=True)
    parser.add_argument('--workloads', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--cases', choices=CASES, nargs='+', default=list(CASES))
    parser.add_argument('--seconds', type=float, default=20)
    parser.add_argument('--repeats', type=int, default=3)
    parser.add_argument('--resolution', default='1280x720')
    parser.add_argument('--cap', type=int, default=0)
    parser.add_argument('--no-profile', action='store_true')
    parser.add_argument('--p99-regression-fraction', type=float, default=.10)
    args = parser.parse_args()
    if not 3 <= args.repeats <= 10 or not 10 <= args.seconds <= 300:
        parser.error('use 3–10 repetitions and 10–300 seconds per capture')
    if not 0 <= args.cap <= 1000 or not math.isfinite(args.p99_regression_fraction) or not 0 <= args.p99_regression_fraction <= 1:
        parser.error('invalid cap or p99 regression tolerance')
    if len(set(args.cases)) != len(args.cases):
        parser.error('select each case once')
    workloads, out = args.workloads.resolve(), args.out.resolve()
    if not (workloads / 'WORKLOADS.json').is_file():
        parser.error('prepare and freeze inputs first')
    state_contract = json.loads((workloads / 'WORKLOADS.json').read_text()).get('state_contract', 0)
    for case in args.cases:
        if not (workloads / CASES[case][0]).is_file():
            parser.error(f'{case} is absent; prepare its inputs or explicitly select available cases')
    out.mkdir(parents=True, exist_ok=False)
    # Freeze executables so an unrelated incremental rebuild cannot change a
    # campaign halfway through. The original files and branches are untouched.
    binaries = {}
    for mode, source in [('baseline', args.baseline), ('candidate', args.candidate)]:
        source = source.resolve()
        destination = out / (mode + source.suffix)
        shutil.copy2(source, destination)
        binaries[mode] = destination
    campaign = dict(schema=1, cases=args.cases, results={},
                    binaries={name: dict(path=str(path), sha256=hashlib.sha256(path.read_bytes()).hexdigest())
                              for name, path in binaries.items()},
                    scope='Relative p99 regression gate. Not human acceptance or an absolute frame-budget certification.')
    (out / 'RESULTS.json').write_text(json.dumps(campaign, indent=2) + '\n')
    runner = Path(__file__).with_name('run_whole_engine.py')
    for case in args.cases:
        relative, controls = CASES[case]
        paths = dict(baseline=[], candidate=[])
        duration = max(args.seconds, 45) if case == 'animation-ik' else args.seconds
        for repeat in range(args.repeats):
            for mode in (['baseline', 'candidate'] if repeat % 2 == 0 else ['candidate', 'baseline']):
                capture = out / f'{case}-{repeat}-{mode}'
                command = [sys.executable, str(runner), '--binary', str(binaries[mode]),
                           '--project', str(workloads / relative), '--workload-manifest',
                           str(workloads / 'WORKLOADS.json'), '--out', str(capture),
                           '--seconds', str(duration), '--resolution', args.resolution,
                           '--cap', str(args.cap), *controls]
                if args.no_profile:
                    command.append('--no-profile')
                subprocess.run(command, check=True)
                if state_contract >= 1:
                    verify_state(capture, case)
                paths[mode].append(capture)
        old, new = [load(p) for p in paths['baseline']], [load(p) for p in paths['candidate']]
        result = compare(old, new, args.p99_regression_fraction)
        result['candidate_discarded_seconds'] = max((r['summary']['physics_discarded_seconds'] or 0)
                                                    for r in new) if not args.no_profile else None
        result['candidate_worst_within_60fps'] = result['exceptional_worst_ms'] <= 1000 / 60
        campaign['results'][case] = result
        (out / 'RESULTS.json').write_text(json.dumps(campaign, indent=2) + '\n')
        print(json.dumps(dict(case=case, **result)), flush=True)
    return 0 if all(result['regression_pass'] for result in campaign['results'].values()) else 1


if __name__ == '__main__':
    raise SystemExit(main())
