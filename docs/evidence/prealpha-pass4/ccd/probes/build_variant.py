from pathlib import Path
import datetime,hashlib,json,shlex,subprocess,sys,time
root=Path.cwd();out=root/'.cache/prealpha-pass4/ccd'/sys.argv[1]; flags=(root/'build/CMakeFiles/judas_engine.dir/flags.make').read_text().splitlines();args=[]
for key in ['CXX_DEFINES','CXX_INCLUDES','CXX_FLAGS']:args+=shlex.split(next(x.split('=',1)[1] for x in flags if x.startswith(key+' =')))
rec={'utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'sources':{},'commands':[],'results':[]}
for name in []:
 source=out/'sources'/name;rec['sources'][name]=hashlib.sha256(source.read_bytes()).hexdigest();cmd=['c++',*args,'-I',str(root/'src'),'-c',str(source),'-o',str(out/(name+'.o'))];rec['commands'].append(cmd)
 with (out/(name+'.build.log')).open('w') as f:r=subprocess.run(cmd,stdout=f,stderr=subprocess.STDOUT)
 if r.returncode:(out/'RESULTS.json').write_text(json.dumps(rec,indent=2));raise SystemExit(r.returncode)
link=shlex.split((root/'build/CMakeFiles/judas_impact_mesh_cost_tests.dir/link.txt').read_text());objects=[str(root/'build'/x) for x in link if x.endswith('.o')];libs=link[link.index('libjudas_engine.a')+1:];libs=[str(root/'build'/x) if x.endswith('.a') and not x.startswith('/') else x for x in libs]
lib=root/'.cache/prealpha-pass4/ccd/accepted-engine.a'
if not lib.exists():raise SystemExit('baseline not frozen yet')
cmd=['c++','-O3','-DNDEBUG',*objects,str(out/'PhysicsWorld.cpp.o'),str(out/'CollisionGeometry.cpp.o'),'-o',str(out/'mesh'),str(lib),*libs];rec['commands'].append(cmd)
with (out/'link.log').open('w') as f:r=subprocess.run(cmd,stdout=f,stderr=subprocess.STDOUT)
if r.returncode:(out/'RESULTS.json').write_text(json.dumps(rec,indent=2));raise SystemExit(r.returncode)
for label,argv in [('continuation',['1','trough']),('entry',['1','trough','1.2','entry'])]:
 cmd=[str(out/'mesh'),*argv];start=time.monotonic()
 try:
  with (out/(label+'.log')).open('w') as f:r=subprocess.run(cmd,stdout=f,stderr=subprocess.STDOUT,timeout=90)
  code=r.returncode
 except subprocess.TimeoutExpired:code='timeout90'
 rec['results'].append({'label':label,'command':cmd,'code':code,'seconds':time.monotonic()-start});(out/'RESULTS.json').write_text(json.dumps(rec,indent=2)+'\n')
 if code=='timeout90':break
print(json.dumps(rec['results']))
