#!/usr/bin/env python3
"""Run one explicitly selected Release driver against an exact archive, in a fresh directory."""
import argparse,hashlib,json,pathlib,subprocess,time
p=argparse.ArgumentParser();p.add_argument('--source',required=True);p.add_argument('--archive',required=True);p.add_argument('--output',required=True);p.add_argument('--template',default='docs/evidence/m78-closure/audit-lifecycle/linux-current-01/run-04/IDENTITY.json');p.add_argument('--run-arg',action='append',default=[]);p.add_argument('--reuse-object',help='Prior fresh run directory; reuse only the same source and compile flags')
a=p.parse_args();root=pathlib.Path.cwd();out=(root/a.output).resolve()
if out.exists():raise SystemExit('Refusing existing output directory')
out.mkdir(parents=True);source=(root/a.source).resolve();archive=(root/a.archive).resolve()
sha=lambda path:hashlib.sha256(path.read_bytes()).hexdigest()
rec=json.loads((root/a.template).read_text())['recipes'][0]
compile=rec['compile'][:];compile[compile.index('-c')+1]=str(source);compile[compile.index('-o')+1]=str(out/'driver.o')
link=rec['link'][:];link=[str(out/'driver.o') if x.endswith('/streaming.o') else str(archive) if x.endswith('/libjudas_engine.a') else str(out/'driver') if x.endswith('/streaming.exe') else x for x in link]
identity={'source':str(source),'sourceSHA256':sha(source),'archive':str(archive),'archiveSHA256':sha(archive),'templateSHA256':sha(root/a.template),'compile':compile,'link':link,'linkInputs':{x:sha(pathlib.Path(x)) for x in link if pathlib.Path(x).is_file()}}
(out/'IDENTITY.json').write_text(json.dumps(identity,indent=2)+'\n');(out/'compiled-source.cpp').write_bytes(source.read_bytes())
rows=[]
phases=[('compile',compile)]
if a.reuse_object:
 prior=(root/a.reuse_object).resolve();old=json.loads((prior/'IDENTITY.json').read_text());normalize=lambda cmd:[x for i,x in enumerate(cmd)if i!=cmd.index('-o')+1]
 if old['sourceSHA256']!=sha(source) or normalize(old['compile'])!=normalize(compile):raise SystemExit('Object source/flags disagree')
 import shutil
 shutil.copyfile(prior/'driver.o',out/'driver.o');identity['reusedObject']={'path':str(prior/'driver.o'),'sha256':sha(out/'driver.o')};phases=[]
for name,cmd in phases+[('link',link),('run',[str(out/'driver'),str(out/'fixture')]+a.run_arg)]:
 start=time.monotonic();result=subprocess.run(cmd,cwd=root,stdout=subprocess.PIPE,stderr=subprocess.PIPE)
 (out/(name+'.stdout.log')).write_bytes(result.stdout);(out/(name+'.stderr.log')).write_bytes(result.stderr)
 rows.append({'phase':name,'command':cmd,'exit':result.returncode,'seconds':time.monotonic()-start})
 (out/'RECEIPTS.json').write_text(json.dumps(rows,indent=2)+'\n')
 print(name,result.returncode,round(rows[-1]['seconds'],3),flush=True)
 if result.returncode:
  if name!='run':print(result.stderr.decode(errors='replace')[-5000:])
  break
if (out/'driver').is_file():(out/'BINARY_SHA256.txt').write_text(sha(out/'driver')+'\n')
if (out/'driver.o').is_file():identity['objectSHA256']=sha(out/'driver.o')
(out/'IDENTITY.json').write_text(json.dumps(identity,indent=2)+'\n')
raise SystemExit(rows[-1]['exit'])
