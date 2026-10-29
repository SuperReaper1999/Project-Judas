#!/usr/bin/env python3
"""Verify archived evidence bytes without extraction or execution."""
from pathlib import Path
import hashlib,json,tarfile
base=Path(__file__).resolve().parent
manifest=json.loads((base/'SOURCE_MANIFEST.json').read_text())
archive=base/'rain-contrast-evidence.tar.gz'
assert hashlib.sha256(archive.read_bytes()).hexdigest()==manifest['archiveSha256']
seen=set()
with tarfile.open(archive,'r:gz') as source:
 for member in source.getmembers():
  assert member.isfile() and not Path(member.name).is_absolute()
  assert '..' not in Path(member.name).parts and member.name not in seen
  data=source.extractfile(member).read();expected=manifest['files'][member.name]
  assert len(data)==expected['bytes'] and hashlib.sha256(data).hexdigest()==expected['sha256'],member.name
  seen.add(member.name)
assert seen==set(manifest['files'])
print(f'Verified {len(seen)} archived source/evidence files; no extraction or execution.')
