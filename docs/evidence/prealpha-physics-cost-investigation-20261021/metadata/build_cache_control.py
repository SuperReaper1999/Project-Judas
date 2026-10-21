from pathlib import Path
import subprocess,shlex,shutil,json,hashlib
r=Path.cwd();b=r/'build';w=r/'.cache/physics-exhaustive-profile-20261021';d=w/'response/cache-control';f={};commands=[]
for l in (b/'CMakeFiles/judas_engine.dir/flags.make').read_text().splitlines():
 if ' = 'in l:
  k,v=l.split(' = ',1);f[k]=shlex.split(v)
assert hashlib.sha256((b/'libjudas_engine.a').read_bytes()).hexdigest() == '05f626d3fb5400bfd1db23ec37a3cd3e6490f8452145365662466eebccd705ec', 'Original engine archive drifted from the recorded f5624f0b baseline'
def run(c,cwd=r):commands.append(list(map(str,c)));subprocess.run(list(map(str,c)),cwd=cwd,check=True)
for n in ['WorldIncrementalImpact','WorldImpactEvents']:
 run(['/usr/bin/c++',*f['CXX_DEFINES'],'-I'+str(d),'-I'+str(r/'src/physics'),*f['CXX_INCLUDES'],*f['CXX_FLAGS'],'-c',d/(n+'.cpp'),'-o',d/(n+'.cpp.o')])
core=d/'libjudas_engine.a';shutil.copy2(b/'libjudas_engine.a',core);run(['ar','r',core,d/'WorldIncrementalImpact.cpp.o',d/'WorldImpactEvents.cpp.o'])
link=shlex.split((b/'CMakeFiles/judas_physics_stress_live_benchmark.dir/link.txt').read_text());link=[x for x in link if not x.startswith('-Wl,--dependency-file=')];link[link.index('-o')+1]=str(w/'cache-control-live');link=[str(core)if x=='libjudas_engine.a' else str(w/'buildsrc/SampledLive.cpp.o')if x.endswith('/tests/PhysicsStressLiveBenchmark.cpp.o')else x for x in link];run(link,b)
(w/'BUILD-cache-control.json').write_text(json.dumps({'commands':commands,'core_sha256':hashlib.sha256(core.read_bytes()).hexdigest(),'binary_sha256':hashlib.sha256((w/'cache-control-live').read_bytes()).hexdigest()},indent=2)+'\n')
