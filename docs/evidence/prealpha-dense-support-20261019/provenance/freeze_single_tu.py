#!/usr/bin/env python3
"""Freeze an explicit isolated one-TU variant from immutable safe snapshot bytes."""
from pathlib import Path
import argparse,datetime,gzip,hashlib,io,json,shutil,tarfile
P=argparse.ArgumentParser();P.add_argument('--base',type=Path,required=True);P.add_argument('--archive',type=Path,required=True);P.add_argument('--binary',type=Path,required=True);P.add_argument('--source-name',required=True);P.add_argument('--source',type=Path,required=True);P.add_argument('--changed-member',required=True);P.add_argument('--receipt',type=Path,action='append',default=[]);P.add_argument('--output',type=Path,required=True);a=P.parse_args()
a.base=a.base.resolve();a.output=a.output.resolve();a.archive=a.archive.resolve();a.binary=a.binary.resolve();a.source=a.source.resolve()
sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
def members(path):
 data=Path(path).read_bytes();assert data[:8]==b'!<arch>\n';p=8;long=b'';out=[]
 while p<len(data):
  h=data[p:p+60];assert h[58:60]==b'`\n';name=h[:16].decode().strip();n=int(h[48:58]);v=data[p+60:p+60+n];p+=60+n+(n%2)
  if name=='//':long=v;continue
  if name in ('/','/SYM64/'):continue
  if name.startswith('/') and name[1:].isdigit():name=long[int(name[1:]):].split(b'/\n',1)[0].decode()
  else:name=name.rstrip('/')
  out.append((name,hashlib.sha256(v).hexdigest()))
 return out
f=json.loads((a.base/'FROZEN.json').read_text());assert sha(a.base/'libjudas_engine.a')==f['artifacts']['libjudas_engine.a']['copy_sha256'];assert sha(a.base/'source-inputs.tar.gz')==f['source_archive']['sha256']
before=members(a.base/'libjudas_engine.a');after=members(a.archive);assert len(before)==len(after);changes=[]
for i,(x,y) in enumerate(zip(before,after)):
 assert x[0]==y[0]
 if x!=y:changes.append({'ordinal':i,'name':x[0],'before':x[1],'after':y[1]})
assert len(changes)==1 and changes[0]['name']==a.changed_member,(changes,a.changed_member)
a.output.mkdir(exist_ok=False);(a.output/'bin').mkdir();(a.output/'provenance').mkdir()
for name,old in f['artifacts'].items():
 if old.get('unavailable'):continue
 src=a.archive if name=='libjudas_engine.a' else a.binary if name=='bin/judas_physics_stress_live_benchmark' else a.base/name
 dst=a.output/name;dst.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(src,dst);h=sha(dst);assert h==sha(src);f['artifacts'][name]={'path':str(src),'sha256':h,'copy_sha256':h,'bytes':dst.stat().st_size}
overlay=a.source.read_bytes();found=False
with (a.output/'source-inputs.tar.gz').open('wb') as raw:
 with gzip.GzipFile(filename='',mode='wb',fileobj=raw,mtime=0,compresslevel=1) as z:
  with tarfile.open(fileobj=z,mode='w') as out,tarfile.open(a.base/'source-inputs.tar.gz') as old:
   for m in old.getmembers():
    if m.name==a.source_name:data=overlay;found=True
    else:data=old.extractfile(m).read()
    m.size=len(data);out.addfile(m,io.BytesIO(data))
assert found,'overlay source must exist in original immutable archive'
f['source_inputs'][a.source_name]={'bytes':len(overlay),'sha256':hashlib.sha256(overlay).hexdigest()};f['source_archive']={'path':str(a.output/'source-inputs.tar.gz'),'sha256':sha(a.output/'source-inputs.tar.gz'),'bytes':(a.output/'source-inputs.tar.gz').stat().st_size}
receipts=[]
for p in a.receipt:
 dst=a.output/'provenance'/p.name;assert not dst.exists();shutil.copy2(p,dst);assert sha(p)==sha(dst);receipts.append({'path':str(dst.relative_to(a.output)),'sha256':sha(dst)})
f['variant']={'name':a.output.name,'safe_checkpoint':'8f6bd18b876158c131bee61fa0dc71625d912c30','base_snapshot_manifest_sha256':sha(a.base/'FROZEN.json'),'base_source_archive_sha256':sha(a.base/'source-inputs.tar.gz'),'explicit_source_overlays':[a.source_name],'ordered_archive_member_changes':changes,'receipts':receipts,'note':'Single isolated TU source/object variant. Original e30 dirty source provenance and safe8f checkpoint identity deliberately separate.'};f['finished_utc']=datetime.datetime.now(datetime.timezone.utc).isoformat();f['notes']=['Exact binary/archive copies; independently parsed ordered archive members verify only the named object differs, including duplicate member names.','Source archive uses immutable base bytes plus one explicit prototype source overlay; no live source collection.'];(a.output/'FROZEN.json').write_text(json.dumps(f,indent=2,sort_keys=True)+'\n');print(json.dumps({'snapshot':str(a.output),'manifest_sha256':sha(a.output/'FROZEN.json'),'binary_sha256':sha(a.output/'bin/judas_physics_stress_live_benchmark'),'archive_sha256':sha(a.output/'libjudas_engine.a'),'source_archive_sha256':f['source_archive']['sha256']},indent=2))
