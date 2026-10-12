#!/usr/bin/python3
"""Prepare immutable named native inputs only. Does not contact the VM."""
import json,hashlib,zipfile,copy
from pathlib import Path
R=Path.cwd(); T=R/'.cache/windows-vm-transfer'; E=R/'docs/evidence/m78-closure/native'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
v1=json.loads((T/'m78-final-cold-repair-v1.json').read_text());v2=json.loads((T/'m78-final-cold-repair-v2.json').read_text())
base={c['id']:c for c in v1['cases']}; base.update({c['id']:c for c in v2['cases']})
m={k:copy.deepcopy(v2[k])for k in ['engineFlags','baselineMemberCount']};m.update(label='closure-final-v1',scope='Actual M78 closure production source. Native Windows functional validation only; frozen archive plus explicit replacement PW/WP objects, unchanged fixture/controller/timestep. No performance gate.',scratch=r'C:\JudasValidation\M78ClosureBuild',output=r'C:\JudasValidation\M78ClosureResults',baselineLibrary={'path':r'C:\JudasValidation\M78ContinuationBuild\cold-repair-final-v2\judas_engine.lib','sha256':'71a722dded64cfcc634d679e55ddbd257058da888ada914f7f7150b8d88c2d62','bytes':67019246},oldSource=r'C:\JudasValidation\M78ContinuationBuild\cold-repair-final-v2\source')
paths=set(p.relative_to(R).as_posix()for p in (R/'src').rglob('*.h'))
paths.update(['src/PhysicsWorld.cpp','src/WorldPersistence.cpp','tests/PhysicsContinuationTests.cpp','tests/SaveStreamingTests.cpp','tests/SignalApplicationTests.cpp','third_party/nlohmann/json.hpp','tests/TestInput.h'])
for p in [R/'docs/evidence/m78-closure/review-physics/RestoredPublicationIsolationProbeV2.cpp',R/'docs/evidence/m78-closure/review-physics/MixedBalanceLabProbeV2.cpp',R/'docs/evidence/m78-closure/review-physics/MixedCurrentPhysicalEndpointObserver.cpp']:
 if p.exists():paths.add(p.relative_to(R).as_posix())
archive=T/'m78-closure-source-v1.zip';assert not archive.exists(),'Preserve prepared archive'
with zipfile.ZipFile(archive,'w',zipfile.ZIP_DEFLATED) as z:
 for rel in sorted(paths):z.write(R/rel,rel)
m['archiveName']=archive.name;m['archiveSHA256']=sha(archive);m['files']=[{'path':p,'bytes':(R/p).stat().st_size,'sha256':sha(R/p)}for p in sorted(paths)]
m['replacements']=[{'path':p,'sha256':sha(R/p),'objectName':Path(p).stem+'.obj'}for p in ['src/PhysicsWorld.cpp','src/WorldPersistence.cpp']]
m['cases']=[]
for cid in ['controller','motion','physical','joints','contact_lifecycle','impact_timing','gravity','world_save','streaming','balance_lab','character_lab','physics_continuation','runtime']:
 c=copy.deepcopy(base[cid]);c['runs']=copy.deepcopy(c.get('runs', [{'id':cid,'arguments':[]}]))
 for run in c['runs']:
  run['arguments']=[a.replace(r'M78ColdRepairFinalV2Results','M78ClosureResults')for a in run.get('arguments',[])]
  run['expectedExitCode']=1 if run['id']=='balance-exact-cold-read' else 0
 if cid=='balance_lab':c['runs']=[r for r in c['runs']if r['id']in ['balance-step-trace','balance-lifecycle','balance-radial','balance-cold-stand-write','balance-cold-stand-read','balance-cold-recovery-write','balance-cold-recovery-read','balance-exact-cold-read','balance-new-cold-write','balance-new-cold-read']]
 if cid=='physics_continuation':c['inputs'][0].update(sha256=sha(R/'tests/PhysicsContinuationTests.cpp'),reuseOriginal=False)
 m['cases'].append(c)
for cid,src,runs in [
 ('publication_isolation','docs/evidence/m78-closure/review-physics/RestoredPublicationIsolationProbeV2.cpp',[{'id':'publication-isolation','arguments':[]}]),
 ('mixed_balance','docs/evidence/m78-closure/review-physics/MixedBalanceLabProbeV2.cpp',[{'id':'mixed-write','arguments':[m['output']+r'\mixed-cold','cold-write']},{'id':'mixed-read','arguments':[m['output']+r'\mixed-cold','cold-read']}]),
 ('mixed_endpoint','docs/evidence/m78-closure/review-physics/MixedCurrentPhysicalEndpointObserver.cpp',[{'id':'mixed-endpoint-write','arguments':[m['output']+r'\mixed-endpoint','cold-write']},{'id':'mixed-endpoint-read','arguments':[m['output']+r'\mixed-endpoint','cold-read']}]),
 ('signal_application','tests/SignalApplicationTests.cpp',[{'id':'signals-write','arguments':[r'C:\JudasValidation\Source\projects\signals_lab\signals_lab.judasproj']},{'id':'signals-read','arguments':[r'C:\JudasValidation\Source\projects\signals_lab\signals_lab.judasproj','load']}])]:
 if not (R/src).exists():continue
 c=copy.deepcopy(base['balance_lab']);c.update(id=cid,inputs=[{'path':src,'sha256':sha(R/src),'bytes':(R/src).stat().st_size,'reuseOriginal':False,'objectName':cid+'-0.obj'}],runs=[dict(r,expectedExitCode=0)for r in runs]);m['cases'].append(c)
(T/'m78-closure-v1.json').write_text(json.dumps(m,indent=2)+'\n');(E/'PREPARED_INPUTS.json').write_text(json.dumps(m,indent=2)+'\n')
print(json.dumps({'archiveBytes':archive.stat().st_size,'archiveSHA256':sha(archive),'files':len(paths),'cases':[c['id']for c in m['cases']]},indent=2))
