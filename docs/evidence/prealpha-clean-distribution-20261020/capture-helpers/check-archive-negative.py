from pathlib import Path
import subprocess,json,hashlib
r=Path(__file__).resolve().parent;s=r/'source-no-git';p=s/'JUDAS_SOURCE_FILES.json';original=p.read_bytes();before=hashlib.sha256(original).hexdigest()
try:
 p.write_bytes(original+b'\n')
 with (r/'receipts/archive-corrupt-manifest.log').open('w') as log:
  run=subprocess.run(['cmake','-S',str(s),'-B',str(r/'archive-negative'),'-DJUDAS_BUILD_IDENTITY_ONLY=ON','-Djudas_git=/not-installed/git'],stdout=log,stderr=subprocess.STDOUT)
 assert run.returncode!=0,'Corrupt archive provenance was accepted'
finally:p.write_bytes(original)
assert hashlib.sha256(p.read_bytes()).hexdigest()==before
(r/'receipts/ARCHIVE_NEGATIVE.json').write_text(json.dumps({'corruptManifestRejected':True,'sourceRestoredByteExactly':True,'exit':run.returncode,'originalManifestSha256':before},indent=2)+'\n')
