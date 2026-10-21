from pathlib import Path
import subprocess,shlex,shutil,hashlib,json,datetime,time
root=Path('/home/conner/Documents/GitHub/Project-Judas');build=root/'build';work=Path(__file__).resolve().parent;out=work/'experiment'
out.mkdir(exist_ok=False)
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
expected='6b57d6c554c728a92afeb227581a863c41be9246cd2790aefe41afb3e047b4b7'
assert sha(build/'libjudas_engine.a')==expected
for p in ['ContactSolver.cpp','ImpactSolver.cpp']:
 assert (work/(Path(p).stem+'.original.cpp')).read_bytes()==(root/'src'/p).read_bytes(),p
flags={}
for line in (build/'CMakeFiles/judas_engine.dir/flags.make').read_text().splitlines():
 if ' = ' in line:
  k,v=line.split(' = ',1);flags[k]=shlex.split(v)
records=[]
def run(command,name,cwd=root,expected_exit=0):
 command=list(map(str,command));start=time.monotonic();utc=datetime.datetime.now(datetime.timezone.utc).isoformat()
 with (out/(name+'.log')).open('w') as log:
  p=subprocess.run(command,cwd=cwd,stdout=log,stderr=subprocess.STDOUT,timeout=120)
 row={'name':name,'argv':command,'cwd':str(cwd),'exit':p.returncode,'expected_exit':expected_exit,'seconds':time.monotonic()-start,'start_utc':utc}
 records.append(row);(out/'COMMANDS.json').write_text(json.dumps(records,indent=2)+'\n')
 if expected_exit=='nonzero':assert p.returncode!=0,name
 else:assert p.returncode==expected_exit,name
for p in ['ContactSolver.cpp','ImpactSolver.cpp']:
 run(['/usr/bin/c++',*flags['CXX_DEFINES'],'-I'+str(work),'-I'+str(root/'src/physics'),*flags['CXX_INCLUDES'],*flags['CXX_FLAGS'],'-c',work/p,'-o',out/(p+'.o')],p+'.compile')
core=out/'libjudas_engine.a';shutil.copy2(build/'libjudas_engine.a',core)
members=subprocess.check_output(['ar','t',core],text=True).splitlines()
for p in ['ContactSolver.cpp.o','ImpactSolver.cpp.o']:assert members.count(p)==1,(p,members.count(p))
run(['ar','r',core,out/'ContactSolver.cpp.o',out/'ImpactSolver.cpp.o'],'archive')
original=shlex.split((build/'CMakeFiles/judas_growing_impact_tests.dir/link.txt').read_text())
closure=[str(core) if x=='libjudas_engine.a' else x for x in original if x.endswith(('.a','.so')) or x.startswith('-l')]
for name,source in [('dependency',work/'RowDependencyTests.cpp'),('tracked300',work/'TrackedContactRowsDependencyTests.cpp'),('growing',root/'tests/GrowingImpactTests.cpp')]:
 obj=out/(name+'.o')
 run(['/usr/bin/c++',*flags['CXX_DEFINES'],'-I'+str(work),'-I'+str(root/'third_party'),*flags['CXX_INCLUDES'],*flags['CXX_FLAGS'],'-c',source,'-o',obj],name+'.compile')
 run(['/usr/bin/c++','-O3','-DNDEBUG',obj,'-o',out/name,*closure],name+'.link',build)
 run([out/name],name+'.run')
for name in ['endpoint','own','target']:
 run([out/'dependency','--broken-'+name],'negative-'+name,expected_exit='nonzero')
# Same existing ordinary app driver objects; only the two private solver objects differ.
link=shlex.split((build/'CMakeFiles/judas_physics_stress_live_benchmark.dir/link.txt').read_text())
link=[x for x in link if not x.startswith('-Wl,--dependency-file=')]
link[link.index('-o')+1]=str(out/'live');link=[str(core) if x=='libjudas_engine.a' else x for x in link]
run(link,'live.link',build)
report={'normal_core':str(build/'libjudas_engine.a'),'normal_core_sha256':expected,'private_core_sha256':sha(core),'binary_sha256':sha(out/'live'),'inputs':{str(p):sha(p) for p in [work/'RowDependency.h',work/'ContactSolver.cpp',work/'ImpactSolver.cpp',work/'RowDependencyTests.cpp',work/'TrackedContactRowsDependencyTests.cpp',root/'src/ContactSolver.cpp',root/'src/ImpactSolver.cpp',root/'src/physics/WorldImpactEvents.cpp']},'commands':records}
assert sha(build/'libjudas_engine.a')==expected
(out/'BUILD_AND_ORACLES.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps({'core_sha256':report['private_core_sha256'],'live_sha256':report['binary_sha256'],'checks':'all positive passed; three broken controls failed'},indent=2))
