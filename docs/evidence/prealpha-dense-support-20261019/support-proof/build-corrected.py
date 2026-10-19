#!/usr/bin/env python3
from pathlib import Path
import datetime,hashlib,json,shlex,subprocess
owned=Path(__file__).resolve().parent;root=owned.parents[2];build=root/'build';corrected=owned/'corrected'
base=owned.parent/'candidate-fastpaths-final/libjudas_engine.a';sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
flags={}
for line in(build/'CMakeFiles/judas_engine.dir/flags.make').read_text().splitlines():
 if ' = 'in line:
  k,v=line.split(' = ',1);flags[k]=shlex.split(v)
records=[]
def compile(source,obj):
 cmd=['/usr/bin/c++',*flags['CXX_DEFINES'],'-I'+str(root),'-I'+str(root/'src/physics'),*flags['CXX_INCLUDES'],*flags['CXX_FLAGS'],'-MMD','-MF',str(obj)+'.d','-c',str(source),'-o',str(obj)]
 with Path(str(obj)+'.log').open('w')as log:r=subprocess.run(cmd,cwd=root,stdout=log,stderr=subprocess.STDOUT)
 records.append({'argv':cmd,'exit_code':r.returncode,'source_sha256':sha(source),'object_sha256':sha(obj)if obj.exists()else None})
 if r.returncode:print(Path(str(obj)+'.log').read_text());raise SystemExit(r.returncode)
 return obj
world=compile(corrected/'WorldCCDSearch.cpp',corrected/'WorldCCDSearch.cpp.o')
replay=compile(corrected/'SupportReplay.cpp',corrected/'SupportReplay.cpp.o')
fixture=compile(owned/'SupportCompletenessTests.cpp',owned/'SupportCompletenessTests.cpp.o')
def link(obj,name,override):
 cmd=shlex.split((build/'CMakeFiles/judas_impact_solver_tests.dir/link.txt').read_text());cmd=[x for x in cmd if not x.startswith('-Wl,--dependency-file=')and not x.endswith('tests/ValidationMain.cpp.o')]
 cmd=[str(obj)if x.endswith('tests/ImpactSolverTests.cpp.o')else str(base)if x=='libjudas_engine.a'else x for x in cmd]
 if override:cmd.insert(cmd.index(str(base)),str(world))
 binary=owned/name;cmd[cmd.index('-o')+1]=str(binary)
 with Path(str(binary)+'.link.log').open('w')as log:r=subprocess.run(cmd,cwd=build,stdout=log,stderr=subprocess.STDOUT)
 records.append({'argv':cmd,'exit_code':r.returncode,'binary_sha256':sha(binary)if binary.exists()else None})
 if r.returncode:print(Path(str(binary)+'.link.log').read_text());raise SystemExit(r.returncode)
 return binary
replaybin=link(replay,'corrected/support-replay',True)
originaltests=link(fixture,'support-completeness-original',False)
correctedtests=link(fixture,'support-completeness-corrected',True)
results=[]
for cmd,name in [([str(replaybin),str(owned.parent/'event-topology-observer-v2/pair-trace.jsonl'),str(corrected/'RESULTS.json')],'corrected/replay.log'),([str(originaltests),'--expect-original-failure'],'original-completeness.log'),([str(correctedtests)],'corrected-completeness.log')]:
 with(owned/name).open('w')as log:r=subprocess.run(cmd,stdout=log,stderr=subprocess.STDOUT)
 results.append({'argv':cmd,'exit_code':r.returncode,'output':(owned/name).read_text()});print(name,r.returncode,(owned/name).read_text()[-1800:])
(owned/'CORRECTED_BUILD_RESULTS.json').write_text(json.dumps({'utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'original_world_source_sha256':sha(root/'src/physics/WorldCCDSearch.cpp'),'corrected_world_source_sha256':sha(corrected/'WorldCCDSearch.cpp'),'base_archive_sha256':sha(base),'build':records,'results':results},indent=2)+'\n')
