#!/usr/bin/env python3
"""Check exact M92 source, evidence and optional local binary bytes. No builds or writes."""
import argparse
import hashlib
import json
import subprocess
import tarfile
from pathlib import Path


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--git', action='store_true', help='also verify exact staged/index blobs')
    parser.add_argument('--binaries', action='store_true', help='require the retained local binaries')
    args = parser.parse_args()
    here = Path(__file__).resolve().parent
    root = here.parents[2]
    sha = lambda data: hashlib.sha256(data).hexdigest()
    manifest = json.loads((here / 'FINAL_FINGERPRINTS.json').read_text())
    failures = []
    for name, expected in manifest['files'].items():
        path = root / name
        if not path.is_file() or sha(path.read_bytes()) != expected:
            failures.append(name)
    artifact = json.loads((here / 'ARTIFACTS.json').read_text())
    archive = root / artifact['archive']
    if not archive.is_file() or sha(archive.read_bytes()) != artifact['sha256']:
        failures.append(artifact['archive'])
    else:
        with tarfile.open(archive, 'r:gz') as tar:
            expected = {x['member']: x for x in artifact['members']}
            members = tar.getmembers()
            if len(members) != len(expected) or {x.name for x in members} != set(expected):
                failures.append('archive member inventory')
            for member in members:
                if not member.isfile() or member.name not in expected:
                    failures.append(member.name + ' unexpected member')
                    continue
                data = tar.extractfile(member).read()  # inspect bytes; never extract paths
                record = expected[member.name]
                if len(data) != record['bytes'] or sha(data) != record['sha256']:
                    failures.append(member.name + ' original bytes')
    binaries = json.loads((here / 'FINAL_BINARIES.json').read_text())['files'] if args.binaries else {}
    for name, expected in binaries.items():
        path = root / name
        if not path.is_file() or sha(path.read_bytes()) != expected:
            failures.append(name + ' binary')
    if args.git:
        algorithm = subprocess.check_output(['git', 'rev-parse', '--show-object-format'], cwd=root, text=True).strip()
        indexed = {}
        for item in subprocess.check_output(['git', 'ls-files', '--stage', '-z'], cwd=root).split(b'\0'):
            if item:
                meta, name = item.split(b'\t', 1)
                indexed[name.decode()] = meta.split()[1].decode()
        names = set(manifest['files']) | {str((here / 'FINAL_FINGERPRINTS.json').relative_to(root))}
        for name in names:
            path = root / name
            if not path.is_file():
                continue
            data = path.read_bytes()
            blob = hashlib.new(algorithm, b'blob ' + str(len(data)).encode() + b'\0' + data).hexdigest()
            if indexed.get(name) != blob:
                failures.append(name + ' Git index bytes')
    for name in failures:
        print('MISMATCH', name)
    print(f'{len(manifest["files"])} source/evidence files; {len(artifact["members"])} lossless observations; '
          f'{len(binaries)} retained binaries; {len(failures)} mismatches. Human review pending.')
    return bool(failures)


if __name__ == '__main__':
    raise SystemExit(main())
