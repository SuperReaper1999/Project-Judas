#!/usr/bin/env python3
"""File-only verification/preservation; never compiles or starts Judas."""
from pathlib import Path
import datetime
import hashlib
import json
import shutil
import subprocess

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]
PRIOR = ROOT / '.cache/physics-exhaustive-profile-20261021'
EXPECTED_HEAD = 'f5624f0bb4f92ece7211199ca8fe51abc08a2b19'
EXPECTED_CORE = '05f626d3fb5400bfd1db23ec37a3cd3e6490f8452145365662466eebccd705ec'


def sha(path):
    digest = hashlib.sha256()
    with path.open('rb') as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b''):
            digest.update(block)
    return digest.hexdigest()


def retain(source, target, records):
    expected = sha(source)
    if target.exists():
        if sha(target) != expected:
            raise RuntimeError(f'preserved destination disagrees; refusing overwrite: {target}')
    else:
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source, target)
    if sha(target) != expected:
        raise RuntimeError(f'copy verification failed: {target}')
    records[str(target.relative_to(HERE))] = {
        'source': str(source), 'sha256': expected, 'bytes': target.stat().st_size}


def main():
    frozen_path = PRIOR / 'baseline-snapshot/FROZEN.json'
    frozen = json.loads(frozen_path.read_text())
    if frozen['git_head'] != EXPECTED_HEAD or frozen['artifacts']['libjudas_engine.a']['sha256'] != EXPECTED_CORE:
        raise RuntimeError('frozen provenance differs from the designated baseline')
    frozen_core = PRIOR / 'baseline-snapshot/libjudas_engine.a'
    if sha(frozen_core) != EXPECTED_CORE:
        raise RuntimeError('frozen core bytes disagree')
    if sha(PRIOR / 'baseline-snapshot/source-inputs.tar.gz') != frozen['source_archive']['sha256']:
        raise RuntimeError('frozen source archive bytes disagree')
    fixture = ROOT / 'projects/physics_stress_lab/Scenes/room1024.judas'
    if sha(fixture) != frozen['fixture']['sha256']:
        raise RuntimeError('original fixture differs; do not substitute a new fixture')
    mismatches = []
    for relative, record in frozen['source_inputs'].items():
        current = ROOT / relative
        actual = sha(current) if current.is_file() else None
        if actual != record['sha256']:
            mismatches.append({'path': relative, 'expected': record['sha256'], 'current': actual})
    records = {}
    for name in ['FROZEN.json', 'source-inputs.tar.gz', 'libjudas_engine.a']:
        retain(PRIOR / 'baseline-snapshot' / name, HERE / 'baseline/frozen' / name, records)
    retain(fixture, HERE / 'baseline/room1024.judas', records)
    for name in ['RUNS.json', 'MATCHED_CACHE_REPEATS.json']:
        source = PRIOR / name
        if source.is_file():
            retain(source, HERE / 'baseline/provenance' / name, records)
    for name in ['unobserved900', 'unobserved900-B', 'long1800', 'live35-sampled']:
        for filename in ['steps.csv', 'frames.csv', 'begins.csv', 'quality.json']:
            retain(PRIOR / name / filename, HERE / 'baseline' / name / filename, records)
        retain(PRIOR / (name + '.log'), HERE / 'baseline' / (name + '.log'), records)
    # Retain independent low-overhead occupancy evidence, never stage timings as
    # normal-core percentages. All copied files retain their original bytes.
    for name in ['sampled900', 'live35-sampled']:
        for relative in ['sampling/metadata.json', 'sampling/cpu-pc.csv', 'sampling/modules.csv',
                         'sampling/maps.txt', 'sampling/symbolized/source-buckets/buckets.json']:
            retain(PRIOR / name / relative, HERE / 'baseline' / name / relative, records)
    helper = PRIOR / 'sampler/compare_steps.py'
    retain(helper, HERE / 'compare_steps_frozen.py', records)
    manifest = {'schema': 'judas.persistent-contact.baselines.v1', 'head': EXPECTED_HEAD,
                'core_sha256': EXPECTED_CORE, 'records': records,
                'notes': ['Live35 baseline includes the explicitly recorded 2 ms CPU sampler.',
                          'Normal900/1800 timing baselines are not the instrumented demand observer.',
                          'Current-source differences are candidate work, never rewritten baseline expectations.']}
    encoded = json.dumps(manifest, indent=2) + '\n'
    destination = HERE / 'BASELINES.json'
    if destination.exists() and destination.read_text() != encoded:
        raise RuntimeError('baseline inventory changed; inspect rather than overwrite')
    destination.write_text(encoded)
    build_core = ROOT / 'build/libjudas_engine.a'
    verification = {'utc': datetime.datetime.now(datetime.timezone.utc).isoformat(),
                    'head': subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip(),
                    'branch': subprocess.check_output(['git', 'branch', '--show-current'], cwd=ROOT, text=True).strip(),
                    'frozen_core_verified': True, 'fixture_verified': True,
                    'source_count': len(frozen['source_inputs']), 'current_source_mismatches': mismatches,
                    'current_build_core_sha256': sha(build_core) if build_core.is_file() else None,
                    'current_build_matches_frozen': build_core.is_file() and sha(build_core) == EXPECTED_CORE,
                    'copied_files': len(records), 'copied_bytes': sum(row['bytes'] for row in records.values()),
                    'no_engine_executed': True}
    stamp = datetime.datetime.now(datetime.timezone.utc).strftime('%Y%m%dT%H%M%S%fZ')
    (HERE / ('VERIFIED-' + stamp + '.json')).write_text(json.dumps(verification, indent=2) + '\n')
    print(json.dumps(verification, indent=2))


if __name__ == '__main__':
    main()
