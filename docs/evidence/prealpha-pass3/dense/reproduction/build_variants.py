#!/usr/bin/env python3
"""Isolated object replacement against a frozen accepted engine archive.
No shared build writes. Embedded identity remains frozen parent; explicit source
hashes below identify experimental code. Coordinator schedules CPU work.
"""
from pathlib import Path
import argparse,datetime,hashlib,json,shlex,shutil,subprocess,time
p=argparse.ArgumentParser();p.add_argument('--root',type=Path,default=Path.cwd());p.add_argument('--variants',nargs='+',default=['baseline','single-context','selected-normal']);p.add_argument('--engine',action='store_true');p.add_argument('--observer',action='store_true');a=p.parse_args();r=a.root.resolve();o=r/'.cache/prealpha-pass3/dense';frozen=r/'.cache/prealpha-pass3/baseline/bin';records=[]
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def run(cmd,log):
 start=time.monotonic()
 with log.open('w')as stream:subprocess.run(list(map(str,cmd)),cwd=r/'build',stdout=stream,stderr=subprocess.STDOUT,check=True)
 records.append({'argv':list(map(str,cmd)),'log':str(log),'seconds':time.monotonic()-start});print('built',log.name,flush=True)
def flags(target):
 lines=(r/'build/CMakeFiles'/f'{target}.dir/flags.make').read_text().splitlines();out=[]
 for n in ['CXX_DEFINES','CXX_INCLUDES','CXX_FLAGS']:out+=shlex.split(next(x.split('=',1)[1] for x in lines if x.startswith(n+' =')))
 return out
def link(target,out,archive):
 args=shlex.split((r/'build/CMakeFiles'/f'{target}.dir/link.txt').read_text());objects=[str((r/'build'/x).resolve())for x in args if x.endswith('.o')];libs=args[args.index('libjudas_engine.a')+1:];libs=[str((r/'build'/x).resolve())if x.endswith('.a')and not x.startswith('/')else x for x in libs]
 run(['/usr/bin/c++','-O3','-DNDEBUG',*objects,'-o',out,archive,*libs],out.with_suffix('.link.log'))
for v in a.variants:
 b=o/('build-'+v);b.mkdir(exist_ok=True);src=o/f'Contacts.{v}.cpp';probe=o/'PrimitiveWitnessProbe.cpp'
 run(['/usr/bin/c++','-O3','-DNDEBUG','-std=c++17','-I'+str(r/'src'),probe,src,'-o',b/'primitive'],b/'primitive.build.log')
 if a.engine:
  run(['/usr/bin/c++',*flags('judas_engine'),'-c',src,'-o',b/'Contacts.cpp.o'],b/'contacts.build.log')
  physics=o/('PhysicsWorld.baseline.cpp'if v=='baseline'else'PhysicsWorld.sorted-membership.cpp')
  if a.observer:physics=o/('PhysicsWorld.observer-baseline.cpp'if v=='baseline'else'PhysicsWorld.observer-candidate.cpp')
  run(['/usr/bin/c++',*flags('judas_engine'),'-c',physics,'-o',b/'PhysicsWorld.cpp.o'],b/'physics.build.log')
  archive=b/'libjudas_engine.a';shutil.copyfile(frozen/'libjudas_engine.a',archive);run(['ar','r',archive,b/'Contacts.cpp.o',b/'PhysicsWorld.cpp.o'],b/'archive.log')
  for target in ['judas_physics_stress_live_benchmark','judas_physics_scaling_diagnostics']:
   link(target,b/target,archive)
  records.append({'variant':v,'kind':'isolated_parent_archive_object_replacement','baselineArchiveSha256':sha(frozen/'libjudas_engine.a'),'sources':{str(p.relative_to(r)):sha(p)for p in [src,physics,probe]},'archiveSha256':sha(archive),'binaries':{name:sha(b/name)for name in ['primitive','judas_physics_stress_live_benchmark','judas_physics_scaling_diagnostics']},'embeddedBuildIdentityNote':'Parent f647d7fd accepted identity preserved in archive; changed source hashes above identify actual experiment,not a new pure parent build.'})
 else:records.append({'variant':v,'sources':{str(p.relative_to(r)):sha(p)for p in [src,probe]},'binarySha256':sha(b/'primitive')})
(o/('BUILD_OBSERVER.json'if a.observer else 'BUILD_VARIANTS.json')).write_text(json.dumps({'utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'records':records},indent=2)+'\n')
