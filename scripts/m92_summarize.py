#!/usr/bin/env python3
"""Summarize retained M92 observations; never runs a benchmark or changes fixtures."""
import argparse
import csv
import json
import re
from pathlib import Path


def quantiles(values):
    values = sorted(values)
    return {name: values[int(q * (len(values) - 1))]
            for name, q in [('median', .5), ('p95', .95), ('max', 1)]}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('measure', type=Path)
    parser.add_argument('--watch', type=Path)
    parser.add_argument('--pixels', action='store_true', help='requires Pillow; compares equal-light workloads only')
    args = parser.parse_args()
    receipt = json.loads((args.measure / 'RESULTS.json').read_text())
    root = Path(__file__).resolve().parents[1]
    result = {'measurement': receipt, 'cases': [], 'pixelComparisons': [], 'triangleComparisons': []}
    for run in receipt['runs']:
        folder = root / run['path']
        current = None
        for line in (folder / 'stdout.log').read_text().splitlines():
            if line.startswith('MEASURE '):
                current = {'stage': run['stage'], 'round': run['round'], 'workload': run['name']}
                for key, value in re.findall(r'(\w+)=([\d.]+)', line):
                    current[key] = float(value) if '.' in value else int(value)
                result['cases'].append(current)
            elif current is not None and line.startswith(('CPU_WORK ', 'PAYLOAD ')):
                current[line.split()[0]] = {k: float(v) for k, v in re.findall(r'(\w+)=([\d.]+)', line)}
        match = re.search(r'Maximum resident set size \(kbytes\): (\d+)', (folder / 'memory.txt').read_text())
        run['peakRSSKiB'] = int(match.group(1)) if match else None
    if args.pixels:
        from PIL import Image, ImageChops, ImageStat
        for before in sorted(args.measure.glob('*-baseline-*/*.png')):
            if 'objects' in str(before.parent) and before.stem.split('-')[1] == '128':
                continue  # Baseline silently truncated 128 lights to five: not equal quality.
            after = Path(str(before).replace('-baseline-', '-candidate-'))
            difference = ImageChops.difference(Image.open(before).convert('RGB'), Image.open(after).convert('RGB'))
            stat = ImageStat.Stat(difference)
            result['pixelComparisons'].append({'case': str(before.relative_to(args.measure)),
                'meanAbsolute8BitChannelError': sum(stat.mean) / 3,
                'max8BitChannelError': max(x[1] for x in difference.getextrema())})
    for before in sorted(args.measure.glob('*-baseline-*/samples.csv')):
        after = Path(str(before).replace('-baseline-', '-candidate-'))
        with before.open() as f:
            a = list(csv.DictReader(f))
        with after.open() as f:
            b = list(csv.DictReader(f))
        same = len(a) == len(b) and all(x['triangles'] == y['triangles'] for x, y in zip(a, b))
        result['triangleComparisons'].append({'case': str(before.relative_to(args.measure)), 'identical': same})
        if not same:
            raise RuntimeError('Submitted geometry changed: ' + str(before))
    if args.watch:
        with (args.watch / 'frames.csv').open() as f:
            rows = list(csv.DictReader(f))[30:]
        result['watch'] = {'warmupFrames': 30, 'samples': len(rows),
            'metrics': {key: quantiles([float(r[key]) for r in rows]) for key in rows[0] if key != 'frame'},
            'effectiveFPS': 1000 / (sum(float(r['interval_ms']) for r in rows) / len(rows))}
    (args.measure / 'SUMMARY.json').write_text(json.dumps(result, indent=2) + '\n')
    print(f'{len(result["cases"])} cases; {len(result["pixelComparisons"])} matched images; SUMMARY.json written')


if __name__ == '__main__':
    main()
