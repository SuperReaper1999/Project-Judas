#!/usr/bin/env python3
"""Compare repeated, identity-matched whole-frame captures; exit 1 on regression."""
import argparse
import json
from pathlib import Path
import sys
from performance.comparison import compare


def load(path):
    identity = json.loads((path / 'invocation.json').read_text())
    summary = json.loads((path / 'summary.json').read_text())
    if identity['exit'] != 0:
        raise ValueError('capture process failed: ' + str(path))
    return dict(identity=identity, summary=summary,
                dropped_events=summary.get('diagnostic_dropped_events', 0),
                truncated_records=summary.get('diagnostic_truncated_records', 0))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--baseline', type=Path, nargs='+', required=True)
    parser.add_argument('--candidate', type=Path, nargs='+', required=True)
    parser.add_argument('--p99-regression-fraction', type=float, default=.10)
    args = parser.parse_args()
    try:
        result = compare([load(path) for path in args.baseline],
                         [load(path) for path in args.candidate], args.p99_regression_fraction)
        print(json.dumps(result, indent=2))
        return 0 if result['regression_pass'] else 1
    except (ValueError, OSError, KeyError) as error:
        print('Invalid comparison: ' + str(error), file=sys.stderr)
        return 2


if __name__ == '__main__':
    raise SystemExit(main())
