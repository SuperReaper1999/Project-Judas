#!/usr/bin/env python3
"""Observer-only benchmark relink. Supply exact frozen baseline/candidate archive."""
import argparse,datetime,hashlib,json,shlex,subprocess,tarfile
from pathlib import Path

OWNED=Path(__file__).resolve().parent
ROOT=OWNED.parents[2]
BUILD=ROOT/'build'
BASELINE=OWNED.parent/'baseline-e30'
p=argparse.ArgumentParser();p.add_argument('--archive',type=Path,required=True);p.add_argument('--name',required=True);p.add_argument('--reuse-driver',type=Path);a=p.parse_args()
out=OWNED/a.name;out.mkdir(exist_ok=False)
def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()
receipt={'utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'archive':str(a.archive.resolve()),'archive_sha256':sha(a.archive),'commands':[],'files':{}}
def run(argv,cwd=ROOT):receipt['commands'].append({'argv':list(map(str,argv)),'cwd':str(cwd)});subprocess.run(argv,cwd=cwd,check=True)
with tarfile.open(BASELINE/'source-inputs.tar.gz') as tar:source=tar.extractfile('tests/PhysicsStressLiveBenchmark.cpp').read().decode()
source=source.replace('int main(int argc,char** argv){','#include "TemporalObserver.h"\n\nint main(int argc,char** argv){',1)
source=source.replace(' ApplicationControl control;', ' temporal_quality::TemporalObserver temporal(output);\n ApplicationControl control;',1)
source=source.replace('started=true;\n   play.SetFixedStepObserver',
    'started=true;temporal.Observe(world,0);\n   play.SetFixedStepObserver',1)
old="<<d.nonfinite<<'\\n';steps.flush();});"
new="<<d.nonfinite<<'\\n';steps.flush();temporal.Observe(world,step);});"
if old not in source:raise SystemExit('observer insertion point absent')
source=source.replace(old,new,1)
driver=out/'PhysicsStressLiveBenchmark.observer.cpp';driver.write_text(source)
flags={}
for line in (BUILD/'CMakeFiles/judas_physics_stress_live_benchmark.dir/flags.make').read_text().splitlines():
    if ' = ' in line:key,value=line.split(' = ',1);flags[key]=shlex.split(value)
obj=out/'PhysicsStressLiveBenchmark.cpp.o'
if a.reuse_driver:
    import shutil
    previous=a.reuse_driver.resolve()
    if (previous/'PhysicsStressLiveBenchmark.observer.cpp').read_text()!=source:raise SystemExit('reused driver source differs')
    original=previous/'PhysicsStressLiveBenchmark.cpp.o'
    if any((OWNED/header).stat().st_mtime_ns>original.stat().st_mtime_ns for header in ['TemporalObserver.h','FloorOracle.h']):raise SystemExit('observer header newer than reusable driver object')
    shutil.copy2(original,obj);receipt['reused_driver_object']={'path':str(original),'sha256':sha(original),'source_sha256':sha(driver)}
else:
    run(['/usr/bin/c++',*flags['CXX_DEFINES'],*flags['CXX_INCLUDES'],*flags['CXX_FLAGS'],'-I'+str(OWNED),'-c',str(driver),'-o',str(obj)])
link=shlex.split((BUILD/'CMakeFiles/judas_physics_stress_live_benchmark.dir/link.txt').read_text())
link=[x for x in link if not x.startswith('-Wl,--dependency-file=')]
link[link.index('-o')+1]=str(out/'judas_physics_stress_live_benchmark')
link=[str(a.archive.resolve()) if x=='libjudas_engine.a' else str(obj) if x.endswith('/tests/PhysicsStressLiveBenchmark.cpp.o') else x for x in link]
run(link,BUILD)
# Link the small actual-native oracle controls using the same engine/link set.
unit=out/'OracleControls.cpp.o';run(['/usr/bin/c++',*flags['CXX_DEFINES'],*flags['CXX_INCLUDES'],*flags['CXX_FLAGS'],'-I'+str(OWNED),'-c',str(OWNED/'OracleControls.cpp'),'-o',str(unit)])
control=[x for x in link if x not in [str(obj)] and not x.startswith('CMakeFiles/judas_physics_stress_live_benchmark.dir/src/')]
control[control.index('-o')+1]=str(out/'oracle_controls');control.insert(control.index('-o'),str(unit));run(control,BUILD)
for path in [driver,obj,OWNED/'FloorOracle.h',OWNED/'TemporalObserver.h',OWNED/'OracleControls.cpp',out/'judas_physics_stress_live_benchmark',out/'oracle_controls']:
    receipt['files'][str(path)]={'sha256':sha(path),'bytes':path.stat().st_size}
receipt['finished_utc']=datetime.datetime.now(datetime.timezone.utc).isoformat();(out/'BUILD.json').write_text(json.dumps(receipt,indent=2)+'\n')
print(json.dumps({'binary':str(out/'judas_physics_stress_live_benchmark'),'controls':str(out/'oracle_controls')},indent=2))
