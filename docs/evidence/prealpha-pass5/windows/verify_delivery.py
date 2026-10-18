#!/usr/bin/env python3
"""Verify the delivered native Windows evidence without invoking the VM/tests."""
from pathlib import Path
import hashlib,json
root=Path(__file__).resolve().parent
manifest=json.loads((root/'MANIFEST.json').read_text())
for row in manifest['files']:
 path=root/row['path']
 assert path.is_file(),row['path']
 data=path.read_bytes()
 assert len(data)==row['bytes'],row['path']
 assert hashlib.sha256(data).hexdigest()==row['sha256'],row['path']
expected={row['path'] for row in manifest['files']}|{'MANIFEST.json'}
assert {p.relative_to(root).as_posix() for p in root.rglob('*') if p.is_file()}==expected
integrity=json.loads((root/'raw/FINAL_INTEGRITY.json').read_text())
assert integrity['passed'] and not integrity['sourceErrors'] and not integrity['unexpectedFunctionalResults']
assert integrity['sourceFilesVerified']==1438
follow=json.loads((root/'raw/CONTACT_FOLLOWUP_RESULTS.json').read_text(encoding='utf-8-sig'))
assert follow['passed'] and follow['fixtureSHA256']=='dcbcb9bf038f11662a2e6c388cc21cc4bdcbfe6807c54a282bb6dea87d7b57ce'
initial=json.loads((root/'raw/FUNCTIONAL_RESULTS.json').read_text(encoding='utf-8-sig'))
rows={row['label']:row for row in initial['rows']}
assert rows['judas_contact_cache_tests']['exit']==1
assert rows['curved-original-continuation-known-failure']['exit']==1
assert 'checks 372 failures 4' in rows['curved-original-continuation-known-failure']['stdout']
for name in ('m77-step-read','m77-stand-read','m77-recovery-read'):
 assert rows[name]['exit']==0
 assert 'restored native task reaches measured physical standing' in rows[name]['stdout']
print('Windows evidence verified:',len(manifest['files']),'files; original failures retained; focused follow-up passed')
