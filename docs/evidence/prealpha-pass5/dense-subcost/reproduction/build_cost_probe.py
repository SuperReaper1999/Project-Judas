#!/usr/bin/env python3
"""Two isolated diagnostic objects, no shared archive or production edits."""
from pathlib import Path
import datetime,hashlib,json,shlex,shutil,subprocess,time
r=Path.cwd();out=r/'.cache/prealpha-pass5/dense/cost-probe';records=[]
base=r/'.cache/prealpha-pass5/baseline/libjudas_engine.a'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def run(name,args):
 start=time.monotonic()
 with(out/(name+'.log')).open('w')as f: result=subprocess.run(list(map(str,args)),cwd=r/'build',stdout=f,stderr=subprocess.STDOUT)
 records.append(dict(name=name,argv=list(map(str,args)),cwd=str(r/'build'),exit=result.returncode,seconds=time.monotonic()-start))
 if result.returncode:raise SystemExit(name+' failed')
lines=(r/'build/CMakeFiles/judas_engine.dir/flags.make').read_text().splitlines();flags=[]
for label in ['CXX_DEFINES','CXX_INCLUDES','CXX_FLAGS']:flags+=shlex.split(next(x.split('=',1)[1]for x in lines if x.startswith(label+' =')))
objects=[]
for name in ['Contacts','WorldImpactEvents']:
 source=out/(name+'.cpp');obj=out/(name+'.cpp.o');run('compile-'+name,['/usr/bin/c++',*flags,'-I'+str(r/'src/physics'),'-c',source,'-o',obj]);objects.append(obj)
archive=out/'libjudas_engine.a';shutil.copyfile(base,archive);run('archive',['ar','r',archive,*objects])
target='judas_physics_stress_live_benchmark';args=shlex.split((r/'build/CMakeFiles'/f'{target}.dir/link.txt').read_text());objects=[str((r/'build'/x).resolve())for x in args if x.endswith('.o')]
libs=args[args.index('libjudas_engine.a')+1:];libs=[str((r/'build'/x).resolve())if x.endswith('.a')and not x.startswith('/')else x for x in libs]
run('link',['/usr/bin/c++','-O3','-DNDEBUG',*objects,'-o',out/target,archive,*libs])
(out/'BUILD.json').write_text(json.dumps(dict(utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),head=subprocess.check_output(['git','rev-parse','HEAD'],cwd=r,text=True).strip(),records=records,sources={str(p.relative_to(r)):sha(p)for p in sorted(out.iterdir())if p.suffix in ['.cpp','.h']},baseArchiveSha256=sha(base),archiveSha256=sha(archive),binarySha256=sha(out/target),scope='CACHE-ONLY attribution, actual two-TU source hashes explicit. Embedded identity is whatever current link identity records; no observer cost used for performance acceptance.'),indent=2)+'\n')
print('Built isolated subcost observer',sha(out/target),flush=True)
