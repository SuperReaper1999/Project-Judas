#!/usr/bin/env python3
"""Opt-in scoped receipt attachment; run only after all native writers release.

Default is a dry-run description. --write updates only the two NEW closure JSON
overlays, never the frozen register/crosswalk or any historical evidence. This
does not run tests, invent coverage, classify an unexecuted probe or promote any
copied baseline platform state. Stop before the final inventory is captured.

Example after writing release:
  python3 docs/evidence/m78-closure/probes/attach_coverage_receipts.py \
    --native-summary docs/evidence/m78-closure/native/SUMMARY.json \
    --save-exclusion docs/evidence/m78-closure/audit-lifecycle/save-exclusion/linux-current-01/fixture/RESULT.json \
    --write
"""
import argparse
import hashlib
import json
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[4]
DEST = ROOT / 'docs/evidence/m78-closure'
COVERAGE = DEST / 'coverage'
GROUPS = {
    'publication_isolation': ['A.04', 'A.06', 'C.05', 'D.07', 'D.10', 'I.01', 'I.03'],
    'physics_continuation': ['A.04', 'A.06', 'C.05', 'C.08', 'D.07', 'D.10', 'I.01', 'I.03'],
    'mixed_balance': ['A.06', 'D.01', 'D.07', 'D.10', 'I.01', 'I.03'],
    'mixed_endpoint_v2': ['A.06', 'D.07', 'D.10', 'I.01', 'I.03'],
    'mixed_physical_endpoint': ['A.06', 'D.07', 'D.10', 'I.01', 'I.03'],
    'ordinary_physical_endpoint': ['D.07', 'D.10', 'I.01', 'I.03'],
    'legacy_and_new_cold': ['D.07', 'D.10', 'I.01', 'I.03'],
    'balance_lab': ['D.07', 'D.10', 'I.01', 'I.03'],
    'character_lab': ['D.01', 'I.01', 'I.03'],
    'save_streaming': ['I.01', 'I.03', 'I.04', 'J.08'],
    'signal_application': ['A.02', 'A.07', 'H.08', 'I.01', 'I.04'],
    'signals': ['A.02', 'A.07', 'H.08', 'I.01', 'I.04'],
}


def read(path):
    return json.loads(Path(path).read_text(encoding='utf-8-sig'))


def owned(value):
    path = Path(value)
    if not path.is_absolute():
        path = ROOT / path
    if path.is_symlink():
        raise ValueError('Receipt symlinks are not followed: '+str(value))
    path = path.resolve()
    if not path.is_relative_to(DEST) or not path.is_file():
        raise ValueError('Receipt must be an ordinary existing closure file: '+str(value))
    return path


def receipt(value):
    path = owned(value)
    return {'path': path.relative_to(ROOT).as_posix(),
            'sha256': hashlib.sha256(path.read_bytes()).hexdigest()}


def slug(value):
    return re.sub(r'[^a-z0-9]+', '-', str(value).lower()).strip('-')


def paths(value):
    if isinstance(value, str):
        return [value]
    if isinstance(value, list):
        return [x['path'] if isinstance(x, dict) else x for x in value]
    if isinstance(value, dict):
        return [value['path']]
    raise ValueError('Expected an explicit receipt path or list, not '+repr(value))


def result_of_runs(runs):
    if not runs:
        return 'not-executed', 'No executed runs; no pass inferred.'
    for run in runs:
        if 'exitCode' not in run:
            raise ValueError('Executed native run lacks actual exitCode: '+repr(run))
        for field in ('checks', 'failures'):
            if field in run and run[field] is not None and (
                    not isinstance(run[field], int) or isinstance(run[field], bool) or run[field] < 0):
                raise ValueError('Invalid actual assertion count: '+repr(run))
    failed = [run for run in runs if run['exitCode'] != 0 or (run.get('failures') or 0) > 0]
    if failed:
        # expectedExitCode is deliberately ignored: a failed assertion is not
        # successful merely because a launcher expected the failing exit.
        return 'failed-within-envelope', 'Actual nonzero exit/failed assertions retained; expected exits do not turn failures into passes.'
    if all(run.get('checks') is not None and run.get('failures') == 0 for run in runs):
        return 'pass-within-envelope', 'Actual recorded assertions pass within these exact native functional routes.'
    return 'executed-exit-zero-within-smoke-scope', 'Exit-zero route only where assertion counts are absent; no member semantics inferred.'


