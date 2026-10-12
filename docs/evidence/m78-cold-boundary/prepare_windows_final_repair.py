#!/usr/bin/env python3
"""Prepare explicit frozen native inputs; does not compile or operate guest."""
import hashlib, json, zipfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[3]
TRANSFER=ROOT/'.cache/windows-vm-transfer'
REPORTS=TRANSFER/'reports'
def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()
def main():
 old=json.loads((TRANSFER/'m78-continuation-direct-combined-v2.json').read_text())
 label='cold-repair-final-v1';scratch='C:\\JudasValidation\\M78ContinuationBuild\\'+label
 output='C:\\JudasValidation\\M78ColdRepairFinalResults'
 flags=json.loads((TRANSFER/'m78-native-new-capture-publication.json').read_text())['engineFlags']
 paths=list(sorted((ROOT/'src').rglob('*.h')))+[ROOT/'src'/n for n in ['PhysicsWorld.cpp','WorldPersistence.cpp','PhysicalBalance.cpp']]
 previous_functional=json.loads((TRANSFER/'m78-native-functional-manifest.json').read_text())
 selected=['controller','motion','foundation','physical','joints','contact_lifecycle','impact_timing','gravity','world_save','balance_lab','character_lab','runtime']
 cases=[];objectdir=TRANSFER/'m78-final-repair-objects';objectdir.mkdir(exist_ok=True)
 for name in selected:
  c=dict(next(c for c in old['cases'] if c['id']==name));c['inputs']=[]
  archive=json.loads((REPORTS/f'm78-combined-v2-{name}-archive.json').read_text(encoding='utf-8-sig'))
  z=REPORTS/archive['archiveName'];assert z.stat().st_size==archive['archiveBytes'] and sha(z)==archive['archiveSHA256']
  with zipfile.ZipFile(z) as content:
   for i,s in enumerate(c['sources']):
    rel=s['sourceRelativePath'];p=ROOT/rel;expected=sha(p);objname=f'{name}-{i}.obj'
    original=next(f for f in archive['files'] if f['name']==objname)
    data=content.read(objname);assert len(data)==original['bytes'] and hashlib.sha256(data).hexdigest()==original['sha256']
    reused=expected==s['sha256']
    op=objectdir/objname;op.write_bytes(data)
    inp={'path':rel,'sha256':expected,'bytes':p.stat().st_size,'reuseOriginal':reused,'originalObjectSHA256':original['sha256'],'objectURL':f'http://192.168.122.1:8872/m78-final-repair-objects/{objname}','objectName':objname}
    c['inputs'].append(inp)
    if not reused:paths.append(p)
  previous=next((f for f in previous_functional['cases'] if f['id']==name),None)
  c['runs']=previous['runs'] if previous else []
  for run in c['runs']:
   run['arguments']=[a.replace('M78NativeFunctionalResults','M78ColdRepairFinalResults') for a in run.get('arguments',[])]
  if name=='balance_lab':
   c['runs'] += [{'id':'balance-new-cold-write','arguments':[output+'\\balance-new-cold','cold-write']},{'id':'balance-new-cold-read','arguments':[output+'\\balance-new-cold','cold-read']}]
  cases.append(c)
 # New bounded continuation regression uses the ordinary engine-only fixture recipe.
 new=ROOT/'tests/PhysicsContinuationTests.cpp';paths.append(new)
 template=dict(next(c for c in old['cases'] if c['id']=='foundation'))
 template.update({'id':'physics_continuation','sources':[],'arguments':[], 'runs':[{'id':'physics-continuation','arguments':[]}], 'inputs':[{'path':'tests/PhysicsContinuationTests.cpp','sha256':sha(new),'bytes':new.stat().st_size,'reuseOriginal':False,'objectName':'physics_continuation-0.obj'}]})
 cases.append(template)
 zipname='m78-final-cold-repair-source-v1.zip';zp=TRANSFER/zipname;files=[]
 with zipfile.ZipFile(zp,'w',zipfile.ZIP_DEFLATED) as z:
  for p in sorted(set(paths)):
   rel=p.relative_to(ROOT).as_posix();z.write(p,rel);files.append({'path':rel,'bytes':p.stat().st_size,'sha256':sha(p)})
 manifest={'label':label,'scope':'Final actual production candidate native focused validation; unchanged original fixtures and acceptance. Exact legacy a106 remains preserved separately. No VM performance gate.','scratch':scratch,'output':output,'archiveName':zipname,'archiveSHA256':sha(zp),'files':files,'engineFlags':flags,'baselineLibrary':{'path':'C:\\JudasValidation\\M78ContinuationBuild\\combined-v2\\judas_engine.lib','bytes':66840496,'sha256':'791edc06d69e9a0c750c2bf23f91b20936ed5c0d530f20a936eed0c544c9ba48'},'baselineMemberCount':138,'replacements':[{'path':'src/'+n+'.cpp','objectName':n+'.obj','sha256':sha(ROOT/'src'/f'{n}.cpp')} for n in ['PhysicsWorld','WorldPersistence','PhysicalBalance']],'cases':cases,'exactSave':{'path':'C:\\JudasValidation\\M78ContinuationResults\\diagnostic-boundary-v1\\source\\slots\\m77.save','sha256':'a106ce212416a510ec0470d2cf7abe0fc96fccdfec202c95e5209c9e56c4379b'},'project':'C:\\JudasValidation\\Source\\projects\\balance_lab'}
 p=TRANSFER/'m78-final-cold-repair-v1.json';p.write_text(json.dumps(manifest,indent=2)+'\n')
 (ROOT/'docs/evidence/m78-cold-boundary/native-final-cold-repair-manifest.json').write_bytes(p.read_bytes())
 print(json.dumps({'manifest':str(p),'archive':str(zp),'bytes':zp.stat().st_size,'sha256':sha(zp),'units':manifest['replacements'],'newTestCompiles':[(c['id'],i['path']) for c in cases for i in c['inputs'] if not i['reuseOriginal']]},indent=2))
if __name__=='__main__':main()
