#!/usr/bin/env python3
"""Verify every archived source/log byte; never extract or run the archive."""
import hashlib,json,tarfile
from pathlib import Path
here=Path(__file__).resolve().parent
manifest=json.loads((here/'SOURCE_MANIFEST.json').read_text())
expected=manifest['files'];archive=here/'stage5-sources-and-evidence.tar.gz'
with tarfile.open(archive,'r:gz') as source:
    members=source.getmembers()
    assert len(members)==len(expected),'archive member count differs'
    seen=set()
    for member in members:
        assert member.isfile() and member.name.startswith('.cache/'),'unexpected archive member'
        assert '..' not in Path(member.name).parts and member.name not in seen,'unsafe/duplicate archive path'
        seen.add(member.name);record=expected[member.name];data=source.extractfile(member).read()
        assert len(data)==record['bytes'] and hashlib.sha256(data).hexdigest()==record['sha256'],member.name
assert seen==set(expected),'manifest/archive membership differs'
print(f'Verified {len(seen)} exact archived source/evidence files; no extraction or execution.')
