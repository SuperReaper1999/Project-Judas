#!/usr/bin/env python3
"""Read-only verification; no extraction, builds, simulations or network."""
from pathlib import Path
import hashlib,json,tarfile
here=Path(__file__).resolve().parent
manifest=json.loads((here/'SOURCE_MANIFEST.json').read_text())
archive=here/'filtered-arithmetic-reference.tar.gz'
assert hashlib.sha256(archive.read_bytes()).hexdigest()==manifest['archiveSha256']
seen=set()
with tarfile.open(archive,'r:gz') as tar:
 for item in tar.getmembers():
  name=item.name;parts=Path(name).parts
  assert item.isfile() and not name.startswith('/') and '..' not in parts
  assert name.startswith(('.cache/fluid-','src/')) and name not in seen
  seen.add(name);data=tar.extractfile(item).read();record=manifest['files'][name]
  assert len(data)==record['bytes'] and hashlib.sha256(data).hexdigest()==record['sha256'],name
assert seen==set(manifest['files'])
print(f'Verified {len(seen)} archived private source/evidence/dependency files; no extraction or execution.')
