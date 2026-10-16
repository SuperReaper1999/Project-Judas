#!/usr/bin/env python3
"""Verify checkpoint source/evidence bytes; never rebuild, simulate or change Git."""
import argparse
import gzip
import hashlib
import json
import subprocess
from pathlib import Path


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--binaries', action='store_true', help='also require the locally retained measured binaries')
    parser.add_argument('--git', action='store_true', help='require exact Git index blobs, without text-filter normalization')
    args = parser.parse_args()
    here = Path(__file__).resolve().parent
    root = here.parents[2]
    sha = lambda data: hashlib.sha256(data).hexdigest()
    manifest = json.loads((here/'FINAL_FINGERPRINTS.json').read_text())
    failures = []
    for name, expected in manifest['files'].items():
        path = root/name
        if not path.is_file() or sha(path.read_bytes()) != expected:
            failures.append(name)
    artifacts = json.loads((here/'ARTIFACTS.json').read_text())['files']
    for record in artifacts:
        path = root/record['stored']
        if not path.is_file():
            failures.append(record['stored'])
            continue
        data = path.read_bytes()
        if sha(data) != record['stored_sha256']:
            failures.append(record['stored'] + ' stored bytes')
        try:
            original = gzip.decompress(data) if record['gzip'] else data
            if sha(original) != record['uncompressed_sha256'] or len(original) != record['uncompressed_bytes']:
                failures.append(record['stored'] + ' original bytes')
        except (OSError, EOFError):
            failures.append(record['stored'] + ' gzip')
    binaries = json.loads((here/'FINAL_BINARIES.json').read_text())['files'] if args.binaries else {}
    for name, expected in binaries.items():
        path = root/name
        if not path.is_file() or sha(path.read_bytes()) != expected:
            failures.append(name)
    if args.git:
        algorithm = subprocess.check_output(['git', 'rev-parse', '--show-object-format'], cwd=root, text=True).strip()
        indexed = {}
        for item in subprocess.check_output(['git', 'ls-files', '--stage', '-z'], cwd=root).split(b'\0'):
            if item:
                metadata, name = item.split(b'\t', 1)
                indexed[name.decode()] = metadata.split()[1].decode()
        names = set(manifest['files']) | {a['stored'] for a in artifacts}
        names.add(str((here/'FINAL_FINGERPRINTS.json').relative_to(root)))
        for name in names:
            path = root/name
            if not path.is_file():
                continue
            data = path.read_bytes()
            blob = hashlib.new(algorithm, b'blob ' + str(len(data)).encode() + b'\0' + data).hexdigest()
            if indexed.get(name) != blob:
                failures.append(name + ' Git index bytes')
    for name in failures:
        print('MISMATCH', name)
    print(f'{len(manifest["files"])} source/metadata files, {len(artifacts)} lossless artifacts, '
          f'{len(binaries)} local binaries; {len(failures)} mismatches. Human acceptance pending.')
    return bool(failures)


if __name__ == '__main__':
    raise SystemExit(main())
