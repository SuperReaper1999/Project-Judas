#!/usr/bin/env python3
from pathlib import Path
import datetime,hashlib,json,shlex,subprocess
owned=Path(__file__).resolve().parent;root=owned.parents[2];build=root/'build'
baseline=owned/'libjudas_engine.a';flags={}
for line in (build/'CMakeFiles/judas_engine.dir/flags.make').read_text().splitlines():
    if ' = ' in line:
        key,value=line.split(' = ',1);flags[key]=shlex.split(value)
obj=owned/'PairReplay.cpp.o'
compile=['/usr/bin/c++',*flags['CXX_DEFINES'],'-I'+str(root),'-I'+str(owned.parent/'event-topology-observer-v2/include/src/physics'),
    '-I'+str(owned.parent/'event-topology-observer-v2/include/src'),'-I'+str(owned),*flags['CXX_INCLUDES'],*flags['CXX_FLAGS'],
    '-c',str(owned/'PairReplay.cpp'),'-o',str(obj)]
subprocess.run(compile,cwd=root,check=True)
link=shlex.split((build/'CMakeFiles/judas_impact_solver_tests.dir/link.txt').read_text())
link=[x for x in link if not x.startswith('-Wl,--dependency-file=') and not x.endswith('tests/ValidationMain.cpp.o')]
link=[str(obj) if x.endswith('tests/ImpactSolverTests.cpp.o') else x for x in link]
link=[str(baseline) if x=='libjudas_engine.a' else x for x in link]
binary=owned/'pair-replay';link[link.index('-o')+1]=str(binary)
subprocess.run(link,cwd=build,check=True)
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
(owned/'REPLAY_BUILD.json').write_text(json.dumps({'utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),
    'compile':compile,'link':link,'baseline_archive_sha256':sha(baseline),
    'source_sha256':sha(owned/'PairReplay.cpp'),'binary_sha256':sha(binary)},indent=2)+'\n')
print(binary)
