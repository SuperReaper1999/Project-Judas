from pathlib import Path
import gzip,hashlib,json,datetime
r=Path.cwd();src=r/'.cache/prealpha-pass4/dense';dst=r/'docs/evidence/prealpha-pass4/dense';dst.mkdir(parents=True,exist_ok=True);manifest=[]
def save(data,relative,compress=False):
 p=dst/relative;p.parent.mkdir(parents=True,exist_ok=True);stored=gzip.compress(data,compresslevel=6,mtime=0)if compress else data;p.write_bytes(stored);manifest.append({'path':str(relative),'bytes':len(stored),'sha256':hashlib.sha256(stored).hexdigest(),'uncompressedBytes':len(data),'uncompressedSha256':hashlib.sha256(data).hexdigest(),'compression':'gzip'if compress else'none'})
def copy(p,relative=None,compress=False):
 if p.is_file():save(p.read_bytes(),str(relative or p.relative_to(src)),compress)
for n in ['REPORT.md','SOURCE_AT_ASSIGNMENT.json','HARDWARE.json','HASH_DIRTY_RESULTS.json','EXACT_SCHEDULING.patch']:copy(src/n)
copy(r/'.cache/prealpha-pass4/baseline/FROZEN.json','FROZEN_BASELINE.json')
for n in ['build_variant.py','run_workloads.py','compare.py','package_evidence.py']:copy(src/n,Path('reproduction')/n)
for n in ['PhysicsWorld.baseline.cpp','PhysicsWorld.hash-dirty.cpp']:copy(src/n,Path('reproduction')/(n+'.gz'),True)
for label in ['baseline','hash-dirty']:
 for p in (src/label).rglob('*'):
  if not p.is_file()or p.suffix not in ['.json','.csv','.log']:continue
  rel=Path('raw')/label/p.relative_to(src/label);compress=p.suffix=='.csv'or p.stat().st_size>250000;copy(p,str(rel)+'.gz'if compress else rel,compress)
for p in (src/'build-hash-dirty').glob('*'):
 if p.is_file()and(p.suffix=='.log'or p.name=='BUILD.json'):copy(p,Path('raw')/'build-hash-dirty'/p.name)
save(b'* -text\n','.gitattributes')
(dst/'MANIFEST.json').write_text(json.dumps({'schema':'judas.prealpha.pass4.dense.evidence.v1','utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'scope':'Exact accepted baseline and one isolated derived-scheduling candidate. Experimental timing overlap disclosed; final integrated candidate measured separately.','files':manifest},indent=2)+'\n');print('packaged',len(manifest),'files',sum(x['bytes']for x in manifest),'bytes')
