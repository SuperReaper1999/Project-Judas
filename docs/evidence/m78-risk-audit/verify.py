#!/usr/bin/env python3
"""Read-only audit identity check. Never regenerate expected fingerprints."""
from pathlib import Path
import hashlib,json,subprocess
ROOT=Path(__file__).resolve().parents[3];E=ROOT/'docs/evidence/m78-risk-audit'
def sha(p):
 h=hashlib.sha256()
 with p.open('rb')as f:
  for chunk in iter(lambda:f.read(1024*1024),b''):h.update(chunk)
 return {'bytes':p.stat().st_size,'sha256':h.hexdigest()}
m=json.loads((E/'FINAL_FINGERPRINTS.json').read_text());failures=[];counts={}
for label in ['evidenceFiles','sourceInputs','localBinaryOutputs']:
 counts[label]=0
 for name,expected in m[label].items():
  p=ROOT/name
  if not p.is_file() or sha(p)!=expected:failures.append(label+': '+name)
  counts[label]+=1
head=subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT,text=True).strip()
if subprocess.run(['git','merge-base','--is-ancestor',m['startingCheckpoint'],head],cwd=ROOT).returncode:failures.append('starting checkpoint is not an ancestor')
if subprocess.check_output(['git','diff','--name-only',m['startingCheckpoint'],'--','src','tests','projects','third_party','assets','tools','CMakeLists.txt'],cwd=ROOT,text=True).strip():failures.append('production paths differ from audited engine checkpoint')
if subprocess.check_output(['git','diff','--name-only','HEAD'],cwd=ROOT,text=True).strip():failures.append('tracked source/index changed')
actual=sorted(str(p.relative_to(ROOT))for p in E.rglob('*')if p.is_file()and '__pycache__'not in p.parts)
expected=(E/'CHANGED_FILES.txt').read_text().splitlines()
if actual!=expected:failures.append('new evidence inventory differs')
print(json.dumps({'passed':not failures,'counts':counts,'failures':failures,'manifestIdentity':sha(E/'FINAL_FINGERPRINTS.json'),'scope':'Exact identities only; no human or comprehensive certification.'},indent=2))
raise SystemExit(bool(failures))
