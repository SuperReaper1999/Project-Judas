from pathlib import Path
import gzip,hashlib,json,datetime
r=Path.cwd();src=r/'.cache/prealpha-pass4/dense';dst=r/'docs/evidence/prealpha-pass4/dense';dst.mkdir(exist_ok=True)
def save(data,relative,compress=False):
 p=dst/relative;p.parent.mkdir(parents=True,exist_ok=True);payload=gzip.compress(data,compresslevel=6,mtime=0)if compress else data;p.write_bytes(payload)
def copy(p,relative,compress=False):
 if p.is_file():save(p.read_bytes(),relative,compress)
for n in ['REPORT.md','FINAL_MATCHED_RESULTS.json','FINAL_WORST_STEP_PROFILE.json','SOURCE_FINGERPRINTS.json','INTEGRATED_BUILD.json','INTEGRATED_CONTROLS.json','DIRTY_WITNESS.json','INTEGRATED_SCHEDULING.patch']:copy(src/n,n)
for n in ['relink_checks.py','relink_workloads.py','relink_observer.py','build_dirty_observer.py','package_final.py']:copy(src/n,Path('reproduction')/n)
for n in ['WorldImpactEvents.refactor-baseline.cpp','WorldImpactEvents.integrated.cpp','WorldImpactEvents.observer.cpp']:copy(src/n,Path('reproduction')/n)
for label in ['integrated-checks','integrated-controls','integrated-bin','observer-checks','dirty-observer','dirty-negative','strict-baseline','strict-integrated']:
 for p in(src/label).rglob('*'):
  if not p.is_file()or p.suffix not in ['.json','.csv','.log']:continue
  rel=Path('raw')/label/p.relative_to(src/label);compress=p.suffix=='.csv'or p.stat().st_size>250000;copy(p,str(rel)+'.gz'if compress else rel,compress)
copy(src/'build-integrated.log','raw/build-integrated.log')
files=[]
for p in sorted(dst.rglob('*')):
 if p.is_file()and p.name!='MANIFEST.json':files.append({'path':str(p.relative_to(dst)),'bytes':p.stat().st_size,'sha256':hashlib.sha256(p.read_bytes()).hexdigest()})
(dst/'MANIFEST.json').write_text(json.dumps({'schema':'judas.prealpha.pass4.dense.evidence.v2','utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'scope':'Initial isolated/confounded experiment preserved separately; final exact production scheduling patch, actual dirty-union positive/negative controls, same-object native regressions, four physical controls and strict serialized270-step pair. No meaningful worst-tail or memory-gain claim.','files':files},indent=2)+'\n');print('packaged',len(files),'files',sum(x['bytes']for x in files),'bytes')
