from pathlib import Path
import json,tarfile,hashlib,time
r=Path.cwd();o=r/'.cache/prealpha-final-qualification';sdk=o/'sdk-final'
info=json.loads((sdk/'BUILD_INFO.json').read_text());assert info['source_revision']=='f7691198ee299a5752c8a55c93fb42e639d033cf' and not info['source_dirty']
manifest=json.loads((sdk/'SDK_MANIFEST.json').read_text())
for row in manifest['files']:
 p=sdk/row['path'];assert hashlib.sha256(p.read_bytes()).hexdigest()==row['sha256'],row['path']
archive=o/'Judas-PreAlpha-Linux-Ubuntu24-f7691198.tar.gz';start=time.monotonic()
with tarfile.open(archive,'w:gz',compresslevel=1) as a:a.add(sdk,arcname='Judas-PreAlpha-Linux-Ubuntu24')
h=hashlib.sha256(archive.read_bytes()).hexdigest();Path(str(archive)+'.sha256').write_text(h+'  '+archive.name+'\n')
record={'engine_revision':info['source_revision'],'build':info,'sdk_directory':str(sdk),'archive':str(archive),'archive_sha256':h,'archive_bytes':archive.stat().st_size,'sdk_payload_files':len(manifest['files']),'sdk_payload_bytes':sum(x['bytes'] for x in manifest['files']),'tool_count':len(manifest['tools']),'tool_sha256':{x['path']:x['sha256'] for x in manifest['files'] if x['path'] in manifest['tools']},'sdk_manifest_sha256':hashlib.sha256((sdk/'SDK_MANIFEST.json').read_bytes()).hexdigest(),'archive_seconds':time.monotonic()-start,'standalone_packages':[str(o/'final-moved-editor-export'),str(o/'final-moved-cli-export')],'created_project':str(o/'sdk-work-final/game/SDK_First_Game.judasproj'),'note':'Local qualification artifact, not an Alpha release/publication. Bundled engine/docs are the exact clean runtime checkpoint; subsequent final qualification receipts/support supplement are separate.'}
(o/'FINAL_ARTIFACTS.json').write_text(json.dumps(record,indent=2)+'\n');print('SDK archive',record['archive_bytes'],'bytes',h,flush=True)
