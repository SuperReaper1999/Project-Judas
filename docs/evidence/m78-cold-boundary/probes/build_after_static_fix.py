#!/usr/bin/env python3
"""Narrow WorldPersistence follow-up on the frozen combined Release archive."""
from pathlib import Path
import hashlib,json,shlex,shutil,subprocess,time
r=Path.cwd();base=r/'.cache/m78-followup/release';previous=r/'.cache/m78-cold-boundary/final-combined-build';out=r/'.cache/m78-cold-boundary/final-after-static-build';ev=r/'docs/evidence/m78-cold-boundary/final-after-static'
out.mkdir(parents=True,exist_ok=False);ev.mkdir(parents=True,exist_ok=False)
h=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
old=json.loads((r/'docs/evidence/m78-cold-boundary/final-combined/BUILD.json').read_text());assert h(previous/'libjudas_engine.a')==old['archive']['sha256']
for p,v in old['sourceSHA256'].items():
 if p not in ['src/WorldPersistence.cpp','tests/PhysicsContinuationTests.cpp']:assert h(r/p)==v,p
flags={}
for line in(base/'CMakeFiles/judas_engine.dir/flags.make').read_text().splitlines():
 if ' = 'in line:k,v=line.split(' = ',1);flags[k]=shlex.split(v)
rows=[]
def run(cmd,name,cwd=r):
 start=time.perf_counter();p=subprocess.run(cmd,cwd=cwd,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True);log=ev/(name+'.log');log.write_text(p.stdout)
 rows.append(dict(name=name,command=cmd,exitCode=p.returncode,seconds=time.perf_counter()-start,logSHA256=h(log)));(ev/'BUILD.json').write_text(json.dumps(dict(scope=__doc__,steps=rows),indent=2)+'\n');print(name,p.returncode,flush=True)
 if p.returncode:raise SystemExit('Preserved build failure '+str(log))
obj=out/'WorldPersistence.cpp.o';run(['c++',*flags['CXX_DEFINES'],*flags['CXX_INCLUDES'],*flags['CXX_FLAGS'],'-c',str(r/'src/WorldPersistence.cpp'),'-o',str(obj)],'WP-compile')
archive=out/'libjudas_engine.a';shutil.copyfile(previous/'libjudas_engine.a',archive);run(['ar','r',str(archive),str(obj)],'archive')
targets=[name for name in old['linked'] if name!='judas_physics_continuation_tests']+['judas_editor','judas_navigation_dependency_tests']
linked={}
for target in targets:
 cmd=shlex.split((base/('CMakeFiles/'+target+'.dir/link.txt')).read_text());cmd=[str(archive)if x=='libjudas_engine.a'else x for x in cmd if not x.startswith('-Wl,--dependency-file=')];exe=out/target;cmd[cmd.index('-o')+1]=str(exe);run(cmd,target+'-link',base);linked[target]=dict(path=str(exe.relative_to(r)),sha256=h(exe))
test=out/'PhysicsContinuationTests.cpp.o';run(['c++',*flags['CXX_DEFINES'],*flags['CXX_INCLUDES'],*flags['CXX_FLAGS'],'-c',str(r/'tests/PhysicsContinuationTests.cpp'),'-o',str(test)],'focused-compile')
cmd=shlex.split((base/'CMakeFiles/judas_world_persistence_tests.dir/link.txt').read_text());cmd=[str(archive)if x=='libjudas_engine.a'else x for x in cmd if not x.startswith('-Wl,--dependency-file=')];cmd[cmd.index('CMakeFiles/judas_world_persistence_tests.dir/tests/WorldPersistenceTests.cpp.o')]=str(test);exe=out/'judas_physics_continuation_tests';cmd[cmd.index('-o')+1]=str(exe);run(cmd,'focused-link',base);linked[exe.name]=dict(path=str(exe.relative_to(r)),sha256=h(exe))
result=dict(scope=__doc__,priorBuildSHA256=h(r/'docs/evidence/m78-cold-boundary/final-combined/BUILD.json'),priorArchiveSHA256=old['archive']['sha256'],archive=dict(path=str(archive.relative_to(r)),sha256=h(archive)),sourceSHA256={p:h(r/p)for p in old['sourceSHA256']},unchangedControllerSHA256=h(r/'tests/PhysicalBalanceControllerTests.cpp'),unchangedApplicationSHA256=h(r/'tests/BalanceLabTests.cpp'),unchangedCharacterApplicationSHA256=h(r/'tests/CharacterLabTests.cpp'),linked=linked,steps=rows)
(ev/'BUILD.json').write_text(json.dumps(result,indent=2)+'\n')
