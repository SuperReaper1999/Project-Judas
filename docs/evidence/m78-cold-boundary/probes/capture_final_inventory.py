#!/usr/bin/env python3
"""Opt-in final capture / verification. Read Git; never stage or mutate .git.

Prepare first, capture only after the root's reports/matrix are complete.
Generated-file graph: CHANGED_FILES.txt lists itself and FINAL_FINGERPRINTS.json;
its text is hashed by the manifest. The manifest excludes itself and the optional
later FINAL_VERIFICATION.json. Historical manifests are never regenerated.
"""
import argparse
import datetime
import fnmatch
import hashlib
import json
import os
import re
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[4]
DEST = 'docs/evidence/m78-cold-boundary/'
CHANGED = DEST+'CHANGED_FILES.txt'
FINAL = DEST+'FINAL_FINGERPRINTS.json'
VERIFY = DEST+'FINAL_VERIFICATION.json'
GENERATED = {CHANGED, FINAL, VERIFY}
SELF_EXCLUDED = {FINAL, VERIFY}
FROZEN = 'docs/evidence/m78/BASELINE.json'
ORIGINAL = 'docs/evidence/m78/FINAL_FINGERPRINTS.json'
PRIOR = 'docs/evidence/m78-followup/FINAL_FINGERPRINTS.json'
SOURCE_RECEIPT = 'docs/evidence/m78-continuation/PRESERVATION_SOURCE_FINAL.json'
EXPECTED_ORIGINAL_SHA = 'fe4d7221e165d26cf79836a62f4fe000eff8700661d06b1e21b0ffc938cb0e27'
EXPECTED_PRIOR_SHA = '38892f9979dc8eadf91c6a397c7f86ae60678bce83e48b5927aafac7f4c829e5'
CURRENT_TRACKED = {
    'CMakeLists.txt', 'docs/ARCHITECTURE.md', 'docs/M77.md',
    'src/CollisionGeometry.cpp', 'src/CollisionGeometry.h',
    'src/PhysicsWorld.cpp', 'src/PhysicsWorld.h', 'src/Prefab.cpp',
    'src/RigidBody.cpp', 'src/RigidBody.h', 'src/RigidMotion.h',
    'src/editor/TerrainTools.cpp', 'src/editor/TerrainTools.h',
    'tests/CollisionGeometryTests.cpp', 'tests/ContactLifecycleTests.cpp',
    'tests/PrefabTests.cpp', 'tests/RigidMotionTests.cpp',
    'tests/ScalabilityTests.cpp', 'tests/SpacecraftControlTests.cpp',
    'tests/SpacecraftFlightTests.cpp', 'tests/WorldStreamingTests.cpp',
    'tools/NavigationBake.cpp', 'src/WorldPersistence.cpp', 'src/WorldPersistence.h',
    'src/PhysicalBalance.cpp', 'docs/judasjs/saves.md',
}
PRIOR_EXCEPTIONS = {
    'docs/ARCHITECTURE.md', 'docs/M77.md', 'docs/M78_FOLLOWUP.md',
    'src/CollisionGeometry.cpp', 'src/PhysicsWorld.cpp',
    'src/RigidBody.cpp', 'src/RigidBody.h', 'src/RigidMotion.h',
    'tests/CollisionGeometryTests.cpp', 'tests/RigidMotionTests.cpp',
    'CMakeLists.txt', 'src/PhysicsWorld.h', 'src/PhysicalBalance.cpp',
    'src/WorldPersistence.cpp', 'src/WorldPersistence.h',
}
NEW_EXACT = {'docs/M78.md', 'docs/M78_FOLLOWUP.md', 'docs/M78_CONTINUATION.md',
             'src/authoring/NavigationDependencies.cpp',
             'src/authoring/NavigationDependencies.h', 'tests/NavigationDependencyTests.cpp', 'tests/PhysicsContinuationTests.cpp', 'docs/M78_COLD_CONTINUATION.md'}
NEW_PREFIXES = ('docs/evidence/m78/', 'docs/evidence/m78-followup/', 'docs/evidence/m78-continuation/', 'docs/evidence/m78-windows-tail/', DEST)
BINARY_SUFFIXES = {'.exe', '.dll', '.so', '.a', '.o', '.obj', '.lib', '.pdb',
                   '.ilk', '.pch', '.zip', '.7z', '.tar', '.gz', '.xz', '.bin'}
