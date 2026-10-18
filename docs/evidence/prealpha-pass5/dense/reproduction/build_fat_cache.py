#!/usr/bin/env python3
"""One isolated diagnostic object; production files/archives remain untouched."""
from pathlib import Path
import datetime,hashlib,json,shlex,shutil,subprocess,time
r=Path.cwd();out=r/'.cache/prealpha-pass5/dense/fat-cache';out.mkdir(exist_ok=False)
source=r/'.cache/prealpha-pass5/dense/WorldImpactEvents.fat-cache.cpp'
base=r/'.cache/prealpha-pass5/baseline/libjudas_engine.a'
records=[]
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def run(name,args):
 start=time.monotonic()
 with(out/(name+'.log')).open('w')as f: result=subprocess.run(list(map(str,args)),cwd=r/'build',stdout=f,stderr=subprocess.STDOUT)
 records.append(dict(name=name,argv=list(map(str,args)),cwd=str(r/'build'),exit=result.returncode,seconds=time.monotonic()-start))
 assert result.returncode==0,name
lines=(r/'build/CMakeFiles/judas_engine.dir/flags.make').read_text().splitlines();flags=[]
for label in ['CXX_DEFINES','CXX_INCLUDES','CXX_FLAGS']:flags+=shlex.split(next(x.split('=',1)[1]for x in lines if x.startswith(label+' =')))
obj=out/'WorldImpactEvents.cpp.o';run('compile',['/usr/bin/c++',*flags,'-I'+str(r/'src/physics'),'-c',source,'-o',obj])
archive=out/'libjudas_engine.a';shutil.copyfile(base,archive);run('archive',['ar','r',archive,obj])
target='judas_physics_stress_live_benchmark';args=shlex.split((r/'build/CMakeFiles'/f'{target}.dir/link.txt').read_text())
objects=[str((r/'build'/x).resolve())for x in args if x.endswith('.o')]
libs=args[args.index('libjudas_engine.a')+1:];libs=[str((r/'build'/x).resolve())if x.endswith('.a')and not x.startswith('/')else x for x in libs]
run('link',['/usr/bin/c++','-O3','-DNDEBUG',*objects,'-o',out/target,archive,*libs])
(out/'BUILD.json').write_text(json.dumps(dict(utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),head=subprocess.check_output(['git','rev-parse','HEAD'],cwd=r,text=True).strip(),records=records,
 source=dict(path=str(source.relative_to(r)),sha256=sha(source)),helperSha256=sha(r/'.cache/prealpha-pass5/dense/ImpactCandidateCache.h'),baseArchiveSha256=sha(base),archiveSha256=sha(archive),binarySha256=sha(out/target),
 scope='CACHE-ONLY exact fat-candidate prototype; embedded build identity remains clean4c while actual observer source SHA is explicit. All original filtering/geometry/TOI calls remain active; cache changes only sorted candidate discovery.'),indent=2)+'\n')
print('Built isolated fat candidate',sha(out/target),flush=True)