def frozen_invariants(register, overlay, crosswalk, requirements):
    if len(register['features']) != 485 or len(crosswalk) != 77:
        raise ValueError('Frozen register/crosswalk identity count changed')
    if [x['id'] for x in register['features']] != [x['id'] for x in overlay['features']]:
        raise ValueError('Baseline feature IDs/order changed')
    for old, new in zip(register['features'], overlay['features']):
        for platform in ('linux', 'windows'):
            value = old['evidence'][platform]
            if isinstance(value, dict):
                value = value['status']
            if new['baseline'][platform] != value:
                raise ValueError('Copied baseline platform flag changed: '+old['id'])
    if [x['milestone'] for x in crosswalk] != [x['milestone'] for x in requirements['milestones']]:
        raise ValueError('Original milestone identities/order changed')
    for old, new in zip(crosswalk, requirements['milestones']):
        if (old['status'] != new['frozenStatus'] or
                old['missingOriginalBrief'] != new['frozenMissingOriginalBrief']):
            raise ValueError('Original milestone flags changed')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--native-summary', required=True, type=Path)
    parser.add_argument('--native-result', action='append', default=[], type=Path,
                        help='Additional final run receipt, attached with actual exit metadata only')
    parser.add_argument('--endpoint', action='append', default=[], type=Path,
                        help='Additional actual physical endpoint comparison; no controller-only witness')
    parser.add_argument('--save-exclusion', type=Path,
                        help='Actual executed legal simple-actor exclusion receipt; never a queued source file')
    parser.add_argument('--write', action='store_true', help='Write only NEW closure overlay JSON files')
    args = parser.parse_args()
    if (DEST / 'FINAL_FINGERPRINTS.json').exists():
        raise ValueError('Final inventory already captured; do not mutate the frozen final graph')
    summary_path = owned(args.native_summary)
    summary = read(summary_path)
    if not isinstance(summary.get('cases'), list) or 'exactEngineIdentity' not in summary:
        raise ValueError('Final native summary requires cases and exactEngineIdentity')
    index = read(COVERAGE / 'CLOSURE_CASES.json')
    overlay = read(COVERAGE / 'CAPABILITIES_BASELINE_OVERLAY.json')
    requirements = read(COVERAGE / 'REQUIREMENT_OVERLAY.json')
    register = read(ROOT / 'docs/evidence/m78/CAPABILITIES.json')
    crosswalk = read(ROOT / 'docs/evidence/m78/REQUIREMENT_CROSSWALK.json')
    frozen_invariants(register, overlay, crosswalk, requirements)
    before_baselines = [(x['id'], dict(x['baseline'])) for x in overlay['features']]
    additions = []
    for entry in summary['cases']:
        name = entry['id']
        runs = entry.get('executedRuns', [])
        if not runs:
            continue  # Outstanding scopes remain explicitly unexecuted.
        verdict, meaning = result_of_runs(runs)
        additions.append({
            'id': 'native-final-'+slug(name),
            'relatedFeatureIds': entry.get('relatedFeatureIds', GROUPS.get(name, [])),
            'status': 'SCOPED FINAL NATIVE FUNCTIONAL RECEIPT — '+verdict.upper(),
            'linuxStatus': 'not-a-new-Linux-execution', 'windowsStatus': verdict,
            'receipts': [receipt(summary_path)]+[receipt(p) for p in paths(entry['receipt'])],
            'oracle': entry['scope'], 'executedRuns': runs,
            'observedResult': meaning,
            'exactEngineIdentity': {'summaryReceipt': receipt(summary_path),
                                    'meaning': 'Exact archive/replacement-object/source/binary identities are retained once in the immutable linked native summary.'},
            'remaining': 'Only named functional routes. No unexecuted public-member, physical GPU/controller/audio/focus, human or Windows performance acceptance.'})
    for path in args.native_result:
        data = read(owned(path))
        verdict, meaning = result_of_runs([data])
        additions.append({'id': 'native-run-'+slug(owned(path).stem),
                          'relatedFeatureIds': GROUPS.get(data.get('case'), []),
                          'status': 'SCOPED NATIVE RUN RECEIPT — '+verdict.upper(),
                          'linuxStatus': 'not-a-new-Linux-execution', 'windowsStatus': verdict,
                          'receipts': [receipt(path)], 'observedResult': meaning,
                          'runMetadata': data,
                          'remaining': 'Raw run identity/exit only unless independent assertion/physical oracle is recorded separately.'})
    endpoint_paths = list(args.endpoint)
    for value in summary.get('physicalEndpointComparisons', []):
        endpoint_paths.extend(paths(value))
    for path in dict.fromkeys(map(str, endpoint_paths)):
        data = read(owned(path))
        endpoint_receipts = [receipt(path)]
        if 'actualEndpoint' in data:
            if not isinstance(data['actualEndpoint'], dict):
                raise ValueError('Actual endpoint must be an explicit physical observation: '+path)
        elif isinstance(data.get('rows'), list) and data['rows']:
            # The final native ordinary-reader receipt stores summaries under
            # rows[].endpoint and names the separately archived body/contact
            # records. Verify those concrete bytes instead of treating a cached
            # controller flag or the mere collection name as a physical witness.
            for row in data['rows']:
                endpoint = row.get('endpoint', row.get('actualEndpoint'))
                if not isinstance(endpoint, dict) or 'currentPhysicalStanding' not in endpoint:
                    raise ValueError('Ordinary endpoint row lacks actual standing observation: '+path)
                record_receipt = receipt(row['actualRecordPath'])
                if record_receipt['sha256'] != row['actualRecordSHA256']:
                    raise ValueError('Ordinary actual-record hash differs: '+row['actualRecordPath'])
                record = read(owned(row['actualRecordPath']))
                if not isinstance(record.get('bodies'), list) or not record['bodies']:
                    raise ValueError('Ordinary endpoint row has no archived physical bodies: '+path)
                physical = record.get('currentPhysicalEndpoint')
                if not isinstance(physical, dict) or any(physical.get(k) != v for k, v in endpoint.items()):
                    raise ValueError('Ordinary endpoint summary differs from actual body/contact record: '+path)
                endpoint_receipts.append(record_receipt)
        else:
            raise ValueError('Endpoint comparison must include actual physical endpoint data: '+path)
        additions.append({'id': 'native-actual-endpoint-'+slug(owned(path).stem),
                          'relatedFeatureIds': ['D.07', 'D.10', 'I.01', 'I.03'],
                          'status': 'SCOPED ACTUAL NATIVE PHYSICAL ENDPOINT OBSERVATION',
                          'linuxStatus': 'not-a-new-Linux-execution',
                          'windowsStatus': 'actual-observation-within-scope',
                          'receipts': endpoint_receipts, 'oracle': data.get('scope'),
                          'observedResult': data,
                          'remaining': 'Only the explicitly identified physical endpoint route. Ordinary stand/recovery reads are not uninterrupted trajectory equality; mixed equality applies only to its named pair with documented fresh native identity exclusion. No Linux/Windows bitwise trajectory or arbitrary recovery promise.'})
    if args.save_exclusion:
        data = read(owned(args.save_exclusion))
        checks, failures = data.get('checks'), data.get('failures')
        if not isinstance(checks, list) or not checks or not isinstance(failures, int):
            raise ValueError('Save-exclusion receipt must contain actual nonempty assertion records and failure count')
        if sum(not row['passed'] for row in checks) != failures:
            raise ValueError('Save-exclusion assertion records/count disagree')
        additions.append({'id': 'legal-simple-actor-save-exclusion',
                          'relatedFeatureIds': ['A.04', 'A.06', 'C.05', 'I.01', 'I.03'],
                          'status': 'EXECUTED LINUX SAVE-EXCLUSION PROBE — '+('FAILURE' if failures else 'PASS WITHIN ENVELOPE'),
                          'linuxStatus': 'failed-within-envelope' if failures else 'pass-within-envelope',
                          'windowsStatus': 'not-executed-for-this-case',
                          'receipts': [receipt(args.save_exclusion)],
                          'oracle': data.get('contract', data.get('scope')),
                          'observedResult': {'checks': len(checks), 'failures': failures},
                          'remaining': 'Only actually executed legal simple dynamic-actor exclude/control paths. Pre-existing/regression/mechanism classification requires the separately reviewed source/participant witness; a queued probe is not a failure.'})
    existing = {row['id']: row for row in index['cases']}
    valid_ids = {row['id'] for row in overlay['features']}
    for row in additions:
        if not set(row['relatedFeatureIds']) <= valid_ids:
            raise ValueError('Unknown original capability ID in attachment')
        if row['id'] in existing:
            if row != existing[row['id']]:
                raise ValueError('Refusing to rewrite existing case: '+row['id'])
            continue
        index['cases'].append(row)
        existing[row['id']] = row
    index['nativeClosureSummary'] = {
        'receipt': receipt(summary_path), 'outstandingScopes': summary.get('outstandingScopes', []),
        'toolingFailures': summary.get('toolingFailures', []),
        'meaning': 'Named actual native functional results; baseline platform flags and unexecuted semantics stay unchanged.'}
    for row in overlay['features']:
        row['closureCaseIds'] = [case['id'] for case in index['cases'] if row['id'] in case['relatedFeatureIds']]
    if before_baselines != [(x['id'], x['baseline']) for x in overlay['features']]:
        raise ValueError('Attachment changed an original ID/baseline')
    frozen_invariants(register, overlay, crosswalk, requirements)
    if args.write:
        # Current docs/remaining-finding prose is deliberately not generated. It
        # needs a human-readable final reconciliation of actual scopes/gaps.
        (COVERAGE / 'CLOSURE_CASES.json').write_text(json.dumps(index, indent=2, ensure_ascii=False)+'\n')
        head = {key: value for key, value in overlay.items() if key != 'features'}
        compact = json.dumps(head, indent=2, ensure_ascii=False)[:-2]+',\n  \"features\": [\n'
        compact += ',\n'.join('    '+json.dumps(row, ensure_ascii=False, separators=(',', ':')) for row in overlay['features'])
        (COVERAGE / 'CAPABILITIES_BASELINE_OVERLAY.json').write_text(compact+'\n  ]\n}\n')
    print(json.dumps({'write': args.write, 'scopedAttachments': [x['id'] for x in additions],
                      'baselineRowsPreserved': 485, 'originalMilestoneIdentitiesPreserved': 77,
                      'nativeOutstandingScopes': summary.get('outstandingScopes', [])}, indent=2))


if __name__ == '__main__':
    try:
        main()
    except (KeyError, OSError, TypeError, ValueError) as error:
        raise SystemExit('FAIL: '+str(error))
