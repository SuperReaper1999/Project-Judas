from pathlib import Path
import subprocess,shlex,shutil,hashlib,json
r=Path.cwd();b=r/'build';w=Path(__file__).resolve().parent;f={};commands=[]
assert hashlib.sha256((b/'libjudas_engine.a').read_bytes()).hexdigest()=='05f626d3fb5400bfd1db23ec37a3cd3e6490f8452145365662466eebccd705ec'
for l in(b/'CMakeFiles/judas_engine.dir/flags.make').read_text().splitlines():
 if ' = 'in l:
  k,v=l.split(' = ',1);f[k]=shlex.split(v)
def run(c,cwd=r):commands.append(list(map(str,c)));subprocess.run(list(map(str,c)),cwd=cwd,check=True)
run(['/usr/bin/c++',*f['CXX_DEFINES'],'-I'+str(r/'src/physics'),*f['CXX_INCLUDES'],*f['CXX_FLAGS'],'-c',w/'WorldImpactEvents.cpp','-o',w/'WorldImpactEvents.cpp.o'])
core=w/'libjudas_engine.a';shutil.copy2(b/'libjudas_engine.a',core);run(['ar','r',core,w/'WorldImpactEvents.cpp.o'])
link=shlex.split((b/'CMakeFiles/judas_physics_stress_live_benchmark.dir/link.txt').read_text());link=[x for x in link if not x.startswith('-Wl,--dependency-file=')];link[link.index('-o')+1]=str(w/'live');link=[str(core)if x=='libjudas_engine.a'else x for x in link];run(link,b)
(w/'BUILD.json').write_text(json.dumps({'commands':commands,'binary_sha256':hashlib.sha256((w/'live').read_bytes()).hexdigest(),'core_sha256':hashlib.sha256(core.read_bytes()).hexdigest()},indent=2)+'\n')
