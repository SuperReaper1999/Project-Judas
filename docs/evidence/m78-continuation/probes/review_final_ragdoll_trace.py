#!/usr/bin/env python3
"""Preserve and analyze the root's completed run; never run physics/builds."""
import csv
import datetime
import hashlib
import json
import math
import shutil
import statistics
from pathlib import Path

ROOT = Path(__file__).resolve().parents[4]
SOURCE = ROOT / '.cache/m78-continuation/midpoint-final-concave-data'
OLD = ROOT / '.cache/m78-followup/concave-final-data'
OUTPUT = ROOT / 'docs/evidence/m78-continuation/final-performance'


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def rows(path):
    return [{key: float(value) for key, value in row.items()} for row in csv.DictReader(path.open())]


def correlation(trace, key):
    x, y = [row['wall_ms'] for row in trace], [row[key] for row in trace]
    mx, my = statistics.mean(x), statistics.mean(y)
    divisor = math.sqrt(sum((value-mx)**2 for value in x)*sum((value-my)**2 for value in y))
    return None if not divisor else sum((a-mx)*(b-my) for a, b in zip(x, y))/divisor


def summarize(trace):
    assert len(trace) == 900
    ordered = sorted(trace, key=lambda row: row['wall_ms'])
    threshold = ordered[854]['wall_ms']
    groups = {'all': trace, 'p95_tail': [row for row in trace if row['wall_ms'] >= threshold],
              'over16_67Ms': [row for row in trace if row['wall_ms'] > 16.67],
              'first120': trace[:120], 'last300': trace[600:]}
    group_summary = {}
    for name, group in groups.items():
        group_summary[name] = {'steps': len(group)}
        if group:
            group_summary[name].update(
                mean={key: statistics.mean(row[key] for row in group) for key in trace[0] if key != 'step'},
                solverShareOfSummedWall=sum(row['solver_ms'] for row in group)/sum(row['wall_ms'] for row in group))
    return dict(medianMsCSV=ordered[450]['wall_ms'], p95MsCSV=threshold, maxMsCSV=ordered[-1]['wall_ms'],
                groups=group_summary, top10=ordered[-10:][::-1],
                correlationWithWall={key: correlation(trace, key) for key in trace[0] if key not in ('step', 'wall_ms')})


def main():
    if OUTPUT.exists():
        raise SystemExit('Preserve prior analysis folder; choose a new destination')
    raw = OUTPUT / 'raw'
    raw.mkdir(parents=True)
    manifest = {}
    for case in ('box-1', 'box-4', 'mesh-1', 'mesh-4'):
        for suffix in ('.csv', '-profile.json'):
            source = SOURCE / (case+suffix)
            destination = raw / source.name
            shutil.copyfile(source, destination)
            assert sha(source) == sha(destination)
            manifest[str(destination.relative_to(ROOT))] = dict(bytes=destination.stat().st_size,
                                                                sha256=sha(destination),
                                                                copiedFrom=str(source.relative_to(ROOT)))
    receipts = ['docs/evidence/m78-continuation/midpoint-final-concave.log',
                'docs/evidence/m78-continuation/midpoint-final-concave.json',
                'docs/evidence/m78-continuation/midpoint-final-concave-build.json']
    exact = {}
    for line in (ROOT/receipts[0]).read_text().splitlines():
        if line.startswith('{"case":'):
            entry = json.loads(line)
            exact[entry['case']] = entry
    cases = {}
    for case in ('box-1', 'box-4', 'mesh-1', 'mesh-4'):
        final_csv = SOURCE / (case+'.csv')
        old_csv = OLD / (case+'.csv')
        final_profile = json.loads((SOURCE/(case+'-profile.json')).read_text())
        cases[case] = dict(rootRunExactSummary=exact[case], originalCSV=summarize(rows(old_csv)),
                          finalCSV=summarize(rows(final_csv)),
                          rawInputSHA256={str(path.relative_to(ROOT)): sha(path) for path in (old_csv, final_csv)},
                          finalProfile=dict(frames=len(final_profile['frames']), spikes=len(final_profile['spikes']),
                                            diagnostics=final_profile.get('diagnostics'), limits=final_profile.get('limits')))
    result = dict(
        utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),
        scope='Read-only analysis of the root completed combined implicit-midpoint benchmark; no simulation/build',
        cases=cases,
        receiptSHA256={path: sha(ROOT/path) for path in receipts},
        calculatorSHA256=sha(Path(__file__)),
        limitations=[
            'CSV timing fields use original truncated precision; exact aggregate timings are copied from root stdout.',
            'Final profiles retain last 120 frames and have no retained spikes; first-impact expensive steps are absent from scoped history.',
            'Early-step attribution is therefore based on CSV solver/broadphase/contact timers and CCD counts, not an invented final phase decomposition.',
            'Different legitimate trajectory from the original and the earlier predictor-only combined run; do not claim physical-counter parity for those corrections.',
            'Isolation-only 14.963051 ms p95 establishes Boolean-work attribution separately.',
            'Sleeping counts are measured, not forced; final mesh-4 has 26/52 sleeping and mesh-1 0/13.',
            'Final source/Windows acceptance remains the root investigation responsibility.',
        ])
    analysis = OUTPUT / 'ANALYSIS.json'
    analysis.write_text(json.dumps(result, indent=2)+'\n')
    manifest[str(analysis.relative_to(ROOT))] = dict(bytes=analysis.stat().st_size, sha256=sha(analysis))
    for path in receipts:
        manifest[path] = dict(bytes=(ROOT/path).stat().st_size, sha256=sha(ROOT/path))
    manifest[str(Path(__file__).relative_to(ROOT))] = dict(bytes=Path(__file__).stat().st_size, sha256=sha(Path(__file__)))
    (OUTPUT/'RAW_FINGERPRINTS.json').write_text(json.dumps(dict(
        scope='Immutable copied raw measurements and read-only analysis only; not final candidate/source fingerprints',
        files=manifest), indent=2)+'\n')
    final = cases['mesh-4']['finalCSV']
    print(json.dumps(dict(output=str(OUTPUT.relative_to(ROOT)), overGate=final['groups']['over16_67Ms']['steps'],
                          tailSolverShare=final['groups']['p95_tail']['solverShareOfSummedWall'],
                          correlations=final['correlationWithWall'])))


if __name__ == '__main__':
    main()
