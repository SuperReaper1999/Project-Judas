#!/usr/bin/env python3
"""Verify checkpoint source/evidence bytes; never rebuild, simulate or change Git."""
import argparse
import gzip
import hashlib
import json
from pathlib import Path


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--binaries', action='store_true', help='also require the locally retained measured binaries')
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
    for name in failures:
        print('MISMATCH', name)
    print(f'{len(manifest["files"])} source/metadata files, {len(artifacts)} lossless artifacts, '
          f'{len(binaries)} local binaries; {len(failures)} mismatches. Human acceptance pending.')
    return bool(failures)


if __name__ == '__main__':
    raise SystemExit(main())