INPUT_GLOBS = [
    'src/*Rigid*', 'src/*Contact*', 'src/*Collision*', 'src/*Impact*', 'src/*Joint*',
    'src/*Gravity*', 'src/*Liquid*', 'src/*Fluid*', 'src/*PhysicalBalance*',
    'src/*PhysicalAnimation*', 'src/*CharacterMotor*', 'src/*Ragdoll*',
    'src/*RuntimeWorld*', 'src/*WorldPersistence*', 'src/*WorldStreaming*',
    'src/*GameSession*', 'src/*Simulation*', 'src/*SceneFingerprint*',
    'src/*WorldState*', 'src/*SceneSerialization*', 'src/*Prefab*',
    'src/authoring/NavigationDependencies.*', 'src/NavigationBake.*',
    'tests/*Contact*', 'tests/*Collision*', 'tests/*Impact*', 'tests/*Joint*',
    'tests/*Rigid*', 'tests/*Gravity*', 'tests/*PhysicalBalance*',
    'tests/*PhysicalAnimation*', 'tests/*CharacterMotor*', 'tests/*Ragdoll*',
    'tests/*WorldPersistence*', 'tests/*WorldStreaming*', 'tests/*Prefab*',
    'tests/*Spacecraft*', 'tests/*Scalability*', 'tests/*NavigationDependency*',
    'projects/balance_lab/**/*', 'projects/character_lab/**/*',
    'projects/ragdoll_demo/**/*', 'projects/animation_demo/Assets/models/bend_bar.glb*',
    'projects/post_m65_consumers/post_m65_consumers.judasproj',
    'projects/post_m65_consumers/Scenes/skate/park.judas',
    'projects/post_m65_consumers/Assets/skate/models/rider.glb*',
    'docs/evidence/ftft3/fixtures/**/*',
]
RAW_MANIFESTS = ['docs/evidence/m78-continuation/'+'performance-analysis/MEASUREMENT_FINGERPRINTS.json',
                 'docs/evidence/m78-continuation/'+'final-performance/RAW_FINGERPRINTS.json',
                 'docs/evidence/m78-continuation/'+'final-performance/REVIEW_HASHES.json']


class CheckFailure(Exception):
    pass


def git(*arguments):
    environment = dict(os.environ, GIT_OPTIONAL_LOCKS='0')
    return subprocess.check_output(['git', '--no-optional-locks', *arguments],
                                   cwd=ROOT, env=environment)


def document(path):
    return json.loads((ROOT/path).read_text())


class Hasher:
    def __init__(self):
        self.cached = {}

    def digest(self, path):
        path = Path(path)
        if not path.is_absolute():
            path = ROOT/path
        if not path.is_file() or path.is_symlink():
            raise CheckFailure(f'Missing/not ordinary file: {path}')
        before = path.stat()
        signature = (before.st_dev, before.st_ino, before.st_size, before.st_mtime_ns, before.st_ctime_ns)
        old = self.cached.get(path)
        if old and old[0] == signature:
            return old[1]
        digest = hashlib.sha256()
        with path.open('rb') as stream:
            for block in iter(lambda: stream.read(1024*1024), b''):
                digest.update(block)
        after = path.stat()
        if signature != (after.st_dev, after.st_ino, after.st_size, after.st_mtime_ns, after.st_ctime_ns):
            raise CheckFailure(f'File changed during capture: {path}')
        result = {'bytes': before.st_size, 'sha256': digest.hexdigest()}
        self.cached[path] = (signature, result)
        return result


def binary(path):
    if Path(path).suffix.lower() in BINARY_SUFFIXES:
        return True
    with (ROOT/path).open('rb') as stream:
        return stream.read(4).startswith((b'\x7fELF', b'MZ'))


