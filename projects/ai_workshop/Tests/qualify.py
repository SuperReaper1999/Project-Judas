#!/usr/bin/env python3
"""Inspect genuine native application output; does not emulate AI or physics."""
import argparse
import csv
import json
from pathlib import Path

parser = argparse.ArgumentParser()
parser.add_argument('--watch-log', type=Path, required=True)
parser.add_argument('--manual-log', type=Path, required=True)
args = parser.parse_args()


def load(path):
    text = path.read_text()
    for error in ['Script exception', 'script fault', 'ReferenceError:', 'TypeError:', 'SyntaxError:']:
        assert error not in text, (path, error)
    records = [json.loads(line.split('AI_WORKSHOP ', 1)[1])
               for line in text.splitlines() if line.startswith('JS: AI_WORKSHOP ')]
    assert len(records) >= 30, (path, 'missing executed AI observations', len(records))
    return text, records


watch_text, watch = load(args.watch_log)
manual_text, manual = load(args.manual_log)
assert watch[-1]['time'] >= 75, 'watch did not actually run its declared interval'
for key in ['meals', 'rested', 'chats', 'shelters']:
    assert watch[-1][key] > 0, ('missing completed gameplay effect', key, watch[-1])
assert min(row['health'] for row in watch) < .8, 'no genuine injury observed'
low = min(range(len(watch)), key=lambda n: watch[n]['health'])
assert any(row['health'] > watch[low]['health'] + .15 for row in watch[low + 1:]), 'no post-injury recovery'
for key, required in [('citizen', {'eat', 'rest', 'socialize'}), ('guard', {'hide', 'recover', 'patrol'})]:
    actions = {row[key]['action'] for row in watch if row[key]}
    assert required <= actions, ('missing selected native action', key, required - actions)
# F at45s closes food, F at55s restocks. Choose interior observations, not UI boundary frames.
closed = [row for row in manual if 46 < row['time'] < 54]
assert len(closed) >= 3
assert all(row['pantry'] == 0 for row in closed), 'food was not actually unavailable'
assert all(not row['citizen'] or row['citizen']['action'] != 'eat' for row in closed), 'ineligible eat action selected'
assert len({row['meals'] for row in closed}) == 1, 'empty canteen produced meals'
assert any(row['citizen'] and row['citizen']['action'] == 'eat' for row in manual if row['time'] > 56), 'restocked canteen never regained eligibility'
# The ordinary application harness logs real MK2 body positions, not project-reported animation.
header = next(line for line in watch_text.splitlines() if line.startswith('step,time,'))
rows = list(csv.DictReader([header] + [line for line in watch_text.splitlines()
    if line.split(',')[0].isdigit() and len(line.split(',')) == len(header.split(','))]))
assert len(rows) > 20
for name in ['obj0', 'obj1']:
    for axis in ['X', 'Y']:
        coords = [float(row[name + 'Pos' + axis]) for row in rows]
        assert max(coords) - min(coords) > 1, ('physical actor did not traverse map', name, axis)
print(json.dumps({'watch_records': len(watch), 'manual_records': len(manual),
    'watch_final': watch[-1], 'physical_rows': len(rows), 'result': 'passed'}, indent=2))
