from pathlib import Path
import gzip,hashlib,json,datetime
r=Path.cwd();src=r/'.cache/prealpha-pass4/refactor/behavior';dst=r/'docs/evidence/prealpha-pass4/refactor';manifest=[]
def save(data,relative,compress=False):
 p=dst/relative;p.parent.mkdir(parents=True,exist_ok=True);payload=gzip.compress(data,compresslevel=6,mtime=0)if compress else data;p.write_bytes(payload)
def copy(p,relative,compress=False):
 if p.is_file():save(p.read_bytes(),relative,compress)
for n in ['CHECKS.json','STAGE.json','PERFORMANCE.json','LIVE_BUILD.json','SCALING_BUILDS.json','SCALING_FORMATTED_BUILD.json','FORMATTED_CONTROLS.json','FINAL_VALIDATED.json']:copy(src/n,n)
for n in ['compare_checks.py','relink_final.py','relink_final.wrong-cwd.py','package_evidence.py']:copy(src/n,Path('reproduction')/n)
copy(r/'.cache/prealpha-pass4/dense/run_workloads.py',Path('reproduction')/'run_workloads.py')
for path in src.rglob('*'):
 if not path.is_file()or path.suffix not in ['.json','.csv','.log']:continue
 if path.parent==src:continue
 relative=Path('raw')/path.relative_to(src);compress=path.suffix=='.csv'or path.stat().st_size>250000;copy(path,str(relative)+'.gz'if compress else relative,compress)
copy(r/'.cache/prealpha-pass4/baseline/FROZEN.json','FROZEN_BASELINE.json')
copy(r/'.cache/prealpha-pass4/dense/HARDWARE.json','HARDWARE.json')
save(b'* -text\n','.gitattributes')
for path in sorted(dst.rglob('*')):
 if not path.is_file()or path.name=='MANIFEST.json':continue
 manifest.append({'path':str(path.relative_to(dst)),'bytes':path.stat().st_size,'sha256':hashlib.sha256(path.read_bytes()).hexdigest()})
(dst/'MANIFEST.json').write_text(json.dumps({'schema':'judas.prealpha.pass4.physics-refactor.evidence.v1','utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'scope':'Pure world extraction/formatting, separate CMake completeness guard, portable independent token/state proof, same-object native old/new controls and final formatted-archive followup. Strict270 measurements belong to preformat token-identical extraction stage. No functional physics optimization or CCD correction in this checkpoint.','files':manifest},indent=2)+'\n');print('packaged',len(manifest),'files',sum(x['bytes']for x in manifest),'bytes')
