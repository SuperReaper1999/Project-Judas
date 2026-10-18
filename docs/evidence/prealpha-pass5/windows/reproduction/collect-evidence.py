from pathlib import Path
import hashlib,json,time,zipfile
root=Path(r'P:\JudasValidation\PreAlphaPass5')
output=root/'EVIDENCE.zip'
if output.exists():raise ValueError('Preserve previous evidence archive')
files=[]
for path in root.iterdir():
 if path.is_file() and path.suffix in ('.json','.py','.ps1','.log','.txt'):files.append(path)
for area in ('GuiWork/Results','GuiWork/ContactFollowupResults','GuiWork/ColdCases'):
 for path in (root/area).rglob('*'):
  if path.is_file() and 'project' not in path.relative_to(root/area).parts:files.append(path)
for path in (root/'GuiWork').glob('*.log'):files.append(path)
for path in (root/'GuiWork').glob('*.txt'):files.append(path)
files=sorted(set(files),key=lambda p:p.relative_to(root).as_posix())
records=[]
with zipfile.ZipFile(output,'x',compression=zipfile.ZIP_DEFLATED,compresslevel=6) as archive:
 for path in files:
  content=path.read_bytes();name=path.relative_to(root).as_posix();records.append({'path':name,'bytes':len(content),'sha256':hashlib.sha256(content).hexdigest()});archive.writestr(name,content)
 metadata={'schema':1,'utc':time.strftime('%Y-%m-%dT%H:%M:%SZ',time.gmtime()),'files':records,'scope':'Raw native validation receipts/logs/snapshots/screenshots; no executables, object archives, model/build assets or accepted package copies.'}
 archive.writestr('COLLECTION_MANIFEST.json',json.dumps(metadata,indent=2)+'\n')
print(json.dumps({'path':str(output),'bytes':output.stat().st_size,'sha256':hashlib.sha256(output.read_bytes()).hexdigest(),'files':len(records)}))
