#!/usr/bin/env python3
"""Build only affected Release TUs and relink unchanged focused fixtures.

The previous combined archive is frozen; a new owned archive receives exactly
three replacement members. Public declarations add no class fields/layouts.
No full production suite, fixture edit, staging or historical overwrite.
"""
from pathlib import Path
import hashlib,json,os,shlex,shutil,subprocess,time
r=Path.cwd();base=r/'.cache/m78-followup/release';out=r/'.cache/m78-cold-boundary/final-combined-build';ev=r/'docs/evidence/m78-cold-boundary/final-combined'
out.mkdir(parents=True,exist_ok=False);ev.mkdir(parents=True,exist_ok=False)
h=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
assert h(base/'libjudas_engine.a')=='577943674b2f319acbe956393f55a94ab6bb9de48a9f33dc4b4471d4775f8100'
flags={}
for line in(base/'CMakeFiles/judas_engine.dir/flags.make').read_text().splitlines():
 if ' = 'in line:k,v=line.split(' = ',1);flags[k]=shlex.split(v)
rows=[]
def run(cmd,name,cwd=r):
 start=time.perf_counter();p=subprocess.run(cmd,cwd=cwd,text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT)
 log=ev/(name+'.log');log.write_text(p.stdout);rows.append(dict(name=name,command=cmd,exitCode=p.returncode,seconds=time.perf_counter()-start,logSHA256=h(log)))
 (ev/'BUILD.json').write_text(json.dumps(dict(scope=__doc__,steps=rows),indent=2)+'\n');print(name,p.returncode,flush=True)
 if p.returncode:raise SystemExit('Build failed; preserved '+str(log))
for name in ['PhysicsWorld','WorldPersistence','PhysicalBalance']:
 run(['c++',*flags['CXX_DEFINES'],*flags['CXX_INCLUDES'],*flags['CXX_FLAGS'],'-c',str(r/'src'/(name+'.cpp')),'-o',str(out/(name+'.cpp.o'))],name+'-compile')
archive=out/'libjudas_engine.a';shutil.copyfile(base/'libjudas_engine.a',archive)
run(['ar','r',str(archive),*[str(out/(n+'.cpp.o'))for n in ['PhysicsWorld','WorldPersistence','PhysicalBalance']]],'archive')
targets=['judas_physical_balance_controller_tests','judas_physical_balance_foundation_tests','judas_rigid_motion_tests','judas_physical_animation_tests','judas_joint_tests','judas_ragdoll_tests','judas_contact_lifecycle_tests','judas_impact_timing_tests','judas_gravity_selection_tests','judas_world_persistence_tests','judas_prefab_tests','judas_streaming_tests','judas_balance_lab_tests','judas_character_lab_tests','judas']
linked={}
for target in targets:
 recipe=base/('CMakeFiles/'+target+'.dir/link.txt')
 if not recipe.exists():raise SystemExit('Missing exact recipe '+str(recipe))
 cmd=shlex.split(recipe.read_text());cmd=[x for x in cmd if not x.startswith('-Wl,--dependency-file=')]
 cmd[cmd.index('libjudas_engine.a')]=str(archive);exe=out/target;cmd[cmd.index('-o')+1]=str(exe)
 run(cmd,target+'-link',base);linked[target]=dict(path=str(exe.relative_to(r)),sha256=h(exe))
obj=out/'PhysicsContinuationTests.cpp.o';run(['c++',*flags['CXX_DEFINES'],*flags['CXX_INCLUDES'],*flags['CXX_FLAGS'],'-c',str(r/'tests/PhysicsContinuationTests.cpp'),'-o',str(obj)],'focused-test-compile')
cmd=shlex.split((base/'CMakeFiles/judas_world_persistence_tests.dir/link.txt').read_text());cmd=[x for x in cmd if not x.startswith('-Wl,--dependency-file=')]
cmd[cmd.index('CMakeFiles/judas_world_persistence_tests.dir/tests/WorldPersistenceTests.cpp.o')]=str(obj);cmd[cmd.index('libjudas_engine.a')]=str(archive);exe=out/'judas_physics_continuation_tests';cmd[cmd.index('-o')+1]=str(exe);run(cmd,'focused-test-link',base);linked[exe.name]=dict(path=str(exe.relative_to(r)),sha256=h(exe))
identity=dict(scope=__doc__,archive=dict(path=str(archive.relative_to(r)),sha256=h(archive)),sourceSHA256={str(p.relative_to(r)):h(p)for p in [r/'src/PhysicsWorld.cpp',r/'src/PhysicsWorld.h',r/'src/WorldPersistence.cpp',r/'src/WorldPersistence.h',r/'src/PhysicalBalance.cpp',r/'tests/PhysicsContinuationTests.cpp']},unchangedControllerSHA256=h(r/'tests/PhysicalBalanceControllerTests.cpp'),unchangedApplicationSHA256=h(r/'tests/BalanceLabTests.cpp'),linked=linked,steps=rows)
(ev/'BUILD.json').write_text(json.dumps(identity,indent=2)+'\n')
