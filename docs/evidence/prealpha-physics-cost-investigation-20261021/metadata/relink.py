from pathlib import Path
import shlex,subprocess,json,hashlib
r=Path.cwd();w=r/'.cache/physics-exhaustive-profile-20261021';b=r/'build';dst=w/'buildsrc'
assert hashlib.sha256((b/'libjudas_engine.a').read_bytes()).hexdigest() == '05f626d3fb5400bfd1db23ec37a3cd3e6490f8452145365662466eebccd705ec', 'Original engine archive drifted from the recorded f5624f0b baseline'
original=shlex.split((b/'CMakeFiles/judas_engine.dir/link.txt').read_text().splitlines()[0]);objects=original[3:]
h=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
normal=w/'reconstructed-original.a'
if normal.exists():normal.unlink()
subprocess.run(['/usr/bin/ar','qc',str(normal),*objects],cwd=b,check=True);subprocess.run(['ranlib',str(normal)],check=True)
assert h(normal)==h(b/'libjudas_engine.a'),(h(normal),h(b/'libjudas_engine.a'))
override=[]
for o in objects:
 n=Path(o).name
 change=('/src/physics/' in o or n in ['PhysicsWorld.cpp.o','ImpactSolver.cpp.o','ContactSolver.cpp.o'])
 override.append(str(dst/n) if change else o)
core=w/'libjudas_engine_observed.a';core.unlink(missing_ok=True)
subprocess.run(['/usr/bin/ar','qc',str(core),*override],cwd=b,check=True);subprocess.run(['ranlib',str(core)],check=True)
link=shlex.split((b/'CMakeFiles/judas_physics_stress_live_benchmark.dir/link.txt').read_text());link=[x for x in link if not x.startswith('-Wl,--dependency-file=')];link[link.index('-o')+1]=str(w/'observed-live');link=[str(core)if x=='libjudas_engine.a'else str(dst/'ObservedLive.cpp.o')if x.endswith('/tests/PhysicsStressLiveBenchmark.cpp.o')else x for x in link];subprocess.run(link,cwd=b,check=True)
(w/'BUILD-overlay-final.json').write_text(json.dumps({'reconstructed_original_identical':True,'original_core_sha256':h(normal),'core_sha256':h(core),'binary_sha256':h(w/'observed-live'),'objects':override,'link':link},indent=2)+'\n')
print('Verified original archive reconstruction exact; overlay linked.')
