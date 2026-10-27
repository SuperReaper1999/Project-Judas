#!/usr/bin/env python3
"""Judas release metadata, notification and local-download integrity verification.

No installation, Git mutation or project writes. Standard-library only.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import sys
import urllib.error
import urllib.request

sys.dont_write_bytecode = True

REPOSITORY = 'SuperReaper1999/Project-Judas'
BASE = f'https://github.com/{REPOSITORY}/releases/'
CHANNELS = ('Alpha', 'Beta', 'Experimental', 'Stable')
LIMIT = 262144
VERSION = re.compile(r'(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)(?:-([0-9A-Za-z-]+(?:\.[0-9A-Za-z-]+)*))?(?:\+([0-9A-Za-z-]+(?:\.[0-9A-Za-z-]+)*))?\Z')
HASH = re.compile(r'[0-9a-f]{64}\Z')
REVISION = re.compile(r'[0-9a-f]{40}\Z')


def semver(value):
    if not isinstance(value, str) or len(value) > 128:
        raise ValueError('Version must be a bounded SemVer string')
    match = VERSION.fullmatch(value)
    if not match:
        raise ValueError('Invalid semantic version')
    pre = match[4].split('.') if match[4] else []
    if any(p.isdigit() and len(p) > 1 and p[0] == '0' for p in pre):
        raise ValueError('Numeric prerelease identifiers cannot have leading zeros')
    # An empty prerelease sorts after any prerelease at the same core version.
    return tuple(int(match[i]) for i in (1, 2, 3)), pre


def compare(left, right):
    a, ap = semver(left)
    b, bp = semver(right)
    if a != b:
        return (a > b) - (a < b)
    if not ap or not bp:
        return (not ap) - (not bp)
    for x, y in zip(ap, bp):
        if x == y:
            continue
        if x.isdigit() and y.isdigit():
            return (int(x) > int(y)) - (int(x) < int(y))
        if x.isdigit() != y.isdigit():
            return -1 if x.isdigit() else 1
        return (x > y) - (x < y)
    return (len(ap) > len(bp)) - (len(ap) < len(bp))


def endpoint(channel):
    if channel not in CHANNELS:
        raise ValueError('Select Alpha, Beta, Experimental or Stable explicitly')
    return BASE + 'download/update-manifests/' + channel + '.json'


def validate(document, channel=None):
    if not isinstance(document, dict) or type(document.get('schema')) is not int or document.get('schema') != 1:
        raise ValueError('Unsupported release manifest schema')
    ch = document.get('channel')
    if ch not in CHANNELS or (channel is not None and ch != channel):
        raise ValueError('Release channel mismatch')
    version = document.get('version')
    core, pre = semver(version)
    stage = pre[0] if pre else 'stable'
    if ch == 'Stable' and pre or ch == 'Alpha' and stage != 'alpha' or ch == 'Beta' and stage != 'beta':
        raise ValueError('Release maturity does not match its channel')
    if not isinstance(document.get('source_revision'), str) or not REVISION.fullmatch(document['source_revision']):
        raise ValueError('Release requires exact source commit')
    protocol = document.get('compatibility')
    if not isinstance(protocol, dict) or type(protocol.get('update_protocol')) is not int or protocol['update_protocol'] != 1 or type(protocol.get('engine_major')) is not int or protocol['engine_major'] != core[0]:
        raise ValueError('Unsupported update compatibility contract')
    tag = document.get('tag')
    if tag != 'v' + version:
        raise ValueError('Tag must match the release version')
    if document.get('notes_url') != BASE + 'tag/' + tag:
        raise ValueError('Untrusted release notes URL')
    notes = document.get('notes')
    if not isinstance(notes, str) or not 1 <= len(notes.encode('utf-8')) <= 8192 or '\x00' in notes:
        raise ValueError('Missing/bounded release notes required')
    artifacts = document.get('artifacts')
    if not isinstance(artifacts, list) or not 1 <= len(artifacts) <= 32:
        raise ValueError('Release requires a bounded artifact inventory')
    seen = set()
    targets = set()
    for a in artifacts:
        if not isinstance(a, dict):
            raise ValueError('Invalid artifact record')
        name = a.get('name')
        if not isinstance(name, str) or not re.fullmatch(r'[A-Za-z0-9][A-Za-z0-9._+\-]{0,180}', name) or name in seen or '..' in name:
            raise ValueError('Invalid/duplicate artifact name')
        seen.add(name)
        if not isinstance(a.get('sha256'), str) or not HASH.fullmatch(a['sha256']) or type(a.get('bytes')) is not int or not 0 < a['bytes'] <= 100 * 1024**3:
            raise ValueError('Invalid artifact integrity metadata')
        if a.get('url') != BASE + 'download/' + tag + '/' + name:
            raise ValueError('Artifact URL must name this repository and version')
        if a.get('platform') not in ('Linux', 'Windows', 'Source') or a.get('architecture') not in ('x86_64', 'any'):
            raise ValueError('Unsupported release artifact platform')
        target = (a['platform'], a['architecture'])
        if target in targets:
            raise ValueError('Ambiguous release platform target')
        targets.add(target)
    return document


def read_manifest(path, channel=None):
    raw = Path(path).read_bytes()
    if len(raw) > LIMIT:
        raise ValueError('Release manifest exceeds size limit')
    return validate(json.loads(raw), channel)


def fetch(channel):
    request = urllib.request.Request(endpoint(channel), headers={'User-Agent': 'Judas-Release-Check/1', 'Accept': 'application/json'})
    with urllib.request.urlopen(request, timeout=7) as response:
        # GitHub redirects release assets to its own CDN. Never accept HTTP.
        if not response.url.startswith('https://'):
            raise ValueError('Update service redirected outside HTTPS')
        raw = response.read(LIMIT + 1)
    if len(raw) > LIMIT:
        raise ValueError('Release response exceeds size limit')
    return validate(json.loads(raw), channel)


def result(document, installed, platform, architecture):
    compatible = semver(installed)[0][0] == document['compatibility']['engine_major']
    artifacts = [a for a in document['artifacts'] if a['platform'] == platform and a['architecture'] == architecture]
    newer = compare(document['version'], installed) > 0
    return {'installed': installed, 'available': document['version'], 'channel': document['channel'],
            'newer': newer, 'compatible': compatible, 'offer': newer and compatible and bool(artifacts),
            'notes': document['notes'], 'notes_url': document['notes_url'], 'artifacts': artifacts,
            'notice': 'Notification only. Nothing has been installed or changed.'}


def verify_download(document, path):
    path = Path(path)
    artifact = next((a for a in document['artifacts'] if a['name'] == path.name), None)
    if not artifact or not path.is_file() or path.stat().st_size != artifact['bytes']:
        raise ValueError('Download is absent or size/name differs from release manifest')
    with path.open('rb') as f:
        actual = hashlib.file_digest(f, 'sha256').hexdigest()
    if actual != artifact['sha256']:
        raise ValueError('Download SHA-256 mismatch; do not install/extract it')
    return actual


def main():
    p = argparse.ArgumentParser(description=__doc__)
    sub = p.add_subparsers(dest='command', required=True)
    check = sub.add_parser('check')
    check.add_argument('--channel', choices=CHANNELS, default='Alpha')
    check.add_argument('--installed', required=True)
    check.add_argument('--platform', choices=['Linux', 'Windows'], required=True)
    check.add_argument('--architecture', default='x86_64')
    check.add_argument('--manifest', type=Path, help='Offline, explicitly supplied local fixture; no network')
    verify = sub.add_parser('verify')
    verify.add_argument('manifest', type=Path)
    verify.add_argument('download', type=Path)
    args = p.parse_args()
    try:
        if args.command == 'verify':
            print('Verified SHA-256:', verify_download(read_manifest(args.manifest), args.download))
        else:
            document = read_manifest(args.manifest, args.channel) if args.manifest else fetch(args.channel)
            print(json.dumps(result(document, args.installed, args.platform, args.architecture), indent=2))
    except (OSError, ValueError, urllib.error.URLError) as error:
        print('Update check/verification unavailable: ' + str(error), file=sys.stderr)
        return 1
    return 0

if __name__ == '__main__':
    sys.exit(main())
