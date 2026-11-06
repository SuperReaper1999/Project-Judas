#!/usr/bin/env python3
"""Matched original-basin comparison. No simulation settings are changed."""
import argparse
import datetime
import decimal
import hashlib
import json
import pathlib
import statistics
import struct
import subprocess
import sys

parser = argparse.ArgumentParser()
parser.add_argument('reference', type=pathlib.Path)
parser.add_argument('candidate', type=pathlib.Path)
parser.add_argument('output', type=pathlib.Path)
parser.add_argument('--samples', type=int, default=3)
args = parser.parse_args()
if args.samples < 1 or sys.byteorder != 'little':
    parser.error('positive samples and a little-endian qualification host required')
args.output.mkdir(parents=True, exist_ok=True)
rows = []
for sample in range(args.samples):
    order = ('reference', 'candidate') if sample % 2 == 0 else ('candidate', 'reference')
    for kind in order:
        stem = args.output / f'{kind}-{sample}'
        executable = getattr(args, kind).resolve()
        command = [str(executable), '--fast' if kind == 'candidate' else '--reference', str(stem)]
        started = datetime.datetime.now(datetime.timezone.utc).isoformat()
        with stem.with_suffix('.log').open('w') as log:
            run = subprocess.run(['/usr/bin/time', '-v', '-o', str(stem.with_suffix('.resource')), 'timeout', '180s', *command], stdout=log, stderr=subprocess.STDOUT)
        stem.with_suffix('.exit').write_text(str(run.returncode)+'\n')
        if run.returncode:
            raise RuntimeError(f'{kind} failed; raw log preserved at {stem}.log')
        for line in stem.with_suffix('.log').read_text().splitlines():
            if line.startswith('BASIN '):
                data = dict(x.split('=', 1) for x in line.split()[1:] if '=' in x)
                if data['admitted'] != '1':
                    raise RuntimeError('original basin did not admit')
                rows.append({'kind': kind, 'sample': sample, 'startedUtc': started, 'rotated': bool(int(data['rotated'])), 'milliseconds': float(data['ms']), 'iterations': int(data['iterations']), 'normalizedResidual': float(data['normalized'])})
        (args.output/'timings.json').write_text(json.dumps(rows, indent=2)+'\n')
        print(kind, sample, 'passed', flush=True)

context = decimal.Context(prec=80)
def fields(path):
    data = path.read_bytes()
    offset = 0
    result = []
    for _ in range(4):
        n, = struct.unpack_from('<Q', data, offset)
        offset += 8
        values = []
        for _ in range(n):
            a, b = struct.unpack_from('<dd', data, offset)
            offset += 16
            values.append(context.add(decimal.Decimal.from_float(a), decimal.Decimal.from_float(b)))
        result.append(values)
    if offset != len(data):
        raise RuntimeError('unexpected state dump layout')
    return result

comparison = {}
for rotated, suffix in ((False, 'level'), (True, 'rotated')):
    reference = fields(args.output/f'reference-0-{suffix}.bin')
    candidate = fields(args.output/f'candidate-0-{suffix}.bin')
    comparison[suffix] = {}
    for name, a, b in zip(('pressurePa', 'waterM3', 'gasKg', 'intervalFluxM3'), reference, candidate):
        if len(a) != len(b):
            raise RuntimeError('field dimensions differ')
        diffs = [abs(context.subtract(x, y)) for x, y in zip(a, b)]
        comparison[suffix][name] = {'maximumAbsoluteDifference': str(max(diffs, default=decimal.Decimal(0))), 'l1Difference': str(sum(diffs, decimal.Decimal(0)))}
    for kind in ('reference', 'candidate'):
        original = (args.output/f'{kind}-0-{suffix}.bin').read_bytes()
        for sample in range(1, args.samples):
            if (args.output/f'{kind}-{sample}-{suffix}.bin').read_bytes() != original:
                raise RuntimeError(f'{kind} physical output was not deterministic')
    for kind in ('reference', 'candidate'):
        values = sorted(r['milliseconds'] for r in rows if r['kind'] == kind and r['rotated'] == rotated)
        comparison[suffix][kind] = {'samples': len(values), 'medianMs': statistics.median(values), 'p95Ms': values[-1], 'worstMs': values[-1]}
report = {'status': 'FIRST_INTERVAL_COMPARISON_ONLY', 'rows': rows, 'fields': comparison, 'p95Policy': 'nearest rank; with three samples p95 equals maximum; no broad tail-distribution claim', 'initialState': 'rest; solid frozen; original geometry, gravity, quantities and native float timestep', 'precision': 'binary64 correction plus compensated refinement and certified original operator residuals', 'deterministic': True}
(args.output/'comparison.json').write_text(json.dumps(report, indent=2)+'\n')
print(json.dumps(report['fields'], indent=2))
