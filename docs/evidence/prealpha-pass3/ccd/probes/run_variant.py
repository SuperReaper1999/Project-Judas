from pathlib import Path
import datetime,hashlib,json,shlex,subprocess,sys,time
root=Path.cwd(); scratch=root/'.cache/prealpha-pass3/ccd'; name=sys.argv[1]; source=Path(sys.argv[2]).resolve(); out=scratch/name; out.mkdir(exist_ok=False)
flags=(root/'build/CMakeFiles/judas_engine.dir/flags.make').read_text().splitlines(); args=[]
for key in ['CXX_DEFINES','CXX_INCLUDES','CXX_FLAGS']:args+=shlex.split(next(line.split('=',1)[1] for line in flags if line.startswith(key+' =')))
record={'utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'frozen_engine':'../baseline/bin/libjudas_engine.a','source':str(source),'source_sha256':hashlib.sha256(source.read_bytes()).hexdigest(),'commands':[],'results':[]}
(out/'PhysicsWorld.cpp').write_bytes(source.read_bytes())
command=['c++',*args,'-c',str(source),'-o',str(out/'PhysicsWorld.cpp.o')];start=time.monotonic()
with (out/'build.log').open('w') as f:r=subprocess.run(command,stdout=f,stderr=subprocess.STDOUT)
record['commands'].append(command);record['build_seconds']=time.monotonic()-start
if r.returncode:(out/'RESULTS.json').write_text(json.dumps(record,indent=2)+'\n');raise SystemExit(r.returncode)
link=shlex.split((root/'build/CMakeFiles/judas_impact_mesh_cost_tests.dir/link.txt').read_text());objects=[str(root/'build'/x) for x in link if x.endswith('.o')];libs=link[link.index('libjudas_engine.a')+1:];libs=[str(root/'build'/x) if x.endswith('.a') and not x.startswith('/') else x for x in libs]
command=['c++','-O3','-DNDEBUG',*objects,str(out/'PhysicsWorld.cpp.o'),'-o',str(out/'mesh'),str(root/'.cache/prealpha-pass3/baseline/bin/libjudas_engine.a'),*libs]
with (out/'link.log').open('w') as f:r=subprocess.run(command,stdout=f,stderr=subprocess.STDOUT)
record['commands'].append(command)
if r.returncode:(out/'RESULTS.json').write_text(json.dumps(record,indent=2)+'\n');raise SystemExit(r.returncode)
record['binary_sha256']=hashlib.sha256((out/'mesh').read_bytes()).hexdigest()
for label,argv in [('continuation',['1','trough']),('entry',['1','trough','1.2','entry']),('ragdoll60',['60','trough'])]:
 command=[str(out/'mesh'),*argv];start=time.monotonic()
 try:
  with (out/(label+'.log')).open('w') as f:r=subprocess.run(command,stdout=f,stderr=subprocess.STDOUT,timeout=90)
  code=r.returncode
 except subprocess.TimeoutExpired:code='timeout90'
 record['results'].append({'label':label,'command':command,'returncode':code,'seconds':time.monotonic()-start});(out/'RESULTS.json').write_text(json.dumps(record,indent=2)+'\n')
 if code=='timeout90':break
print(json.dumps(record['results'],indent=2))
