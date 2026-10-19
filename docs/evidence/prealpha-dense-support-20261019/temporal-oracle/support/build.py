#!/usr/bin/env python3
"""Float-layout support-only observer; never imports coherent-double headers."""
from pathlib import Path
import argparse,datetime,hashlib,json,shlex,shutil,subprocess
O=Path(__file__).resolve().parent;R=O.parents[2];B=R/'build';Q=O.parent/'quality-prototype';BASE=Q/'baseline-complete';P=argparse.ArgumentParser();P.add_argument('--archive',type=Path,required=True);a=P.parse_args();a.archive=a.archive.resolve();sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
receipt={'utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'archive':str(a.archive),'archive_sha256':sha(a.archive),'commands':[],'scope':'Same original temporal observer, original Float public/private layouts. No coherent-double prototype headers or objects. Observer runs after measured physics span; not FPS evidence.'}
def persist():(O/'BUILD.json').write_text(json.dumps(receipt,indent=2)+'\n')
def run(cmd,tag,cwd=R):
 p=subprocess.run(list(map(str,cmd)),cwd=cwd,text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT);(O/(tag+'.log')).write_text(p.stdout);receipt['commands'].append({'argv':list(map(str,cmd)),'cwd':str(cwd),'exit':p.returncode,'log':tag+'.log'});persist()
 if p.returncode:raise SystemExit(p.returncode)
for source,destination in [(BASE/'PhysicsStressLiveBenchmark.observer.cpp',O/'PhysicsStressLiveBenchmark.observer.cpp'),*( (Q/n,O/n) for n in ['TemporalObserver.h','FloorOracle.h','OracleControls.cpp'])]:
 assert not destination.exists();shutil.copy2(source,destination);assert sha(source)==sha(destination)
f={}
for line in (B/'CMakeFiles/judas_physics_stress_live_benchmark.dir/flags.make').read_text().splitlines():
 if ' = ' in line:k,v=line.split(' = ',1);f[k]=shlex.split(v)
common=['/usr/bin/c++',*f['CXX_DEFINES'],*f['CXX_INCLUDES'],*f['CXX_FLAGS'],'-I'+str(O)]
for stem in ['PhysicsStressLiveBenchmark.observer','OracleControls']:
 run([*common,'-MMD','-MF',O/(stem+'.o.d'),'-c',O/(stem+'.cpp'),'-o',O/(stem+'.o')],'compile-'+stem)
link=shlex.split((B/'CMakeFiles/judas_physics_stress_live_benchmark.dir/link.txt').read_text());link=[x for x in link if not x.startswith('-Wl,--dependency-file=')];link=[str(a.archive) if x=='libjudas_engine.a' else str(O/'PhysicsStressLiveBenchmark.observer.o') if x.endswith('/tests/PhysicsStressLiveBenchmark.cpp.o') else x for x in link];link[link.index('-o')+1]=str(O/'judas_physics_stress_live_benchmark');run(link,'link-live',B)
control=[x for x in link if x!=str(O/'PhysicsStressLiveBenchmark.observer.o') and not x.startswith('CMakeFiles/judas_physics_stress_live_benchmark.dir/src/')];control.insert(control.index('-o'),str(O/'OracleControls.o'));control[control.index('-o')+1]=str(O/'oracle_controls');run(control,'link-controls',B)
receipt['files']={str(p):{'sha256':sha(p),'bytes':p.stat().st_size} for p in [O/'PhysicsStressLiveBenchmark.observer.cpp',O/'TemporalObserver.h',O/'FloorOracle.h',O/'OracleControls.cpp',O/'PhysicsStressLiveBenchmark.observer.o',O/'judas_physics_stress_live_benchmark',O/'oracle_controls']}
receipt['compile_dependency_inputs']={}
for d in O.glob('*.o.d'):
 names=shlex.split(d.read_text().replace('\\\n',' ').split(':',1)[1]);assert not any('coherent-geometry' in n for n in names)
 for n in names:
  p=Path(n);p=p if p.is_absolute() else R/p
  receipt['compile_dependency_inputs'][str(p)]={'sha256':sha(p),'bytes':p.stat().st_size}
receipt['original_driver_sha256']=sha(BASE/'PhysicsStressLiveBenchmark.observer.cpp');assert sha(O/'PhysicsStressLiveBenchmark.observer.cpp')==receipt['original_driver_sha256'];receipt['application_helper_objects']={}
for n in link:
 if n.startswith('CMakeFiles/judas_physics_stress_live_benchmark.dir/src/'):
  p=B/n;receipt['application_helper_objects'][n]=sha(p)
receipt['finished_utc']=datetime.datetime.now(datetime.timezone.utc).isoformat();persist();print(O/'judas_physics_stress_live_benchmark')
