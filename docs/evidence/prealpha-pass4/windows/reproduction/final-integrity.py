from pathlib import Path
import hashlib,json,time
root=Path(r'P:\JudasValidation\PreAlphaPass4');source=root/'Source'
def sha(p):
 with p.open('rb')as f:return hashlib.file_digest(f,'sha256').hexdigest()
records=json.loads((source/'JUDAS_SOURCE_FILES.json').read_text(encoding='utf-8'))['files'];source_errors=[r['path'] for r in records if sha(source/r['path'])!=r['sha256']]
binaries=json.loads((root/'NATIVE_BINARIES.json').read_text(encoding='utf-8-sig'))['files'];binary_errors=[r['path']for r in binaries if sha(Path(r['path']))!=r['sha256']]
package=json.loads((root/'PRISTINE_PACKAGE_FILES.json').read_text(encoding='utf-8-sig'));pristine=root/('Moved Game '+chr(0xe9)+' '+chr(0x3b1));package_errors=[r['path']for r in package if sha(pristine/r['path'])!=r['sha256']]
shots=[]
for name in ['editor.edit.png','editor.play.png','runtime.png']:
 p=root/'GuiWork/Results'/name
 shots.append({'path':str(p),'bytes':p.stat().st_size,'sha256':sha(p)})
output={'schema':1,'utc':time.strftime('%Y-%m-%dT%H:%M:%SZ',time.gmtime()),'sourceFiles':len(records),'sourceErrors':source_errors,'nativeBinaryFiles':len(binaries),'nativeBinaryErrors':binary_errors,'pristinePackageFiles':len(package),'pristinePackageErrors':package_errors,'screenshots':shots,'passed':not(source_errors or binary_errors or package_errors)}
(root/'FINAL_INTEGRITY.json').write_text(json.dumps(output,indent=2)+'\n',encoding='utf-8');print(json.dumps(output));raise SystemExit(0 if output['passed']else 1)
