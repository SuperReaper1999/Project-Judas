#!/usr/bin/env python3
"""Read-only archive verification; does not extract or run numerical code."""
from pathlib import Path
import hashlib,json,tarfile
here=Path(__file__).resolve().parent
m=json.loads((here/'SOURCE_MANIFEST.json').read_text())
p=here/'stage16-bounded-operator-reference.tar.gz'
assert hashlib.sha256(p.read_bytes()).hexdigest()==m['archiveSha256']
seen=set()
with tarfile.open(p,'r:gz') as tar:
 for member in tar.getmembers():
  assert member.isfile() and member.name.startswith('.cache/fluid-runtime-candidates-stage16-20261029/')
  assert '..' not in Path(member.name).parts and member.name not in seen
  seen.add(member.name);raw=tar.extractfile(member).read();expected=m['files'][member.name]
  assert len(raw)==expected['bytes'] and hashlib.sha256(raw).hexdigest()==expected['sha256'],member.name
assert seen==set(m['files'])
print(f'Verified {len(seen)} archived source/evidence files; no extraction or execution.')
