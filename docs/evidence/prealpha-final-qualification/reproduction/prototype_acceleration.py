from pathlib import Path
import subprocess,shlex,json,hashlib
r=Path.cwd();o=r/'.cache/prealpha-final-qualification/acceleration-prototype';o.mkdir(exist_ok=False)
src=r/'src/physics/WorldMk2Solver.cpp';s=src.read_text();old='''    for (auto &s : state) s.constrained |= s.dynamic;'''
new='''    for (auto &s : state) {
        // The public acceleration/force kick is already applied before Step.
        // Only actual constraint/coupling solves distribute it over substeps.
        // Free flights still share the chronological CCD intervals, without
        // moving a pre-step impulse to a different time from its counter-impulse.
        if (s.dynamic && !s.constrained && !coupling) {
            s.rb->linearVelocity += s.gravityStep * float(substeps);
            s.gravityStep = glm::vec3(0);
        }
        s.constrained |= s.dynamic;
    }'''
assert s.count(old)==1;s=s.replace(old,new);(o/'WorldMk2Solver.cpp').write_text(s)
flags={}
for line in (r/'build/CMakeFiles/judas_engine.dir/flags.make').read_text().splitlines():
 if line.startswith(('CXX_FLAGS =','CXX_DEFINES =','CXX_INCLUDES =')):
  k,v=line.split('=',1);flags[k.strip()]=shlex.split(v)
cmd=['c++']+flags['CXX_DEFINES']+flags['CXX_INCLUDES']+flags['CXX_FLAGS']+['-I'+str(r/'src/physics'),'-c',str(o/'WorldMk2Solver.cpp'),'-o',str(o/'WorldMk2Solver.o')]
with (o/'build.log').open('w') as f:subprocess.run(cmd,stdout=f,stderr=subprocess.STDOUT,check=True)
args=shlex.split((r/'build/CMakeFiles/judas_mk2_diagnostics_tests.dir/link.txt').read_text());args=[a for a in args if not a.startswith('-Wl,--dependency-file=') and '/tests/ValidationMain.cpp.o' not in a];args[next(i for i,a in enumerate(args) if '/tests/Mk2DiagnosticsTests.cpp.o' in a)]=str(r/'.cache/prealpha-final-qualification/wheel-current.o');args.insert(args.index('libjudas_engine.a'),str(o/'WorldMk2Solver.o'));args[args.index('-o')+1]=str(o/'wheel');subprocess.run(args,cwd=r/'build',check=True)
with (o/'wheel.log').open('w') as f:code=subprocess.run([str(o/'wheel')],stdout=f,stderr=subprocess.STDOUT).returncode
(o/'PROVENANCE.json').write_text(json.dumps(dict(exit=code,original_sha256=hashlib.sha256(src.read_bytes()).hexdigest(),modified_sha256=hashlib.sha256(s.encode()).hexdigest(),compile=cmd,link=args),indent=2)+'\n');print('Prototype wheel result',code)
