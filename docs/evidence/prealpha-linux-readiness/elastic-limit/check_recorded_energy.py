#!/usr/bin/env python3
"""Classify the preserved ideal-elastic probe; intentionally fails its open case.

This checks the retained raw measurement, not a substitute for running probe.cpp.
The tolerance only distinguishes float roundoff from material energy creation;
it does not alter any engine or existing acceptance threshold.
"""
from pathlib import Path
import re
import sys
source = Path(sys.argv[1]) if len(sys.argv) > 1 else Path(__file__).with_name('native-results.log')
failed = []
for line in source.read_text().splitlines():
    result = re.search(r'ratio([0-9.e+-]+)', line)
    if result and float(result[1]) > 1.0001:
        failed.append(line)
for line in failed:
    print('FAIL ideal-elastic no-force energy creation: ' + line)
print(f'{len(failed)} preserved failing energy cases; not a green physics gate')
sys.exit(1 if failed else 0)
