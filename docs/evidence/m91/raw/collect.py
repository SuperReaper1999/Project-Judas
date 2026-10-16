from pathlib import Path
import json,hashlib,gzip,io,shutil
root=Path.cwd();cache=root/'.cache/m91';dest=root/'docs/evidence/m91';dest.mkdir(parents=True,exist_ok=True)
sha=lambda data:hashlib.sha256(data).hexdigest()
allowed={'.json','.csv','.log','.cpp','.h','.py','.txt','.patch','.script','.jlocal','.judas','.judasproj','.judasui','.judasprefab','.judasmeta','.save'}
logical=[]
if (dest/'raw').exists():shutil.rmtree(dest/'raw')
selected={'baseline','before-scaling','scaling-verified','scaling-verified-repeated','final-dense','dense-verified','profile-verified','closed-long','native','verified-native','balance-bisect','experiments','negative','alloc','consumer-verified','concave','verified-cold','verified-delivery','watch-verified','frozen','before-dense','before-dense-registered','cache-dense','block-dense','block-long','identity-long','patch-long','certificate-long','relative-long','coherent-long','warm-long','identity-only-long','load-long','adaptive-long','partition-long','closed-dense'}
plain={'SOURCE.json','BINARIES.json','MATCHED_OBSERVER_LINK.json','RESULTS.json','run.json','results.json','RECEIPT.json','PLAN.json','LINK.json','FIXTURE.json','LAZY.json','VERIFIED_RUNS.json','PACKAGE_FILES.json','SUMMARY.json','summary.json','quality.json'}
def copy(src,rel):
 data=src.read_bytes();stored=dest/rel
 compressed=(('profile' in src.name and src.suffix=='.json') or (len(data)>8192 and src.name not in plain and src.suffix not in {'.py','.txt','.script'}))
 if compressed:
  stored=Path(str(stored)+'.gz');buffer=io.BytesIO()
  with gzip.GzipFile(filename='',mode='wb',fileobj=buffer,mtime=0)as stream:stream.write(data)
  encoded=buffer.getvalue()
 else:encoded=data
 stored.parent.mkdir(parents=True,exist_ok=True);stored.write_bytes(encoded)
 logical.append({'original':str(src.relative_to(root)) if src.is_relative_to(root) else str(src),'stored':str(stored.relative_to(root)),'uncompressed_bytes':len(data),'uncompressed_sha256':sha(data),'stored_bytes':len(encoded),'stored_sha256':sha(encoded),'gzip':compressed})
for src in sorted(cache.rglob('*')):
 if not src.is_file() or src.suffix not in allowed:continue
 # Packages/private authored copies are tested outputs, not inputs for a new
 # solver experiment. Their fingerprints and saved snapshots are retained.
 rel=src.relative_to(cache)
 if len(rel.parts)>1 and rel.parts[0] not in selected:continue
 if src.suffix=='.json' and 'profile' in src.name:
  if src.name=='profile-final.json' and (src.parent/'profile-shutdown.json').exists():continue
  if rel.parts[0] not in {'before-dense-registered','profile-verified','before-scaling','scaling-verified','concave','verified-delivery','watch-verified'}:continue
 if rel.parts[0]=='native' and src.name not in {'RESULTS.json','physical_balance_controller.log'}:continue
 if rel.parts[0]=='concave' and len(rel.parts)>1 and rel.parts[1] in {'after-raw','after-closed-raw'}:continue
 if any(p in {'package','project','editor-project','xdg-config'}for p in rel.parts):continue
 copy(src,Path('raw')/rel)
for src in sorted(Path('/tmp').glob('m91-*.log')):copy(src,Path('raw/build-and-run-logs')/src.name)
for src in [cache/'verified-delivery/editor.edit.png',cache/'verified-delivery/editor.play.png',Path('/tmp/judas-m91-verified-delivery/package-powered.png'),Path('/tmp/judas-m91-verified-delivery/package-rotated.png')]:
 if src.exists():copy(src,Path('images')/src.name)
(dest/'ARTIFACTS.json').write_text(json.dumps({'scope':'Exact raw inputs, developmental failures, final receipts and source snapshots. No executables, static archives, objects, core dumps or user environment dumps. Gzip is deterministic and lossless.','files':logical},indent=2)+'\n')
print(len(logical),'artifacts',sum(x['stored_bytes']for x in logical),'stored bytes')
