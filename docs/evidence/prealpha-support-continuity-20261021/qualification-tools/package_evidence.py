#!/usr/bin/env python3
"""Plan a bounded source/data-only evidence package; copy only with --execute.

No builds, processes under test, source modification, or Git operations occur.
The default is an inventory printed to stdout, not a packaging operation.
Use --map LABEL=PATH for final runs whose output names are chosen later.
"""
from __future__ import annotations

import argparse
import gzip
import hashlib
import io
import json
from pathlib import Path, PurePosixPath
import tarfile


HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]
PRIVATE = HERE.parent
DESTINATION = ROOT / 'docs/evidence/prealpha-support-continuity-20261021'
SCHEMA = 'judas.support-continuity-evidence.v1'

# These are explicit, bounded study roots, not a recursive scan of the repository.
# Their run status is deliberately not inferred from their presence or filename.
GROUPS = {
    'baseline': HERE / 'baseline',
    'support-control': PRIVATE / 'support-control',
    'temporal': PRIVATE / 'temporal',
    'geometry-prototype': PRIVATE / 'geometry-prototype',
    'row-prototype': PRIVATE / 'row-prototype',
    'focused-provisional': PRIVATE / 'focused-support-control',
    'focused-provisional-gravity': PRIVATE / 'focused-support-gravity',
    'focused-fresh': PRIVATE / 'focused-fresh',
    'warm-fixture-paired': PRIVATE / 'warm-fixture-paired',
    'warm-lifecycle-paired': PRIVATE / 'warm-lifecycle-paired',
    'cold-public-invalid-fixture': PRIVATE / 'cold-public-paired',
    'cold-public-paired-v2': PRIVATE / 'cold-public-paired-v2',
    'qualification-tools': HERE,
}
PENDING = ('final900', 'final1800', 'final-live35', 'final-cold', 'final-integration')
SOURCES = ('src/physics/WorldImpactEvents.cpp', 'tests/SupportGuessOwnershipTests.cpp',
           'CMakeLists.txt', 'tools/validation/suites.json',
           'projects/physics_stress_lab/Scenes/room1024.judas')
FROZEN_SELECTION = {'src/physics/WorldImpactEvents.cpp', 'src/PhysicsWorld.cpp',
                    'src/ContactSolver.cpp', 'src/physics/WorldSleep.cpp',
                    'src/physics/WorldInternal.h', 'src/physics/WorldBodies.cpp',
                    'tests/PhysicsStressLiveBenchmark.cpp'}
TEXT_SUFFIXES = {'.csv', '.json', '.jsonl', '.txt', '.md', '.log', '.py', '.h', '.hpp',
                 '.c', '.cc', '.cpp', '.in', '.cmake', '.inl', '.inc', '.js',
                 '.judas', '.judasproj', '.judasprefab', '.judasui', '.judasmeta', '.save', '.previous'}
FORBIDDEN_PARTS = {'__pycache__', 'objects', 'userdata', 'cache', '.cache', 'config', '.git'}
NOTES = """# Support-continuity evidence provenance

This package contains original measurement records, source-only experiment inputs,
qualification tools, and selected final source snapshots. Presence is not a pass.
Binary/archive hashes in original receipts describe private artifacts; executable,
object and static/shared library bytes are intentionally excluded from this package.
The frozen manifest records all 517 original inputs. Selected baseline source
snapshots are included; unchanged files can be recovered from the recorded Git
checkpoint and verified against that manifest, rather than duplicating the entire
engine and four authored scenes in this evidence directory. The complete original
source archive remains preserved privately with its recorded hash.
CSV data is losslessly gzip-compressed with zero mtime and no embedded filename;
the manifest records both original uncompressed and stored-byte SHA-256 hashes.

## Temporal baseline source-monitor warning

`temporal/baseline900/AS_RUN.json` is preserved verbatim. It records the simulation
child exit 0, 900 completed steps, zero nonfinite rows, and unchanged binary, frozen
core and fixture. Its worktree source monitor reports `source_inputs_unchanged:
false`: production source was edited while the immutable baseline binary ran. The
outer runner therefore exited 1. This is not a clean source-monitor qualification
pass, nor an observed simulation crash; the frozen-binary physical observation is
retained with that exact qualification. No receipt is rewritten to remove it.

## Preserved qualification failures

`focused-provisional` includes a stale BendingTests object crash. The same object
also failed against the frozen original core; fresh BendingTests driver objects
passed both cores. Initial reused-object results remain provisional. Final
qualification belongs to the independently recorded fresh-driver checks.

`cold-public-invalid-fixture` records the first private probe's invalid save
metadata, not an engine persistence defect: its content identity was a human label
instead of the required 64-character SHA-256 identity. Exact original probe source
and diagnostic explanation are under qualification-tools/failures/. A later valid
fixture has a canonical authored scene fingerprint and explicit v4 content identity.
No unsupported universal bitwise warm-cache save-continuation promise is inferred.

## Scope and limitations

The ordinary support warm guess and completed actual contact observations are
different state. Event impulses are not serialized or copied into the next ordinary
support warm guess by this correction. Physical trajectories can change under the
same finite-pass solver when valid warm-guess history is retained; exact-parity
claims require the corresponding explicit comparison, not this packaging script.

Normal fixed-step timings, synthetic 900/1800-step throughput, temporal observers,
and actual live35 frame cadence have different scopes. Instrumented observer times
are not uninstrumented CPU percentages. CPU-PC sampling is main-thread time, not
hardware cycles, a call graph, total worker cost, or GPU timing.

The earlier PREALPHA_PHYSICS_COST_INVESTIGATION.txt and its evidence directory are
separate immutable baseline evidence; this tool does not modify or recopy them.
Missing/pending final groups are reported explicitly in PACKAGE.json.
"""


