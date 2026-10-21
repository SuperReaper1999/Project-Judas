from pathlib import Path
import json,hashlib,gzip,shutil,subprocess,datetime
root=Path(__file__).resolve().parents[2];here=Path(__file__).resolve().parent;out=root/'docs/evidence/prealpha-pair-discovery-20261021'
if out.exists():raise SystemExit('fresh evidence destination required')
out.mkdir(parents=True);rows=[]
sha=lambda b:hashlib.sha256(b).hexdigest()
def copy(path,name=None):
 raw=path.read_bytes();name=name or str(path.relative_to(here));dest=out/name
 if path.suffix in ['.csv','.jsonl','.log','.json'] and len(raw)>32768:
  dest=Path(str(dest)+'.gz');dest.parent.mkdir(parents=True,exist_ok=True)
  with dest.open('wb') as f:
   with gzip.GzipFile(filename='',mode='wb',fileobj=f,mtime=0) as z:z.write(raw)
  assert gzip.decompress(dest.read_bytes())==raw
 else:dest.parent.mkdir(parents=True,exist_ok=True);dest.write_bytes(raw)
 rows.append({'source':str(path.relative_to(root)),'stored':str(dest.relative_to(out)),'original_bytes':len(raw),'stored_bytes':dest.stat().st_size,'original_sha256':sha(raw),'stored_sha256':sha(dest.read_bytes())})
runs=['baseline-sampled900','baseline-stationary-profile900','baseline-wasd-live35','baseline-wasd900','pairs900','remaining900','baseline-normal900','remaining-profile900','negative-probe900','combined900','baseline1800','combined1800','baseline-stationary-live35','combined-stationary-live35','baseline-wasd-live35-v3','combined-wasd-live35','baseline-wasd-final','combined-wasd-final','combined-profile900','parity900']
for name in runs:
 folder=here/name
 for f in sorted(folder.rglob('*')):
  rel=f.relative_to(folder)
  if not f.is_file() or any(k in rel.parts for k in ['userdata','cache','config']):continue
  if f.suffix in ['.csv','.json','.jsonl','.log','.txt']:copy(f)
for name in ['diagnostic-build','combined','variants','prototypes','direct-tests','both-active-control','agent-impact-analysis','symbols-early','final-analysis','qualification','cold','integration','delivery','export-followup']:
 folder=here/name
 for f in sorted(folder.rglob('*')):
  rel=f.relative_to(folder)
  if not f.is_file() or any(k in rel.parts for k in ['userdata','cache','config','native','objects','editor-project','exported']):continue
  if f.suffix in ['.cpp','.h','.py','.json','.log','.txt','.csv'] or f.name=='standalone.steps':copy(f)
for f in sorted(here.iterdir()):
 if f.is_file() and f.suffix in ['.json','.log','.py','.h']:copy(f,'tools-and-records/'+f.name)
copy(here/'baseline/FROZEN.json','baseline/FROZEN.json')
for name in ['src/PhysicsWorld.cpp','src/physics/WorldBounds.cpp','src/physics/WorldIncrementalImpact.cpp']:
 copy(here/'baseline/source'/name,'baseline/source/'+name)
# Byte-exact original header from the accepted baseline, with no binary copies.
original=subprocess.check_output(['git','show','129509774df1c5e1ac7996f47430f67b232bdd00:src/Broadphase.h'],cwd=root);dest=out/'baseline/source/src/Broadphase.h';dest.parent.mkdir(parents=True,exist_ok=True);dest.write_bytes(original)
rows.append({'source':'git:12950977:src/Broadphase.h','stored':str(dest.relative_to(out)),'original_bytes':len(original),'stored_bytes':len(original),'original_sha256':sha(original),'stored_sha256':sha(original)})
index={'schema':'judas.pair-discovery.evidence.v1','utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'rows':rows,'original_bytes':sum(x['original_bytes'] for x in rows),'stored_bytes':sum(x['stored_bytes'] for x in rows),'notes':['Raw measurements are never rewritten. Large text tables/logs use lossless deterministic gzip; original/stored hashes are both retained.','No executable, archive, build object, unrelated project, userdata cache or original research is committed.','baseline-sampled900 is a watch-tour control with different gameplay. It is excluded from the matched unchanged stationary workload.','baseline-wasd900 records a source-stability rejection due to concurrent header editing; its frozen binary physics is useful context, not clean-source timing acceptance.','parity900 and negative-probe900 contain diagnostic engine instrumentation and are not normal performance comparisons.','The original export refusal and corrected export follow-up have separate receipts. The final commit identity is linked and exported after checkpointing without changing the qualified core.']}
(out/'INDEX.json').write_text(json.dumps(index,indent=2)+'\n')
print(json.dumps({'payload_files':len(rows),'original_bytes':index['original_bytes'],'stored_bytes':index['stored_bytes']},indent=2))
