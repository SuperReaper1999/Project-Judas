from pathlib import Path
import subprocess,shlex,json,os,hashlib
r=Path.cwd();b=r/'build';w=r/'.cache/contact-redesign-20261021/sanitize-final';w.mkdir(exist_ok=True);flags={};cmds=[]
for l in(b/'CMakeFiles/judas_engine.dir/flags.make').read_text().splitlines():
 if' = 'in l:
  k,v=l.split(' = ',1);flags[k]=shlex.split(v)
extra=['-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer'];objects=[]
with(w/'build.log').open('w')as log:
 for file in['src/ContactSolver.cpp','src/ImpactSolver.cpp','src/RigidBody.cpp']:
  obj=w/(Path(file).stem+'.o');c=['c++',*flags['CXX_DEFINES'],*flags['CXX_INCLUDES'],*flags['CXX_FLAGS'],*extra,'-c',str(r/file),'-o',str(obj)];cmds.append(c);subprocess.run(c,check=True,stdout=log,stderr=subprocess.STDOUT);objects.append(str(obj))
 for name,test in[('growing','GrowingImpactTests'),('rows','TrackedContactRowsTests')]:
  obj=w/(test+'.o');c=['c++',*flags['CXX_DEFINES'],*flags['CXX_INCLUDES'],*flags['CXX_FLAGS'],*extra,'-c',str(r/'tests'/(test+'.cpp')),'-o',str(obj)];cmds.append(c);subprocess.run(c,check=True,stdout=log,stderr=subprocess.STDOUT)
  link=shlex.split((b/'CMakeFiles/judas_impact_solver_tests.dir/link.txt').read_text());link=[x for x in link if not x.startswith('CMakeFiles/')and not x.startswith('-Wl,--dependency-file=')];link[1:1]=[str(obj),*objects,*extra];link[link.index('-o')+1]=str(w/name);cmds.append(link);subprocess.run(link,cwd=b,check=True,stdout=log,stderr=subprocess.STDOUT)
results=[]
for name in['growing','rows']:
 env=os.environ.copy();env['ASAN_OPTIONS']='detect_leaks=0:halt_on_error=1';env['UBSAN_OPTIONS']='halt_on_error=1:print_stacktrace=1'
 with(w/(name+'.log')).open('w')as log:code=subprocess.run([w/name],env=env,stdout=log,stderr=subprocess.STDOUT).returncode
 results.append({'name':name,'exit':code,'sha256':hashlib.sha256((w/name).read_bytes()).hexdigest()})
(w/'RESULTS.json').write_text(json.dumps({'scope':'Tests plus ContactSolver, ImpactSolver and RigidBody instrumented; other archive modules are Release, not a whole-engine sanitizer qualification. Leak detection disabled: restricted runner ptrace makes LSan unavailable; original failure retained.','results':results},indent=2)+'\n');(w/'COMMANDS.json').write_text(json.dumps(cmds,indent=2)+'\n');raise SystemExit(int(any(x['exit']for x in results)))
