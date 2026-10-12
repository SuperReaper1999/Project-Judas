#!/usr/bin/env python3
"""Read-only final V4 identity check. Expected hashes are never regenerated."""
from pathlib import Path
import argparse,hashlib,json,subprocess
R=Path(__file__).resolve().parents[4];E=R/'docs/evidence/m78-lifecycle'
def digest(p):
 h=hashlib.sha256()
 with p.open('rb')as f:
  for b in iter(lambda:f.read(1048576),b''):h.update(b)
 return dict(bytes=p.stat().st_size,sha256=h.hexdigest())
def git(*a):return subprocess.check_output(['git',*a],cwd=R).decode().strip()
p=argparse.ArgumentParser();p.add_argument('--source-only',action='store_true');p.add_argument('--staged',action='store_true');a=p.parse_args()
m=json.loads((E/'FINAL_FINGERPRINTS_V4.json').read_text());errors=[];counts={}
def check(label,records):
 counts[label]=0
 for name,v in records.items():
  q=Path(name);q=q if q.is_absolute()else R/q
  if not q.is_file()or any(digest(q)[k]!=v[k]for k in ('bytes','sha256')):errors.append(label+': '+name)
  counts[label]+=1
check('current',m['files']);check('frozenManifests',m['frozenManifests'])
old=json.loads((R/m['preservation']['priorManifest']).read_text());check('protectedOriginalFiles',{k:v for k,v in old['files'].items()if k not in m['trackedChanges']})
v2=json.loads((E/'FINAL_FINGERPRINTS_V2.json').read_text());check('preservedV2Files',{k:v for k,v in v2['files'].items()if k not in m['intentionalV2Changes']})
if not a.source_only:
 check('localOutputs',m['binaryOutputsHashOnly']);check('priorLocalOutputs',old['binaryOutputsHashOnly']);check('priorExcluded',old['excludedUntrackedHashOnly']);check('laterExcluded',v2['excludedUntrackedHashOnly'])
if (E/'CHANGED_FILES_V4.txt').read_text().splitlines()!=m['changedFiles']:errors.append('inventory differs')
expected=set(m['changedFiles']);actual={str(q.relative_to(R))for q in E.rglob('*')if q.is_file()and '__pycache__'not in q.parts}
if actual!={s for s in expected if s.startswith('docs/evidence/m78-lifecycle/')}:errors.append('evidence inventory differs')
if a.staged:
 staged=set(git('diff','--cached','--name-only').splitlines())
 if staged!=expected:errors.append('staged inventory differs')
 for s in expected:
  b=subprocess.check_output(['git','show',':'+s],cwd=R)
  if hashlib.sha256(b).hexdigest()!=digest(R/s)['sha256']:errors.append('staged bytes differ: '+s)
print(json.dumps(dict(passed=not errors,counts=counts,failures=errors,manifestIdentity=digest(E/'FINAL_FINGERPRINTS_V4.json'),scope='Identity only; not human or comprehensive validation.'),indent=2))
raise SystemExit(bool(errors))
