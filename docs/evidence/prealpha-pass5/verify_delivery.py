#!/usr/bin/env python3
"""Read-only Pass5 evidence/source verification; no builds or test execution."""
from pathlib import Path
import argparse, datetime, gzip, hashlib, json, subprocess

def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def checked(condition, description):
    if not condition:
        raise RuntimeError(description)

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--root', type=Path, default=Path(__file__).resolve().parents[3])
parser.add_argument('--output', type=Path)
parser.add_argument('--local-artifacts', action='store_true', help='Also compare retained host binaries; not required in a source-only checkout')
args = parser.parse_args()
root = args.root.resolve()
base = root / 'docs/evidence/prealpha-pass5'
report = {'utc': datetime.datetime.now(datetime.timezone.utc).isoformat(), 'checks': {}}
count = 0
for manifest in sorted(base.rglob('MANIFEST.json')):
    rows = json.loads(manifest.read_text())['files']
    if isinstance(rows, dict):
        rows = [{'path': name, **row} for name, row in rows.items()]
    for row in rows:
        path = (manifest.parent / row['path']).resolve()
        checked(path.is_relative_to(manifest.parent), 'Unsafe manifest path: ' + str(path))
        checked(path.is_file() and path.stat().st_size == row['bytes'] and sha(path) == row['sha256'], 'Payload mismatch: ' + str(path))
        count += 1
report['checks']['manifestPayloadsMatched'] = count
for path in sorted(base.rglob('*.gz')):
    gzip.decompress(path.read_bytes())
report['checks']['gzipStreamsReadable'] = len(list(base.rglob('*.gz')))
fp = json.loads((base / 'FINAL_FINGERPRINTS.json').read_text())
for group in ['sourceFiles', 'qualifiedRuntimeSources', 'evidenceManifests']:
    for name, expected in fp[group].items():
        checked(sha(root / name) == expected, 'Fingerprint mismatch: ' + name)
report['checks']['sourceAndManifestFingerprintsMatched'] = sum(len(fp[g]) for g in ['sourceFiles', 'qualifiedRuntimeSources', 'evidenceManifests'])
for group in ['sourceFiles', 'qualifiedRuntimeSources']:
    for name in fp[group]:
        checked((root / name).is_file(), 'Missing source: ' + name)
parent = json.loads((root / 'docs/evidence/prealpha-pass4/FINDINGS.json').read_text())
current = json.loads((base / 'FINDINGS.json').read_text())
def previous(value):
    if isinstance(value, dict):
        return {k: previous(v) for k, v in value.items() if not k.startswith('pass5_')}
    if isinstance(value, list):
        return [previous(v) for v in value]
    return value
checked(previous(current) == parent, 'Inherited audit fields changed')
checked(len(current['findings']) == 54 and len(current['workstreams']) == 10, 'Original register coverage changed')
report['checks']['originalAuditFieldParity'] = True
for path in sorted(base.rglob('*')):
    if path.is_file():
        prefix = path.read_bytes()[:8]
        checked(not prefix.startswith((b'\x7fELF', b'MZ', b'!<arch>\n')), 'Executable/archive in evidence: ' + str(path))
report['checks']['noNativeBuildPayloads'] = True
if args.local_artifacts:
    checked(sha(Path(fp['linux']['archive'])) == fp['linux']['archiveSha256'], 'Linux engine archive changed')
    for name, expected in fp['linux']['binaries']['binaries'].items():
        checked(sha(root / 'build' / name) == expected, 'Linux executable changed: ' + name)
    checked(sha(root / '.cache/m88/android-arm64-v8a/libmain.so') == fp['android']['binarySha256'], 'Android runtime changed')
    report['checks']['retainedLocalBinaryHashesMatched'] = len(fp['linux']['binaries']['binaries']) + 2
accepted = fp['acceptedInput']
diff = subprocess.check_output(['git', 'diff', '--name-only', accepted, '--'], cwd=root, text=True).splitlines()
allowed = set(fp['sourceFiles'])
unexpected = [name for name in diff if name not in allowed and not name.startswith('docs/evidence/prealpha-pass5/')]
checked(not unexpected, 'Unrelated/protected tracked differences: ' + repr(unexpected))
report['checks']['protectedTrackedHistoryAndProjectsUnchanged'] = True
report['checks']['coordinatorLines'] = len((root / 'src/PhysicsWorld.cpp').read_text().splitlines())
checked(report['checks']['coordinatorLines'] == 859, 'Coordinator changed')
report['knownFailures'] = ['Original curved continuation:372 checks/4 failed assertions', 'Half-turn knots check21', 'FLU001 deferred unchanged/not rerun', 'Historical a106 save missing continuation information']
report['scope'] = 'Payload/source/Git inventory verification, not another physics/platform/human test run. Local binaries are separately identified in FINAL_FINGERPRINTS and original platform receipts.'
report['passed'] = True
if args.output:
    args.output.write_text(json.dumps(report, indent=2) + '\n')
print(json.dumps(report, indent=2))