def digest(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def inside(path: Path, root: Path) -> bool:
    return path.is_relative_to(root)


def label_ok(label: str) -> bool:
    return bool(label) and all(c.isalnum() or c in '-_' for c in label)


def permit(path: Path, relative: Path) -> bool:
    if any(part in FORBIDDEN_PARTS for part in relative.parts):
        return False
    if path.name.endswith('.original'):
        return path.name[:-9].endswith(('.h', '.cpp'))
    return path.suffix in TEXT_SUFFIXES or path.name in ('CMakeLists.txt', 'DESIGN', 'COMMANDS', '.clang-format')


def gzip_csv(data: bytes) -> bytes:
    buffer = io.BytesIO()
    with gzip.GzipFile(fileobj=buffer, mode='wb', filename='', mtime=0, compresslevel=9) as stream:
        stream.write(data)
    return buffer.getvalue()


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, default=DESTINATION)
    parser.add_argument('--map', action='append', default=[], metavar='LABEL=PATH',
                        help='Explicit supplementary/final evidence root; may replace a default label')
    parser.add_argument('--source', action='append', default=[], metavar='REPOSITORY_PATH',
                        help='Additional explicitly selected current-source snapshot')
    parser.add_argument('--require', action='append', default=[], metavar='LABEL',
                        help='Refuse packaging if this group is missing (repeat for final required groups)')
    parser.add_argument('--max-file-mib', type=int, default=32)
    parser.add_argument('--max-total-mib', type=int, default=256)
    parser.add_argument('--execute', action='store_true')
    args = parser.parse_args()
    output = args.output.resolve()
    if not inside(output, ROOT / 'docs/evidence'):
        raise SystemExit('Output must be under this repository docs/evidence')
    if output.exists():
        raise SystemExit('Fresh destination required: existing evidence is never overwritten')
    if args.max_file_mib <= 0 or args.max_total_mib <= 0:
        raise SystemExit('Positive bounded size limits required')
    groups = dict(GROUPS)
    for mapping in args.map:
        label, separator, value = mapping.partition('=')
        if not separator or not label_ok(label):
            raise SystemExit('Expected safe LABEL=PATH')
        path = Path(value).resolve()
        if not inside(path, ROOT / '.cache'):
            raise SystemExit('Supplementary roots must be inside this repository .cache')
        groups[label] = path
    for required in args.require:
        if required not in groups or not groups[required].is_dir():
            raise SystemExit('Required group missing: ' + required)
    max_file = args.max_file_mib * 1024**2
    entries, skipped, missing = [], [], []
    destinations = set()
    total_bytes = 0

    def enqueue(source: Path | None, target: Path, *, data: bytes | None = None,
                archive_member: str | None = None, source_archive: Path | None = None) -> None:
        nonlocal total_bytes
        if target.suffix == '.csv':
            stored_target = target.with_suffix('.csv.gz')
        else:
            stored_target = target
        name = stored_target.as_posix()
        if target.is_absolute() or '..' in target.parts:
            raise RuntimeError('Unsafe package target: ' + name)
        if name in destinations:
            raise RuntimeError('Duplicate package target: ' + name)
        destinations.add(name)
        if data is None:
            assert source is not None
            if source.is_symlink() or not source.is_file():
                raise RuntimeError('Non-regular input: ' + str(source))
            if source.stat().st_size > max_file:
                raise RuntimeError('Input exceeds file bound: ' + str(source))
            data = source.read_bytes()
        if len(data) > max_file:
            raise RuntimeError('Input exceeds file bound: ' + str(source or archive_member))
        total_bytes += len(data)
        if total_bytes > args.max_total_mib * 1024**2:
            raise RuntimeError('Package exceeds total uncompressed bound')
        stored = gzip_csv(data) if target.suffix == '.csv' else data
        target = stored_target
        row = {'path': target.as_posix(), 'source': str(source) if source else None,
               'source_bytes': len(data), 'source_sha256': digest(data),
               'stored_bytes': len(stored), 'stored_sha256': digest(stored),
               'encoding': 'gzip-mtime0' if target.name.endswith('.csv.gz') else 'exact-bytes'}
        if archive_member:
            row.update(source_archive=str(source_archive), archive_member=archive_member)
        entries.append((row, stored))

    for label, folder in groups.items():
        if not label_ok(label):
            raise RuntimeError('Unsafe group label: ' + label)
        if not folder.is_dir():
            missing.append({'label': label, 'source': str(folder)})
            continue
        candidates = sorted(folder.rglob('*'))
        for path in candidates:
            if path.is_dir():
                continue
            relative = path.relative_to(folder)
            if label == 'final-integration':
                first = relative.parts[0]
                if first == 'editor-owned' or first.endswith(('-userdata', '-cache', '-config')):
                    continue
                if first.startswith('exported') and not (
                        len(relative.parts) == 2 and relative.suffix in {'.json', '.txt', '.judasproj'}):
                    continue
            # Tools owns only its immediate files and preserved source failures;
            # baseline data already has its own nonduplicated group above.
            if label == 'qualification-tools' and len(relative.parts) > 1 and relative.parts[0] != 'failures':
                continue
            if path.is_symlink() or not permit(path, relative):
                skipped.append({'source': str(path), 'reason': 'not selected regular source/data format'})
                continue
            enqueue(path, Path(label) / relative)

    # Preserve original source bytes without committing a source archive or any
    # binary dependency. Never trust archive extraction paths or symbolic links.
    archive = HERE / 'baseline/frozen/source-inputs.tar.gz'
    archive_receipt = None
    if archive.is_file() and not archive.is_symlink():
        archive_receipt = {'source': str(archive), 'bytes': archive.stat().st_size,
                           'sha256': digest(archive.read_bytes()), 'stored_archive': False}
        with tarfile.open(archive, mode='r:gz') as reader:
            for member in sorted(reader.getmembers(), key=lambda row: row.name):
                relative = PurePosixPath(member.name)
                if not member.isfile() or relative.is_absolute() or '..' in relative.parts or member.size > max_file:
                    raise RuntimeError('Unsafe/non-source frozen input member: ' + member.name)
                if member.name not in FROZEN_SELECTION:
                    skipped.append({'source_archive': str(archive), 'member': member.name,
                                    'reason': 'unchanged input retained in full frozen manifest and original archive'})
                    continue
                if not permit(Path(member.name), Path(relative)):
                    skipped.append({'source_archive': str(archive), 'member': member.name,
                                    'reason': 'binary or non-source asset, retained only in original private archive'})
                    continue
                stream = reader.extractfile(member)
                assert stream is not None
                data = stream.read(max_file + 1)
                if len(data) != member.size:
                    raise RuntimeError('Frozen member size mismatch: ' + member.name)
                enqueue(None, Path('frozen-source') / Path(relative), data=data,
                        archive_member=member.name, source_archive=archive)
    else:
        missing.append({'label': 'frozen-source', 'source': str(archive)})

    for relative in dict.fromkeys((*SOURCES, *args.source)):
        path = (ROOT / relative).resolve()
        if not inside(path, ROOT) or path.is_symlink():
            raise SystemExit('Unsafe current source: ' + relative)
        if not path.is_file():
            missing.append({'label': 'current-source', 'source': relative})
            continue
        enqueue(path, Path('current-source') / path.relative_to(ROOT))

    # Preserve this exact package plan/tool as part of the reproducible input set.
    if not any(row['path'] == 'qualification-tools/package_evidence.py' for row, _ in entries):
        enqueue(Path(__file__), Path('qualification-tools/package_evidence.py'))
    report = {'schema': SCHEMA, 'destination': str(output), 'execute': args.execute,
              'groups': {label: str(folder) for label, folder in groups.items()},
              'pending_not_mapped': [label for label in PENDING if label not in groups],
              'missing': missing, 'skipped': skipped, 'frozen_source_archive': archive_receipt,
              'original_bytes': total_bytes, 'stored_bytes': sum(row['stored_bytes'] for row, _ in entries),
              'file_count': len(entries), 'files': [row for row, _ in entries],
              'classification': 'Inventory only; no inferred test or physical acceptance status.'}
    if not args.execute:
        print(json.dumps(report, indent=2, sort_keys=True))
        return
    # All files are read, bounded, hashed and collision-checked before any output.
    output.mkdir(parents=True, exist_ok=False)
    for row, stored in entries:
        target = output / row['path']
        target.parent.mkdir(parents=True, exist_ok=True)
        with target.open('xb') as stream:
            stream.write(stored)
    (output / 'PROVENANCE.md').write_text(NOTES, encoding='utf-8')
    (output / 'PACKAGE.json').write_text(json.dumps(report, indent=2, sort_keys=True) + '\n', encoding='utf-8')
    hashes = []
    for path in sorted(output.rglob('*')):
        if path.is_file():
            hashes.append(digest(path.read_bytes()) + '  ' + path.relative_to(output).as_posix())
    (output / 'SHA256SUMS').write_text('\n'.join(hashes) + '\n', encoding='utf-8')
    print(json.dumps({key: value for key, value in report.items() if key != 'files'}, indent=2, sort_keys=True))


if __name__ == '__main__':
    main()
