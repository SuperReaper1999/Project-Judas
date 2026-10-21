from pathlib import Path
import subprocess,shlex,json,hashlib,datetime
root=Path('/home/conner/Documents/GitHub/Project-Judas');w=Path(__file__).resolve().parent;b=root/'build';flags={};commands=[]
for l in (b/'CMakeFiles/judas_physics_stress_live_benchmark.dir/flags.make').read_text().splitlines():
 if ' = 'in l:
  k,v=l.split(' = ',1);flags[k]=shlex.split(v)
def run(c,cwd=root):
 commands.append(list(map(str,c)));subprocess.run(list(map(str,c)),cwd=cwd,check=True)
run(['/usr/bin/c++',*flags['CXX_DEFINES'],*flags['CXX_INCLUDES'],*flags['CXX_FLAGS'],'-I'+str(w),'-c',w/'PhysicsStressLiveBenchmark.observer.cpp','-o',w/'PhysicsStressLiveBenchmark.cpp.o'])
for name,core in [('baseline',root/'.cache/event-architecture-20261021/final-snapshot/libjudas_engine.a'),('candidate',root/'.cache/contact-redesign-20261021/world-v8/libjudas_engine.a')]:
 link=shlex.split((b/'CMakeFiles/judas_physics_stress_live_benchmark.dir/link.txt').read_text());link=[x for x in link if not x.startswith('-Wl,--dependency-file=')];link[link.index('-o')+1]=str(w/('observer-'+name));link=[str(core)if x=='libjudas_engine.a'else str(w/'PhysicsStressLiveBenchmark.cpp.o')if x.endswith('/tests/PhysicsStressLiveBenchmark.cpp.o')else x for x in link];run(link,b)
(w/'BUILD.json').write_text(json.dumps({'utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'commands':commands,'files':{n:hashlib.sha256((w/n).read_bytes()).hexdigest()for n in ['PhysicsStressLiveBenchmark.observer.cpp','TemporalObserver.h','FloorOracle.h','PhysicsStressLiveBenchmark.cpp.o','observer-baseline','observer-candidate']}},indent=2)+'\n')
