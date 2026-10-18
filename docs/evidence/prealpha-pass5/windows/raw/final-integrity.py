from pathlib import Path
import hashlib,json,struct,time,zipfile
root=Path(r'P:\JudasValidation\PreAlphaPass5');previous=Path(r'P:\JudasValidation\PreAlphaPass4');source=previous/'Source'
def sha(path):
 with path.open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()
def read(path):return json.loads(path.read_text(encoding='utf-8-sig'))
manifest=read(source/'JUDAS_SOURCE_FILES.json')['files']
errors=[row['path'] for row in manifest if sha(source/row['path'])!=row['sha256']]
original=read(root/'SOURCE_VERIFIED.json')
for filename in ('JUDAS_SOURCE_FILES.json','JUDAS_SOURCE_INFO.json'):
 expected=read(root/'source-delta.receipt.json')['afterInfo']
 if filename=='JUDAS_SOURCE_FILES.json' and sha(source/filename)!=expected['source_files_sha256']:errors.append(filename)
 if filename=='JUDAS_SOURCE_INFO.json' and read(source/filename)!=expected:errors.append(filename)
for row in original['acceptedNativeBinariesPreserved']:
 if sha(Path(row['preservedAt']))!=row['sha256']:errors.append('accepted native backup: '+row['preservedAt'])
old_package=read(previous/'PRISTINE_PACKAGE_FILES.json');old_root=previous/('Moved Game '+chr(0xe9)+' '+chr(0x3b1))
for row in old_package:
 if sha(old_root/row['path'])!=row['sha256']:errors.append('accepted package: '+row['path'])
if sha(previous/'final-source.tar.gz')!=original['originalAcceptedSourceArchiveSHA256']:errors.append('accepted original source archive')
functional=read(root/'FUNCTIONAL_RESULTS.json');follow=read(root/'CONTACT_FOLLOWUP_RESULTS.json')
unexpected=[row['label'] for row in functional['rows'] if not row['passed'] and row['label']!='judas_contact_cache_tests']
if not follow['passed']:unexpected.append('contact-cache-original-fixture-followup')
identities=[]
for name in ('judas','judas_editor','judas_export'):
 identity=read(root/f'GuiWork/Results/{name}-identity.stdout.log')
 identities.append({'tool':name,'identity':identity})
 for key in ('build_id','source_revision','source_fingerprint','source_origin','source_dirty'):
  if identity[key]!=functional['sourceIdentity'][key]:errors.append('inconsistent native identity: '+name+' '+key)
pe=[]
for row in functional['binaryFiles']:
 path=Path(row['path'])
 if sha(path)!=row['sha256']:errors.append('executed native binary changed: '+str(path))
 with path.open('rb') as stream:
  dos=stream.read(64);offset=struct.unpack_from('<I',dos,60)[0];stream.seek(offset);header=stream.read(24)
  if dos[:2]!=b'MZ' or header[:4]!=b'PE\0\0' or struct.unpack_from('<H',header,4)[0]!=0x8664:errors.append('not native AMD64 PE: '+str(path))
 pe.append(row)
extra=[]
expected_paths={row['path'] for row in manifest}|{'JUDAS_SOURCE_FILES.json','JUDAS_SOURCE_INFO.json'}
for path in source.rglob('*'):
 if path.is_file() and path.relative_to(source).as_posix() not in expected_paths:extra.append(path.relative_to(source).as_posix())
if extra:errors.append('source contains unexpected files')
archive=previous/'Build/Release/judas_engine.lib'
result={'schema':1,'utc':time.strftime('%Y-%m-%dT%H:%M:%SZ',time.gmtime()),'sourceFilesVerified':len(manifest),'sourceExtraFiles':extra,'sourceIdentity':functional['sourceIdentity'],'sourceErrors':errors,'unexpectedFunctionalResults':unexpected,'knownOriginalCurvedContinuationFailurePreserved':True,'initialMissingFixtureSetupFailurePreserved':True,'correctedFixtureFollowup':follow['passed'],'acceptedNativeBackupsVerified':len(original['acceptedNativeBinariesPreserved']),'acceptedPristinePackageVerified':len(old_package),'acceptedOriginalSourceArchiveVerified':True,'binaryFiles':pe,'nativeToolIdentities':identities,'engineArchive':{'path':str(archive),'bytes':archive.stat().st_size,'sha256':sha(archive)},'isolatedSoftwareOverlay':[{'path':str(root/'GuiWork/QualifiedApps'/name),'sha256':sha(root/'GuiWork/QualifiedApps'/name)} for name in ('opengl32.dll','libgallium_wgl.dll')],'passed':not(errors or unexpected),'scope':'Native Windows functional qualification with explicit preserved curved failure and corrected test-data prerequisite. No Windows performance or physical GPU claim.'}
(root/'FINAL_INTEGRITY.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
print(json.dumps({'passed':result['passed'],'errors':errors,'unexpected':unexpected,'files':len(manifest)}))
raise SystemExit(0 if result['passed'] else 1)
