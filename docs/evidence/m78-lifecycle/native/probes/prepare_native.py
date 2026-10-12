#!/usr/bin/python3
"""Freeze named native inputs after the root source freeze; no VM contact."""
import copy,hashlib,json,shutil,zipfile
from pathlib import Path
R=Path.cwd();T=R/'.cache/windows-vm-transfer';E=R/'docs/evidence/m78-lifecycle/native'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def read(p):return json.loads(p.read_text(encoding='utf-8-sig'))
def row(rel,obj=None):
 p=R/rel
 return dict(path=rel,sha256=sha(p),bytes=p.stat().st_size,reuseOriginal=False,objectName=obj or p.stem+'.obj')
old=read(T/'m78-closure-v1.json');base={c['id']:c for c in old['cases']}
freeze=read(R/'docs/evidence/m78-lifecycle/final-build-v2/BUILD.json')
for rel,h in freeze['sourceSHA256'].items():assert sha(R/rel)==h,(rel,'source freeze differs')
m={k:copy.deepcopy(old[k])for k in ('engineFlags','baselineLibrary','baselineMemberCount','oldSource')}
m.update(label='lifecycle-final-v1',scope='Native Windows functional lifecycle correction checks only. Four new production translation units plus accepted current PhysicsWorld object ahead of frozen archive; no ABI changes or Windows performance acceptance.',startingHEAD='0695433b949df0c1d05ac6798bf118121cc53e2b',scratch=r'C:\JudasValidation\M78LifecycleBuild',output=r'C:\JudasValidation\M78LifecycleResults',diagnosticMapOmittedForBoundedStorage=True)
m['replacements']=[dict(row(rel),objectName=Path(rel).stem+'.obj')for rel in freeze['sourceSHA256']]
m['retainedReplacementObjects']=[dict(path=r'C:\JudasValidation\M78ClosureBuild\objects\PhysicsWorld.obj',source='src/PhysicsWorld.cpp',sourceSHA256=sha(R/'src/PhysicsWorld.cpp'),sha256='6d70c3f9ff8c932ddc8bac2b0a7ab47539813d95e0ea17d7e000e77465702ed1',bytes=1495011)]
paths=set(p.relative_to(R).as_posix()for p in (R/'src').rglob('*.h'))
paths.update(freeze['sourceSHA256']);paths.update(['third_party/nlohmann/json.hpp','tests/TestInput.h'])
paths.update(p.relative_to(R).as_posix()for p in (R/'projects/ragdoll_demo').rglob('*')if p.is_file())
m['auxiliaryFixtures']=[dict(path='projects/animation_demo/Assets/models/bend_bar.glb',bytes=(R/'projects/animation_demo/Assets/models/bend_bar.glb').stat().st_size,sha256=sha(R/'projects/animation_demo/Assets/models/bend_bar.glb'),url='http://192.168.122.1:8872/m78-lifecycle-animation-bend_bar.glb')]
shutil.copyfile(R/'projects/animation_demo/Assets/models/bend_bar.glb',T/'m78-lifecycle-animation-bend_bar.glb')
m['cases']=[]
for cid,src,args in [('character_publication','tests/CharacterPublicationTests.cpp',[m['output']+r'\character-publication']),('save_exclusion','tests/SaveExclusionTests.cpp',[m['output']+r'\save-exclusion',m['scratch']+r'\source']),('consumer_lifecycle','tests/ConsumerLifecycleAuditTests.cpp',[m['output']+r'\consumer-lifecycle']),('motor','tests/CharacterMotorTests.cpp',[])]:
 c=copy.deepcopy(base['balance_lab']);c.update(id=cid,inputs=[row(src,cid+'-0.obj')],runs=[dict(id=cid,arguments=args,expectedExitCode=0)]);m['cases'].append(c);paths.add(src)
 if cid=='consumer_lifecycle':
  for inp in base['runtime']['inputs'][1:]:
   assert sha(R/inp['path'])==inp['sha256'],('unchanged Application TU',inp['path']);c['inputs'].append(copy.deepcopy(inp))
for cid in ['physics_continuation','world_save','streaming','balance_lab','character_lab']:
 c=copy.deepcopy(base[cid])
 if cid=='streaming':c['workingDirectory']=m['scratch']+r'\source'
 for inp in c['inputs']:
  assert sha(R/inp['path'])==inp['sha256'],('unchanged test TU',inp['path'])
  if cid=='physics_continuation':
   receipt=read(E.parent.parent/'m78-closure/native/m78-closure-physics_continuation-build.json');f=receipt['reusedOrFreshInputs'][0]
   host=R/'.cache/m78-closure/native-binaries/physics_continuation/physics_continuation-0.obj';assert sha(host)==f['sha256']
   dst=T/'m78-lifecycle-physics-continuation-0.obj';assert not dst.exists() or sha(dst)==sha(host)
   if not dst.exists():shutil.copyfile(host,dst)
   inp.update(reuseOriginal=True,originalObjectSHA256=f['sha256'],objectURL='http://192.168.122.1:8872/'+dst.name)
 c['runs']=copy.deepcopy(c.get('runs',[dict(id=cid,arguments=[],expectedExitCode=0)]))
 if cid=='balance_lab':c['runs']=[r for r in c['runs']if r['id']in ['balance-step-trace','balance-new-cold-write','balance-new-cold-read']]
 if cid=='character_lab':c['runs']=[r for r in c['runs']if 'cold-' in r['id']]
 for run in c['runs']:
  run['arguments']=[a.replace('M78ClosureResults','M78LifecycleResults')for a in run.get('arguments',[])];run['expectedExitCode']=0
 m['cases'].append(c)
src='docs/evidence/m78-closure/review-physics/MixedCurrentPhysicalEndpointObserver.cpp';paths.add(src)
c=copy.deepcopy(base['balance_lab']);c.update(id='mixed_endpoint',inputs=[row(src,'mixed-endpoint-0.obj')],runs=[dict(id='mixed-endpoint-write',arguments=[m['output']+r'\mixed-endpoint','cold-write'],environment={'JUDAS_DIAGNOSTIC_CONTINUE_WRITE':'1'},expectedExitCode=0),dict(id='mixed-endpoint-read',arguments=[m['output']+r'\mixed-endpoint','cold-read'],environment={'JUDAS_DIAGNOSTIC_CONTINUE_WRITE':None},expectedExitCode=0)]);m['cases'].append(c)
for c in m['cases']:
 for inp in c['inputs']:paths.add(inp['path'])
archive=T/'m78-lifecycle-source-v1.zip';assert not archive.exists(),'Preserve prepared archive'
with zipfile.ZipFile(archive,'w',zipfile.ZIP_DEFLATED)as z:
 for rel in sorted(paths):z.write(R/rel,rel)
m.update(archiveName=archive.name,archiveSHA256=sha(archive),files=[dict(path=p,bytes=(R/p).stat().st_size,sha256=sha(R/p))for p in sorted(paths)])
(T/'m78-lifecycle-v1.json').write_text(json.dumps(m,indent=2)+'\n');(E/'PREPARED_INPUTS.json').write_text(json.dumps(m,indent=2)+'\n')
print(json.dumps({'preparedOnly':True,'archiveBytes':archive.stat().st_size,'archiveSHA256':sha(archive),'files':len(paths),'cases':[c['id']for c in m['cases']]},indent=2))
