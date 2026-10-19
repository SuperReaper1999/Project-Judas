#!/usr/bin/env python3
from pathlib import Path
import datetime,hashlib,json,shlex,subprocess
owned=Path(__file__).resolve().parent;root=owned.parents[2];build=root/'build'
base=owned.parent/'candidate-fastpaths-final/libjudas_engine.a'
flags={}
for line in (build/'CMakeFiles/judas_engine.dir/flags.make').read_text().splitlines():
 if ' = 'in line:
  k,v=line.split(' = ',1);flags[k]=shlex.split(v)
obj=owned/'SupportReplay.cpp.o'
compile=['/usr/bin/c++',*flags['CXX_DEFINES'],'-I'+str(root),'-I'+str(root/'src/physics'),*flags['CXX_INCLUDES'],*flags['CXX_FLAGS'],'-MMD','-MF',str(obj)+'.d','-c',str(owned/'SupportReplay.cpp'),'-o',str(obj)]
with(owned/'compile.log').open('w')as log:result=subprocess.run(compile,cwd=root,stdout=log,stderr=subprocess.STDOUT)
if result.returncode:print((owned/'compile.log').read_text());raise SystemExit(result.returncode)
link=shlex.split((build/'CMakeFiles/judas_impact_solver_tests.dir/link.txt').read_text())
link=[x for x in link if not x.startswith('-Wl,--dependency-file=')and not x.endswith('tests/ValidationMain.cpp.o')]
link=[str(obj)if x.endswith('tests/ImpactSolverTests.cpp.o')else str(base)if x=='libjudas_engine.a'else x for x in link]
binary=owned/'support-replay';link[link.index('-o')+1]=str(binary)
with(owned/'link.log').open('w')as log:result=subprocess.run(link,cwd=build,stdout=log,stderr=subprocess.STDOUT)
if result.returncode:print((owned/'link.log').read_text());raise SystemExit(result.returncode)
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
deps=shlex.split(Path(str(obj)+'.d').read_text().replace('\\\n',' ').split(':',1)[1]);inputs={x:sha(Path(x))for x in deps if Path(x).is_file()}
record={'compile':compile,'link':link,'inputs':inputs,'original_support_source_sha256':sha(root/'src/physics/WorldCCDSearch.cpp'),'base_archive_sha256':sha(base),'binary_sha256':sha(binary),'utc':datetime.datetime.now(datetime.timezone.utc).isoformat()}
(owned/'BUILD.json').write_text(json.dumps(record,indent=2)+'\n');print(binary)
