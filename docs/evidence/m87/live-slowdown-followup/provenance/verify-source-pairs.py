from pathlib import Path
import gzip,re,hashlib,json,io
out=Path('/home/conner/Documents/GitHub/Project-Judas/docs/evidence/m87/live-slowdown-followup');records=[]
def apply(source,patch,reverse=False):
 lines=source.splitlines(keepends=True);result=[];position=0;patchlines=patch.splitlines(keepends=True);i=0
 while i<len(patchlines):
  header=re.match(r'@@ -(\d+)(?:,(\d+))? \+(\d+)(?:,(\d+))? @@',patchlines[i])
  if not header:i+=1;continue
  start=int(header.group(3 if reverse else 1));count=int(header.group(4 if reverse else 2)or'1');start=start-1 if count else start
  if start<position:raise ValueError('overlapping hunk')
  result.extend(lines[position:start]);position=start;i+=1
  while i<len(patchlines)and not patchlines[i].startswith('@@ '):
   line=patchlines[i];i+=1
   if line.startswith('\\ No newline'):continue
   if not line or line[0]not in' +-':break
   kind=line[0];body=line[1:]
   if reverse:kind={'+':'-','-':'+',' ':' '}[kind]
   if kind in' -':
    if position>=len(lines)or lines[position]!=body:raise ValueError('patch context mismatch')
    position+=1
   if kind in' +':result.append(body)
 result.extend(lines[position:]);return''.join(result)
def read(path):
 p=out/path
 return(gzip.decompress(p.read_bytes())if p.suffix=='.gz'else p.read_bytes()).decode()
pairs=[
 ('loop-bookkeeping','m87-loop-scalability.cpp.gz','m87-loop-scalability.patch','m87-loop-base.cpp.gz'),
 ('truth-cache-proposal','m87-step-truth-cache.cpp.gz','m87-step-truth-cache.patch','m87-loop-base.cpp.gz'),
 ('manifold-cache-rejected','m87-manifold-cache.cpp.gz','m87-manifold-cache.patch','m87-manifold-cache-base.cpp.gz'),
 ('lazy-rejected','PhysicsWorld.lazy.cpp.gz','pending-lazy.diff','m87-before-lazy.cpp.gz'),
 ('query-workers-rejected','m87-parallel-prototype.cpp.gz','m87-parallel-prototype.patch','m87-parallel-base.cpp.gz'),
 ('initial-narrowphase','m87-narrow-prototype.cpp.gz','m87-narrow-prototype.patch','m87-narrow-base.cpp.gz'),
 ('persistent-deadline-rejected','PhysicsWorld.cpp.gz','persistent-lazy.diff','PhysicsWorld.base.cpp.gz'),
 ('future-tree-rejected','PhysicsWorld.cpp.gz','future-tree.diff','PhysicsWorld.base.cpp.gz'),
]
for group,variant,patch,base in pairs:
 prefix='experiments/'+group+'/';v=read(prefix+variant);p=read(prefix+patch);b=read(prefix+base);r={'group':group,'variant':prefix+variant,'patch':prefix+patch,'archived_base':prefix+base}
 try:
  reconstructed=apply(v,p,True);r['reverse_reconstructed_base_sha256']=hashlib.sha256(reconstructed.encode()).hexdigest();r['archived_base_sha256']=hashlib.sha256(b.encode()).hexdigest();r['archived_base_matches_reverse']=b==reconstructed;r['forward_reproduces_exact_variant']=apply(reconstructed,p)==v
  if b!=reconstructed:
   dest=out/prefix/'reverse-reconstructed-base.cpp.gz';buf=io.BytesIO()
   with gzip.GzipFile(filename='',fileobj=buf,mode='wb',mtime=0,compresslevel=6)as f:f.write(reconstructed.encode())
   dest.write_bytes(buf.getvalue());r['correct_matching_base']=str(dest.relative_to(out));r['derivation']='Reversed retained unified patch against retained exact variant; reapplying reproduces byte-exact variant. This source reconstruction was performed during evidence collation, not measured in a new run.'
 except Exception as e:r['verification_error']=str(e)
 records.append(r)
(out/'provenance/EXPERIMENT_SOURCE_PAIRS.json').write_text(json.dumps(records,indent=2)+'\n')
print(json.dumps(records,indent=2))
