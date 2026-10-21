from pathlib import Path
import subprocess,shlex,json,hashlib
r=Path.cwd();b=r/'build';w=r/'.cache/physics-exhaustive-profile-20261021';d=w/'buildsrc';f={}
for l in (b/'CMakeFiles/judas_physics_stress_live_benchmark.dir/flags.make').read_text().splitlines():
 if ' = 'in l:
  k,v=l.split(' = ',1);f[k]=shlex.split(v)
assert hashlib.sha256((b/'libjudas_engine.a').read_bytes()).hexdigest() == '05f626d3fb5400bfd1db23ec37a3cd3e6490f8452145365662466eebccd705ec', 'Original engine archive drifted from the recorded f5624f0b baseline'
s=(d/'ObservedLive.cpp').read_text().replace('ccd_audit::SetEnabled(e);response_audit::SetEnabled(e);','ccd_audit::SetEnabled(false);response_audit::SetEnabled(false);');p=d/'StagesLive.cpp';p.write_text(s);o=d/'StagesLive.cpp.o'
c=['/usr/bin/c++',*f['CXX_DEFINES'],'-I'+str(d),*f['CXX_INCLUDES'],*f['CXX_FLAGS'],'-c',str(p),'-o',str(o)];subprocess.run(c,check=True)
link=shlex.split((b/'CMakeFiles/judas_physics_stress_live_benchmark.dir/link.txt').read_text());link=[x for x in link if not x.startswith('-Wl,--dependency-file=')];link[link.index('-o')+1]=str(w/'stages-live');link=[str(w/'libjudas_engine_observed.a')if x=='libjudas_engine.a' else str(o)if x.endswith('/tests/PhysicsStressLiveBenchmark.cpp.o')else x for x in link];subprocess.run(link,cwd=b,check=True)
(w/'BUILD-stages.json').write_text(json.dumps({'commands':[c,link],'core_sha256':hashlib.sha256((w/'libjudas_engine_observed.a').read_bytes()).hexdigest(),'binary_sha256':hashlib.sha256((w/'stages-live').read_bytes()).hexdigest()},indent=2)+'\n')
