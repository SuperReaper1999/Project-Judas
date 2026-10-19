#!/usr/bin/env python3
"""Build one observer translation unit into a COPY of the frozen engine archive."""
import datetime
import hashlib
import json
from pathlib import Path
import shlex
import shutil
import subprocess
import tarfile

OWNED=Path(__file__).resolve().parent
ROOT=OWNED.parents[2]
BUILD=ROOT/'build'
FROZEN=OWNED.parent/'baseline-e30'

def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()
receipt={'utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'commands':[],
         'base_manifest_sha256':sha(FROZEN/'FROZEN.json'),'source_artifacts':{}}
def run(argv,cwd=ROOT):
    receipt['commands'].append({'argv':list(map(str,argv)),'cwd':str(cwd)})
    subprocess.run(argv,cwd=cwd,check=True)

with tarfile.open(FROZEN/'source-inputs.tar.gz') as tar:
    source=tar.extractfile('src/ImpactSolver.cpp').read().decode()
original=source
source=source.replace('} // namespace\n\nImpactResult',
    '} // namespace\n\n#include "ResidualObserver.h"\n\nImpactResult',1)
source=source.replace('    auto run = [&](bool safe) {',
    '    residual_observer::RowSummary residualBefore;\n    auto run = [&](bool safe) {',1)
source=source.replace('        m_solver.Prepare(0.0f);',
    '        m_solver.Prepare(0.0f);\n        if (residual_observer::Enabled() && !safe) residualBefore = residual_observer::ObserveRows(m_solver.Constraints());',1)
prefix,suffix=source.rsplit('    return result;\n}',1)
source=prefix+'    if (residual_observer::Enabled()) residual_observer::Write(residualBefore, m_solver.Constraints(), result, dt, joints && !joints->Empty());\n    return result;\n}'+suffix
if source.count('residual_observer::')!=5:raise SystemExit('unexpected observer insertions')
path=OWNED/'ImpactSolver.observer.cpp'
path.write_text(source)
(OWNED/'ImpactSolver.original.cpp').write_text(original)
flags={}
for line in (BUILD/'CMakeFiles/judas_engine.dir/flags.make').read_text().splitlines():
    if ' = ' in line:
        key,value=line.split(' = ',1);flags[key]=shlex.split(value)
obj=OWNED/'ImpactSolver.cpp.o'
run(['/usr/bin/c++',*flags['CXX_DEFINES'],*flags['CXX_INCLUDES'],*flags['CXX_FLAGS'],
     '-I'+str(OWNED),'-MMD','-MF',str(OWNED/'observer.d'),'-c',str(path),'-o',str(obj)])
archive=OWNED/'libjudas_engine.a'
shutil.copy2(FROZEN/'libjudas_engine.a',archive)
run(['ar','r',str(archive),str(obj)])
link=shlex.split((BUILD/'CMakeFiles/judas_physics_stress_live_benchmark.dir/link.txt').read_text())
link=[x for x in link if not x.startswith('-Wl,--dependency-file=')]
link[link.index('-o')+1]=str(OWNED/'judas_physics_stress_live_benchmark')
link=[str(archive) if x=='libjudas_engine.a' else x for x in link]
run(link,BUILD)
for file in [path,OWNED/'ImpactSolver.original.cpp',OWNED/'ResidualObserver.h',obj,archive,
             OWNED/'judas_physics_stress_live_benchmark']:
    receipt['source_artifacts'][str(file.relative_to(OWNED))]={'sha256':sha(file),'bytes':file.stat().st_size}
receipt['finished_utc']=datetime.datetime.now(datetime.timezone.utc).isoformat()
(OWNED/'BUILD.json').write_text(json.dumps(receipt,indent=2)+'\n')
print(json.dumps({'binary':str(OWNED/'judas_physics_stress_live_benchmark'),
                  'sha256':sha(OWNED/'judas_physics_stress_live_benchmark')},indent=2))
