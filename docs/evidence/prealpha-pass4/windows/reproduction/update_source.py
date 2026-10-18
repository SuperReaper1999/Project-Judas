"""Task-only verified no-Git source replacement; preserves original baseline copy."""
import hashlib,json,os,sys,tarfile,time
from pathlib import Path
root=Path(r'P:\JudasValidation\PreAlphaPass4')
receipt=json.loads((root/'final-source-receipt.json').read_text(encoding='utf-8-sig'))
archive=root/'final-source.tar.gz'
def sha(p):
 with p.open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()
if sha(archive)!=receipt['archiveSHA256']:raise RuntimeError('Transferred final archive hash mismatch')
source=root/'Source'; destination=root/'FinalSource'; baseline=root/'BaselineSource'
if baseline.exists():raise RuntimeError('Preserve existing baseline rather than overwrite')
if not destination.exists():
 with tarfile.open(archive) as t:t.extractall(destination,filter='data')
info=json.loads((destination/'JUDAS_SOURCE_INFO.json').read_text(encoding='utf-8'))
if info['revision']!=receipt['revision'] or info['source_dirty']:raise RuntimeError('Unexpected/dirty final source')
records=json.loads((destination/'JUDAS_SOURCE_FILES.json').read_text(encoding='utf-8'))['files']
actual={p.relative_to(destination).as_posix() for p in destination.rglob('*') if p.is_file()}
expected={r['path'] for r in records}|{'JUDAS_SOURCE_INFO.json','JUDAS_SOURCE_FILES.json'}
if actual!=expected:raise RuntimeError('Final archive path inventory mismatch')
old_records={r['path']:r['sha256'] for r in json.loads((source/'JUDAS_SOURCE_FILES.json').read_text(encoding='utf-8'))['files']}
changed=[]
for row in records:
 p=destination/row['path']
 if sha(p)!=row['sha256']:raise RuntimeError('Source byte mismatch: '+row['path'])
 if old_records.get(row['path'])!=row['sha256']:
  changed.append(row['path'])
  # Reproducible archives have epoch mtimes: make changed existing inputs newer
  # than cached baseline objects so an incremental native build cannot miss them.
  os.utime(p,None)
for name in ['JUDAS_SOURCE_INFO.json','JUDAS_SOURCE_FILES.json']:os.utime(destination/name,None)
source.rename(baseline);destination.rename(source)
output={'schema':1,'utc':time.strftime('%Y-%m-%dT%H:%M:%SZ',time.gmtime()),'archiveSHA256':sha(archive),'source':info,'filesVerified':len(records),'changedPaths':changed,'removedPaths':sorted(set(old_records)-{r['path'] for r in records}),'baselinePreservedAt':str(baseline),'buildCacheReused':True,'changedInputMtimesRefreshed':True}
(root/'FINAL_SOURCE_VERIFIED.json').write_text(json.dumps(output,indent=2)+'\n')
print(json.dumps({'verified':len(records),'changed':len(changed),'revision':info['revision']}))
