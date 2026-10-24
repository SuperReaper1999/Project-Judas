from pathlib import Path
import subprocess,shlex,json,hashlib
r=Path.cwd();o=r/'.cache/prealpha-final-qualification';b=r/'build';base=r/'.cache/mk2-rotation-repair'
s=(r/'tests/WheelTests.cpp').read_text()
s=s.replace('Check(a.LocalPosition().y>.55f', 'std::printf("REST_VECTOR %.9g %.9g %.9g OMEGA %.9g %.9g %.9g\\n",a.world.GetLinearVelocity(a.chassis).x,a.world.GetLinearVelocity(a.chassis).y,a.world.GetLinearVelocity(a.chassis).z,a.world.GetAngularVelocity(a.chassis).x,a.world.GetAngularVelocity(a.chassis).y,a.world.GetAngularVelocity(a.chassis).z);\n    Check(a.LocalPosition().y>.55f')
s=s.replace('Check(uneven.wheels[1].state.contact', 'std::printf("UNEVEN contact %d lengths %.9g %.9g speed %.9g\\n",int(uneven.wheels[1].state.contact),uneven.wheels[1].state.length,uneven.wheels[0].state.length,glm::length(uneven.world.GetLinearVelocity(uneven.chassis)));\n    Check(uneven.wheels[1].state.contact')
probe=o/'wheel-observer.cpp';probe.write_text(s);rows=[]
for version in ('before','current'):
 output=o/('wheel-'+version);obj=Path(str(output)+'.o');include=base/'baseline-source/src' if version=='before' else r/'src'
 subprocess.run(['c++','-O3','-DNDEBUG','-std=gnu++17','-I'+str(include),'-c',str(probe),'-o',str(obj)],check=True)
 args=shlex.split((b/'CMakeFiles/judas_mk2_diagnostics_tests.dir/link.txt').read_text());args=[a for a in args if not a.startswith('-Wl,--dependency-file=') and '/tests/ValidationMain.cpp.o' not in a];args[next(i for i,a in enumerate(args) if '/tests/Mk2DiagnosticsTests.cpp.o' in a)]=str(obj);args[args.index('-o')+1]=str(output)
 if version=='before':args[args.index('libjudas_engine.a')]=str(base/'baseline-libs/libjudas_engine.a')
 subprocess.run(args,cwd=b,check=True)
 with (o/('wheel-'+version+'.log')).open('w') as f:code=subprocess.run([str(output)],stdout=f,stderr=subprocess.STDOUT).returncode
 rows.append({'version':version,'exit':code,'binary_sha256':hashlib.sha256(output.read_bytes()).hexdigest(),'source_sha256':hashlib.sha256(probe.read_bytes()).hexdigest(),'link':args})
(o/'WHEEL_COMPARISON.json').write_text(json.dumps(rows,indent=2)+'\n');print([(x['version'],x['exit']) for x in rows])
