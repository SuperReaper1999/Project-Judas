from pathlib import Path
import json,hashlib,zipfile,shutil,copy
r=Path.cwd();t=r/'.cache/windows-vm-transfer';e=r/'docs/evidence/m78-lifecycle/lif003/native';e.mkdir(parents=True,exist_ok=True)
h=lambda p:hashlib.sha256(p.read_bytes()).hexdigest();old=json.loads((t/'m78-lifecycle-v1.json').read_text());engine=json.loads((r/'docs/evidence/m78-lifecycle/native/m78-lifecycle-engine.json').read_text(encoding='utf-8-sig'))
m=copy.deepcopy(old);m.update(scratch=r'C:\JudasValidation\M78Lif003Build',output=r'C:\JudasValidation\M78Lif003Results',scope='LIF-003 native Windows informational functional check; not a Linux checkpoint gate.')
m['replacements']=[dict(path=s,sha256=h(r/s),bytes=(r/s).stat().st_size,objectName=Path(s).stem+'.obj',reuseOriginal=False)for s in ['src/WorldCharacter.cpp','src/RuntimeWorld.cpp']]
m['retainedReplacementObjects']=[u for u in engine['objects'] if u['source']!='src/WorldCharacter.cpp'];m['auxiliaryFixtures']=[]
base=next(c for c in old['cases'] if c['id']=='motor');m['cases']=[]
for cid,s in [('tags','tests/MotorRuntimeTagsTests.cpp'),('publication','tests/CharacterPublicationTests.cpp')]:
 c=copy.deepcopy(base);c.update(id=cid,inputs=[dict(path=s,sha256=h(r/s),bytes=(r/s).stat().st_size,reuseOriginal=False,objectName=cid+'.obj')],runs=[dict(id=cid,arguments=[m['output']+'\\'+cid],expectedExitCode=0)]);m['cases'].append(c)
paths={f['path'] for f in old['files'] if f['path'].startswith(('src/','third_party/'))};paths.update(u['path']for u in m['replacements']);paths.update(c['inputs'][0]['path']for c in m['cases'])
archive=t/'m78-lif003-source.zip';assert not archive.exists()
with zipfile.ZipFile(archive,'w',zipfile.ZIP_DEFLATED)as z:
 for s in sorted(paths):z.write(r/s,s)
m.update(archiveName=archive.name,archiveSHA256=h(archive),files=[dict(path=s,sha256=h(r/s),bytes=(r/s).stat().st_size)for s in sorted(paths)])
(t/'m78-lif003-v1.json').write_text(json.dumps(m,indent=2)+'\n');shutil.copyfile(t/'m78-lif003-v1.json',e/'PREPARED_INPUTS.json')
for name in ['windows_prepare.ps1','windows_case.ps1']:
 s=(r/'docs/evidence/m78-lifecycle/native/probes'/name).read_text().replace('m78-lifecycle','m78-lif003').replace('M78LifecycleResults','M78Lif003Results')
 if name=='windows_case.ps1':s=s.replace('(PhysicsWorld|WorldPersistence|WorldCharacter|WorldRegions|WorldScripts)','(PhysicsWorld|WorldPersistence|WorldCharacter|WorldRegions|WorldScripts|RuntimeWorld)')
 p=e/name;p.write_text(s);shutil.copyfile(p,t/('m78-lif003-'+name))