def inventory(allowed_tracked, allowed_new):
    frozen = document(FROZEN)
    head = git('rev-parse', 'HEAD').decode().strip()
    if head != frozen['checkpoint']:
        raise CheckFailure(f'HEAD differs from starting checkpoint: {head}')
    if git('diff', '--cached', '--name-only', '-z'):
        raise CheckFailure('Index is not empty; this candidate must remain unstaged')
    tracked = set(filter(None, git('diff', '--no-ext-diff', '--name-only', 'HEAD', '-z').decode().split('\0')))
    unexpected = sorted(tracked-allowed_tracked)
    if unexpected:
        raise CheckFailure('Unexpected tracked changes: '+', '.join(unexpected))
    untracked = set(filter(None, git('ls-files', '--others', '--exclude-standard', '-z').decode().split('\0')))
    selected, excluded = set(tracked), []
    for path in sorted(untracked):
        if path in GENERATED:
            continue
        if path == 'POST_M73_REPORT.txt':
            excluded.append({'path': path, 'reason': 'Unrelated pre-existing report'})
        elif path in allowed_new or path.startswith(NEW_PREFIXES):
            if Path(path).suffix.lower() == '.lock':
                excluded.append({'path': path, 'reason': 'Extracted runtime save-slot lock; temporary output, preserved but never staged'})
            elif any(part in {'.cache', 'build', 'CMakeFiles', '__pycache__'} for part in Path(path).parts) or binary(path):
                excluded.append({'path': path, 'reason': 'Build/cache/executable/archive output; never staged'})
            else:
                selected.add(path)
        else:
            excluded.append({'path': path, 'reason': 'Unknown untracked path outside explicit task scope'})
    for path in selected:
        if not (ROOT/path).is_file() or binary(path):
            raise CheckFailure('Deleted or binary task source: '+path)
    return head, selected, excluded


def compare_manifest(path, hasher, exceptions=()):
    entries = document(path)['files']
    changed = []
    for name, expected in entries.items():
        expected = expected['sha256'] if isinstance(expected, dict) else expected
        actual = hasher.digest(name)['sha256']
        if actual != expected:
            changed.append({'path': name, 'recorded': expected, 'actual': actual})
    forbidden = [entry for entry in changed if entry['path'] not in exceptions]
    if forbidden:
        raise CheckFailure(f'Protected mismatch in {path}: '+json.dumps(forbidden))
    return {'manifest': path, 'manifestSHA256': hasher.digest(path)['sha256'],
            'checked': len(entries), 'authorizedDifferences': changed,
            'exceptions': sorted(exceptions), 'unexpectedDifferences': []}


def preservation(hasher, allowed_tracked, allowed_prior):
    if hasher.digest(ORIGINAL)['sha256'] != EXPECTED_ORIGINAL_SHA or hasher.digest(PRIOR)['sha256'] != EXPECTED_PRIOR_SHA:
        raise CheckFailure('An original historical manifest changed')
    source_receipt = document(SOURCE_RECEIPT)
    if not set(source_receipt['priorCandidate']['differences']) <= allowed_prior:
        raise CheckFailure('Prior-candidate exceptions do not cover the explicit prior source receipt')
    proofs = [compare_manifest(FROZEN, hasher, allowed_tracked),
              compare_manifest(ORIGINAL, hasher), compare_manifest(PRIOR, hasher, allowed_prior)]
    raw = []
    for path in RAW_MANIFESTS:
        entries = document(path)['files']
        # Old binaries can be archived/cleaned under their receipts. Raw CSVs,
        # profiles and repository evidence must remain immutable; no current
        # source or build-output hashes are silently substituted into history.
        selected = {name: value for name, value in entries.items()
                    if name.startswith('docs/evidence/') or name.endswith(('.csv', '-profile.json'))}
        for name, expected in selected.items():
            digest = expected['sha256'] if isinstance(expected, dict) else expected
            if hasher.digest(name)['sha256'] != digest:
                raise CheckFailure(f'Historical raw/analysis artifact changed: {name}')
        raw.append({'manifest': path, 'manifestSHA256': hasher.digest(path)['sha256'],
                    'rawEvidenceFilesChecked': len(selected),
                    'externalBuildOutputsNotRequiredPresent': sorted(set(entries)-set(selected))})
    old_final='docs/evidence/m78-continuation/FINAL_FINGERPRINTS.json'
    if hasher.digest(old_final)['sha256'] != '993ef6fe81b5ad4df3897020a6a620def4090a20ecd8b7a80f392923a4c33865':
        raise CheckFailure('Frozen previous combined manifest changed')
    proofs.append(compare_manifest(old_final,hasher,{'CMakeLists.txt','src/PhysicsWorld.cpp','src/PhysicsWorld.h','src/PhysicalBalance.cpp','src/WorldPersistence.cpp','src/WorldPersistence.h','docs/ARCHITECTURE.md','docs/M77.md','docs/judasjs/saves.md'}))
    local = document('docs/evidence/m78-followup/BINARY_INPUT_FINGERPRINTS.json')['originalLocalInputs']
    for entry in local:
        if hasher.digest(entry['path'])['sha256'] != entry['sha256']:
            raise CheckFailure('Frozen local input changed: '+entry['path'])
    post_expected = document('docs/evidence/m78-followup/PRESERVATION_FINAL.json')['unrelatedUntrackedPOSTCurrentSHA256']
    if hasher.digest('POST_M73_REPORT.txt')['sha256'] != post_expected:
        raise CheckFailure('Unrelated pre-existing POST_M73_REPORT.txt changed')
    return {'historicalAndTracked': proofs, 'rawEvidence': raw, 'frozenLocalInputsChecked': len(local),
            'unrelatedPOSTUnchanged': True}


