#!/usr/bin/env python3
"""Read-only final receipt/fingerprint checks; no expected-hash regeneration."""
from pathlib import Path
import hashlib,json,subprocess,gzip
R=Path(__file__).resolve().parents[3];E=Path(__file__).resolve().parent
sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
f=json.loads((E/'FINAL_FINGERPRINTS.json').read_text())
for group in ['source','evidence','binaries']:
 for path,want in f[group].items():
  p=R/path
  if not p.is_file():raise SystemExit('missing recorded '+group+' input: '+path)
  if sha(p)!=want:raise SystemExit('fingerprint mismatch: '+path)
frozen=json.loads((R/'docs/evidence/m78/CAPABILITIES.json').read_text())
final=json.loads((E/'CAPABILITIES_FINAL.json').read_text())
assert [x['id']for x in frozen['features']]==[x['id']for x in final['features']]
assert len(final['features'])==485
previous=json.loads((R/'docs/evidence/m78-next-risk/CAPABILITIES_OVERLAY.json').read_text())
for row,original in zip(final['features'],previous['features']):
 assert row['id']==original['id'] and row['baseline']==original['baseline'],'original Linux/Windows state changed'
for name in ['mod-RESULT.json','skeletal-RESULT.json','artifacts/composed/RESULT-write.json','artifacts/composed/RESULT-read.json']:
 assert json.loads((E/name).read_text())['failures']==0,name
assert json.loads((E/'PERFORMANCE.json').read_text())['fourRigP95Pass']
for p in E.glob('*.json.gz'):json.loads(gzip.decompress(p.read_bytes()))
protected=['src','tests','projects','third_party','CMakeLists.txt']
assert subprocess.run(['git','diff','--quiet',f['subject'],'--',*protected],cwd=R).returncode==0,'production inputs changed during audit'
print('M78 final verification PASS: exact hashes, all 485 IDs, final observed cases, preserved production inputs. No human/Windows certification.')
