#!/usr/bin/env python3
"""Isolated mixed-sphere/box reuse and matched same-object functional fixtures."""
from pathlib import Path
import datetime,hashlib,json,shlex,shutil,subprocess,time
r=Path.cwd();d=r/'.cache/prealpha-pass5/dense/reciprocal';out=d/'candidate';out.mkdir(exist_ok=False)
records=[]
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def run(name,args,expected=0):
 start=time.monotonic()
 with(out/(name+'.log')).open('w')as f:result=subprocess.run(list(map(str,args)),cwd=r,stdout=f,stderr=subprocess.STDOUT,timeout=60)
 entry={'name':name,'argv':list(map(str,args)),'cwd':str(r),'exit':result.returncode,'expectedExit':expected,'seconds':time.monotonic()-start};records.append(entry);save()
 if result.returncode!=expected:raise RuntimeError(name)
def save():
 (out/'RESULTS.json').write_text(json.dumps({'utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'head':subprocess.check_output(['git','rev-parse','HEAD'],cwd=r,text=True).strip(),'scope':'Isolated exact native sphere/box manifold reuse. Build labels are not substituted for explicit modified-source hashes. No production edits.','records':records,'files':{str(p.relative_to(r)):sha(p)for p in[d/'CanonicalSphereBox.h',d/'WorldImpactEvents.cpp',d/'ReciprocalSphereBoxTests.cpp',d/'ReciprocalImpactWorldTests.cpp']},'baseEngine':sha(d/'baseline/libjudas_engine.a'),'candidateEngine':sha(out/'libjudas_engine.a')if(out/'libjudas_engine.a').exists()else None},indent=2)+'\n')
lines=(r/'build/CMakeFiles/judas_engine.dir/flags.make').read_text().splitlines();flags=[]
for label in ['CXX_DEFINES','CXX_INCLUDES','CXX_FLAGS']:flags+=shlex.split(next(x.split('=',1)[1]for x in lines if x.startswith(label+' =')))
def link(target,binary,archive,objects=None):
 args=shlex.split((r/'build/CMakeFiles'/f'{target}.dir/link.txt').read_text());objs=objects or[str((r/'build'/x).resolve())for x in args if x.endswith('.o')];libs=args[args.index('libjudas_engine.a')+1:];libs=[str((r/'build'/x).resolve())if x.endswith('.a')and not x.startswith('/')else x for x in libs]
 run('link-'+binary.name,['/usr/bin/c++','-O3','-DNDEBUG',*objs,'-o',binary,archive,*libs])
obj=out/'WorldImpactEvents.cpp.o';run('compile-events',['/usr/bin/c++',*flags,'-I'+str(r/'src/physics'),'-c',d/'WorldImpactEvents.cpp','-o',obj]);archive=out/'libjudas_engine.a';shutil.copyfile(d/'baseline/libjudas_engine.a',archive);run('archive',['ar','r',archive,obj])
for target in ['judas_physics_stress_live_benchmark','judas_physics_scaling_diagnostics']:link(target,out/target,archive)
# These source objects are identical in both binary groups. Current build identity
# is explicit in the root provenance; it does not influence collision arithmetic.
cases=[x for x in json.loads((r/'.cache/prealpha-pass5/dense-linux/RESULTS.json').read_text())['results']if x['name'].startswith('judas_')and x['name']!='judas_impact_candidate_cache_tests']
for row in cases:
 target=row['name'];link(target,out/target,archive)
 run('baseline-'+target,[r/'build'/target,*row['argv'][1:]])
 run('candidate-'+target,[out/target,*row['argv'][1:]])
for name in ['ReciprocalSphereBoxTests','ReciprocalImpactWorldTests']:
 obj=out/(name+'.cpp.o');run('compile-'+name,['/usr/bin/c++',*flags,'-c',d/(name+'.cpp'),'-o',obj])
 link('judas_contact_geometry_tests',out/name,archive,[obj])
 if name=='ReciprocalImpactWorldTests':
  link('judas_contact_geometry_tests',out/(name+'-baseline'),d/'baseline/libjudas_engine.a',[obj]);run('baseline-world',[out/(name+'-baseline')]);run('candidate-world',[out/name]);run('disabled-collision-negative',[out/name,'--no-collision'],1)
 else:
  run('same-context-positive',[out/name]);run('missing-normal-negative',[out/name,'--omit-normal'],1);run('missing-anchors-negative',[out/name,'--omit-anchors'],1);run('unsupported-box-negative',[out/name,'--all-primitives'],1)
assert(out/'baseline-world.log').read_bytes()==(out/'candidate-world.log').read_bytes(),'World physical trace mismatch'
(out/'WORLD_COMPARISON.json').write_text(json.dumps({'identicalBytes':True,'bytes':(out/'baseline-world.log').stat().st_size,'sha256':sha(out/'baseline-world.log'),'candidateBinarySha256':sha(out/'ReciprocalImpactWorldTests'),'baselineBinarySha256':sha(out/'ReciprocalImpactWorldTests-baseline')},indent=2)+'\n')
print('Same-object focused fixtures and exact actual compound world trace PASS',flush=True)