def unchanged_inputs():
    paths = {FROZEN, ORIGINAL, PRIOR, SOURCE_RECEIPT,
             'docs/evidence/m78-followup/BINARY_INPUT_FINGERPRINTS.json'} | set(RAW_MANIFESTS)
    for pattern in INPUT_GLOBS:
        paths.update(str(path.relative_to(ROOT)) for path in ROOT.glob(pattern) if path.is_file())
    return paths


def write_new(path, value):
    with (ROOT/path).open('x') as stream:
        stream.write(value)


def named_paths(paths):
    result = set()
    for value in paths:
        path = Path(value)
        if path.is_absolute() or '..' in path.parts or '.git' in path.parts:
            raise CheckFailure('Allowlist additions must be named repository-relative paths: '+value)
        if value.startswith(('docs/evidence/', 'prototypes/')):
            raise CheckFailure('Historical evidence/research cannot be an exception: '+value)
        result.add(path.as_posix())
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument('--capture', action='store_true')
    mode.add_argument('--verify', action='store_true')
    parser.add_argument('--binary', action='append', default=[], type=Path,
                        help='Explicit current executable/archive hash-only; not task-file inclusion')
    parser.add_argument('--write-verification', action='store_true',
                        help='Write NEW optional FINAL_VERIFICATION.json; never overwrite it')
    parser.add_argument('--allow-tracked-change', action='append', default=[], metavar='PATH',
                        help='Explicit authorized source/current-doc addition beyond known 22 paths')
    parser.add_argument('--allow-prior-change', action='append', default=[], metavar='PATH',
                        help='Explicit current-source/doc exception beyond approved prior ten paths')
    parser.add_argument('--allow-new-path', action='append', default=[], metavar='PATH',
                        help='Explicit new task source/current-doc path outside known task prefixes')
    args = parser.parse_args()
    if args.verify and args.binary:
        parser.error('--binary is a capture option; --verify uses the recorded binary list')
    if args.capture and args.write_verification:
        parser.error('--write-verification belongs to --verify')
    configuration = dict(allowedTracked=sorted(CURRENT_TRACKED | named_paths(args.allow_tracked_change)),
                         allowedPrior=sorted(PRIOR_EXCEPTIONS | named_paths(args.allow_prior_change)),
                         allowedNew=sorted(NEW_EXACT | named_paths(args.allow_new_path)))
    if args.verify:
        final = document(FINAL)
        if any((args.allow_tracked_change, args.allow_prior_change, args.allow_new_path)) and configuration != final['allowlistConfiguration']:
            raise CheckFailure('Verification cannot silently replace the captured explicit allowlist')
        configuration = final['allowlistConfiguration']
    allowed_tracked = set(configuration['allowedTracked'])
    allowed_prior = set(configuration['allowedPrior'])
    allowed_new = set(configuration['allowedNew'])
    head, changed, excluded = inventory(allowed_tracked, allowed_new)
    hasher = Hasher()
    proof = preservation(hasher, allowed_tracked, allowed_prior)
    if args.capture:
        if any((ROOT/path).exists() for path in GENERATED):
            raise CheckFailure('Final metadata already exists; preserve it and use --verify')
        selected = changed | unchanged_inputs()
        task_list = sorted(changed | {CHANGED, FINAL})
        # Check ordinary inputs before creating any capture file.
        entries = {path: dict(hasher.digest(path), role='changedTask' if path in changed else 'unchangedInput')
                   for path in sorted(selected)}
        binaries = {}
        for path in args.binary:
            resolved = path.resolve()
            if str(resolved).startswith(str(ROOT/'docs/evidence')+'/'):
                raise CheckFailure('A binary output must not live inside evidence: '+str(resolved))
            binaries[str(resolved)] = hasher.digest(resolved)
        write_new(CHANGED, '\n'.join(task_list)+'\n')
        entries[CHANGED] = dict(hasher.digest(CHANGED), role='generatedInventory')
        # Refuse a late task addition/change rather than publishing a mixed state.
        check_head, check_changed, check_excluded = inventory(allowed_tracked, allowed_new)
        if (check_head, check_changed, check_excluded) != (head, changed, excluded):
            raise CheckFailure('Git inventory changed during capture; CHANGED_FILES is retained for investigation')
        for path, entry in entries.items():
            if hasher.digest(path)['sha256'] != entry['sha256']:
                raise CheckFailure('Selected file changed during capture: '+path)
        result = {'utc': datetime.datetime.now(datetime.timezone.utc).isoformat(), 'checkpoint': head,
                  'scope': 'Exact uncommitted/unstaged M78 cold-boundary candidate plus named unchanged inputs; no semantic certification',
                  'changedTaskFiles': task_list, 'excludedUntracked': excluded,
                  'allowlistConfiguration': configuration,
                  'files': entries, 'binaryOutputsHashOnly': binaries, 'preservation': proof,
                  'metadataExclusions': sorted(SELF_EXCLUDED),
                  'selfReferencePolicy': 'CHANGED_FILES lists itself and the manifest; CHANGED_FILES is hashed. Manifest excludes itself and later verification receipt.',
                  'ignoredFilePolicy': 'Ignored caches/builds/user assets are not inventoried; explicit binaries are hash-only and seven frozen local inputs are preservation-checked.'}
        write_new(FINAL, json.dumps(result, indent=2)+'\n')
        print(json.dumps({'capture': FINAL, 'files': len(entries), 'changedTaskFiles': len(task_list),
                          'excludedUntracked': excluded, 'manifestSHA256': hasher.digest(FINAL)['sha256']}))
    else:
        if final['checkpoint'] != head or sorted(changed | {CHANGED, FINAL}) != final['changedTaskFiles']:
            raise CheckFailure('Current changed/new task inventory differs from captured candidate')
        for path, expected in final['files'].items():
            if hasher.digest(path)['sha256'] != expected['sha256']:
                raise CheckFailure('Final captured file mismatch: '+path)
        for path, expected in final['binaryOutputsHashOnly'].items():
            if hasher.digest(path)['sha256'] != expected['sha256']:
                raise CheckFailure('Final explicitly captured binary mismatch: '+path)
        result = {'utc': datetime.datetime.now(datetime.timezone.utc).isoformat(), 'head': head,
                  'manifest': FINAL, 'manifestSHA256': hasher.digest(FINAL)['sha256'],
                  'filesVerified': len(final['files']), 'binaryOutputsVerified': len(final['binaryOutputsHashOnly']),
                  'excludedUntrackedNow': excluded, 'preservation': proof, 'indexDiff': '', 'passed': True}
        if args.write_verification:
            write_new(VERIFY, json.dumps(result, indent=2)+'\n')
        print(json.dumps(result, indent=2))


if __name__ == '__main__':
    try:
        main()
    except (CheckFailure, OSError, ValueError, subprocess.CalledProcessError) as error:
        raise SystemExit('FAIL: '+str(error))
