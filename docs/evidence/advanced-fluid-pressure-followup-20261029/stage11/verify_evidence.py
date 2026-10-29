#!/usr/bin/env python3
"""Verify archived bytes without extraction or executing numerical experiments."""
from pathlib import Path
import hashlib,json,tarfile
here=Path(__file__).resolve().parent
manifest=json.loads((here/'SOURCE_MANIFEST.json').read_text())
archive=here/'stage11-thin-cut-oracle-reference.tar.gz'
assert hashlib.sha256(archive.read_bytes()).hexdigest()==manifest['archiveSha256']
seen=set()
with tarfile.open(archive,'r:gz') as source:
 for member in source.getmembers():
  assert member.isfile() and member.name.startswith('.cache/')
  assert '..' not in Path(member.name).parts and member.name not in seen
  seen.add(member.name);data=source.extractfile(member).read();expected=manifest['files'][member.name]
  assert len(data)==expected['bytes'] and hashlib.sha256(data).hexdigest()==expected['sha256'],member.name
assert seen==set(manifest['files'])
print(f'Verified {len(seen)} archived source/evidence files; no extraction or execution.')
