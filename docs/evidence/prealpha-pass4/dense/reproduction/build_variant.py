#!/usr/bin/env python3
"""Isolated PhysicsWorld object replacement; source hashes distinguish experiments."""
from pathlib import Path
import argparse, datetime, hashlib, json, shlex, shutil, subprocess, time
p=argparse.ArgumentParser();p.add_argument('variant');p.add_argument('--archive',type=Path,required=True);a=p.parse_args()
r=Path.cwd();o=r/'.cache/prealpha-pass4/dense';b=o/('build-'+a.variant);b.mkdir(exist_ok=True);src=o/('PhysicsWorld.'+a.variant+'.cpp');records=[]
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def run(cmd,log):
 start=time.monotonic()
 with log.open('w')as stream:subprocess.run(list(map(str,cmd)),cwd=r/'build',stdout=stream,stderr=subprocess.STDOUT,check=True)
 records.append({'argv':list(map(str,cmd)),'seconds':time.monotonic()-start,'log':str(log)});print('built',log.name,flush=True)
lines=(r/'build/CMakeFiles/judas_engine.dir/flags.make').read_text().splitlines();flags=[]
for n in ['CXX_DEFINES','CXX_INCLUDES','CXX_FLAGS']:flags+=shlex.split(next(x.split('=',1)[1]for x in lines if x.startswith(n+' =')))
obj=b/'PhysicsWorld.cpp.o';run(['/usr/bin/c++',*flags,'-c',src,'-o',obj],b/'build.log')
archive=b/'libjudas_engine.a';shutil.copyfile(a.archive,archive);run(['ar','r',archive,obj],b/'archive.log')
for name in ['judas_physics_stress_live_benchmark','judas_physics_scaling_diagnostics']:
 args=shlex.split((r/'build/CMakeFiles'/f'{name}.dir/link.txt').read_text());objects=[str((r/'build'/x).resolve())for x in args if x.endswith('.o')];libs=args[args.index('libjudas_engine.a')+1:];libs=[str((r/'build'/x).resolve())if x.endswith('.a')and not x.startswith('/')else x for x in libs];binary=b/name
 run(['/usr/bin/c++','-O3','-DNDEBUG',*objects,'-o',binary,archive,*libs],b/(name+'.link.log'))
(b/'BUILD.json').write_text(json.dumps({'utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'commands':records,'source':{'path':str(src.relative_to(r)),'sha256':sha(src)},'baselineArchive':{'path':str(a.archive),'sha256':sha(a.archive)},'archiveSha256':sha(archive),'binaries':{n:sha(b/n)for n in ['judas_physics_stress_live_benchmark','judas_physics_scaling_diagnostics']},'note':'Cache-only changed-object experiment against exact accepted 7da56c2c parent archive; embedded identity remains parent, explicit source hash is actual candidate identity.'},indent=2)+'\n')
