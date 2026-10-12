#!/usr/bin/python3
"""Small host-side control for the explicit frozen native matrix.

Starts only one named operation. It never loops a suite or deletes guest files
without the separately verified archive approval.
"""
import argparse,base64,hashlib,json,pathlib,re,zipfile
import libvirt,libvirt_qemu

ROOT=pathlib.Path.cwd();TRANSFER=ROOT/'.cache/windows-vm-transfer';REPORTS=TRANSFER/'reports';EVIDENCE=ROOT/'docs/evidence/m78-continuation'
p=argparse.ArgumentParser();p.add_argument('mode',choices=['case','run','archive','approve','cleanup','status']);p.add_argument('value');p.add_argument('extra',nargs='?');p.add_argument('--label',default='combined-v2');p.add_argument('--manifest');a=p.parse_args()
label=a.label
if not re.fullmatch(r'[a-z0-9_-]+',label):raise SystemExit('Simple explicit candidate label required')
manifest=a.manifest or 'm78-continuation-direct-'+label+'.json'
if not re.fullmatch(r'm78-continuation-direct-[a-z0-9_-]+\.json',manifest):raise SystemExit('Explicit prepared manifest filename required')
def sha(data):return hashlib.sha256(data).hexdigest()
def read(path):return json.loads(path.read_text(encoding='utf-8-sig'))
def literal(value):return "'"+value.replace("'","''")+"'"
def guest(script):
 script="$env:GALLIUM_DRIVER='llvmpipe';$env:JUDAS_ENGINE_ROOT='C:\\JudasValidation\\Source'\n"+script
 c=libvirt.open('qemu:///system');d=c.lookupByName('win10');code=base64.b64encode(script.encode('utf-16le')).decode()
 request={'execute':'guest-exec','arguments':{'path':'C:\\Windows\\System32\\WindowsPowerShell\\v1.0\\powershell.exe','arg':['-NoProfile','ExecutionPolicy','Bypass','-EncodedCommand',code],'capture-output':False}}
 request['arguments']['arg'][1]='-ExecutionPolicy'
 reply=json.loads(libvirt_qemu.qemuAgentCommand(d,json.dumps(request),30,0))
 print(json.dumps(reply))
def simple(value):
 if not re.fullmatch(r'[a-z0-9_-]+',value):raise SystemExit('Simple explicit case/run required')
 return value
if a.mode=='status':
 c=libvirt.open('qemu:///system');d=c.lookupByName('win10');reply=json.loads(libvirt_qemu.qemuAgentCommand(d,json.dumps({'execute':'guest-exec-status','arguments':{'pid':int(a.value)}}),30,0));print(json.dumps(reply));raise SystemExit
case=simple(a.value)
if a.mode=='case' and (TRANSFER/'m78-priority-observer.hold').exists() and case!='drive_observer':raise SystemExit('PAUSED at safe case boundary for root-prioritized cold-save observer; no guest process launched')
m=read(TRANSFER/manifest);rows=[r for r in m['cases'] if r['id']==case]
if len(rows)!=1:raise SystemExit('Named case absent from frozen manifest')
if a.mode=='case':guest("& 'C:\\JudasValidation\\M78ContinuationSource\\Scripts\\v6\\windows_direct_case.ps1' -ManifestName "+literal(manifest)+' -CaseId '+literal(case)+'\n')
elif a.mode=='run':
 run=simple(a.extra or '');rows=[r for r in m.get('additionalRuns',{}).get(case,[]) if r['id']==run]
 if len(rows)!=1:raise SystemExit('Named additional run absent from frozen manifest')
 guest("& 'C:\\JudasValidation\\M78ContinuationSource\\Scripts\\v6\\windows_direct_run.ps1' -ManifestName "+literal(manifest)+' -CaseId '+literal(case)+' -RunId '+literal(case+'-'+run)+' -Arguments @('+','.join(literal(x) for x in rows[0]['arguments'])+')\n')
elif a.mode in ['archive','cleanup']:
 tool='windows_archive_completed_case.ps1' if a.mode=='archive' else 'windows_remove_verified_case.ps1'
 guest("& 'C:\\JudasValidation\\M78ContinuationSource\\Scripts\\ArchiveV1\\"+tool+"' -Label "+literal(label)+' -CaseId '+literal(case)+'\n')
elif a.mode=='approve':
 name='m78-'+label+'-'+case+'-archive';receipt=read(REPORTS/(name+'.json'));compiled=read(REPORTS/('m78-'+label+'-'+case+'.json'));archive=REPORTS/receipt['archiveName']
 if archive.stat().st_size!=receipt['archiveBytes'] or sha(archive.read_bytes())!=receipt['archiveSHA256']:raise SystemExit('Transferred archive identity mismatch')
 if compiled['executableSHA256']!=next(f['sha256'] for f in receipt['files'] if f['path']==compiled['executable']):raise SystemExit('Compiled executable identity mismatch')
 with zipfile.ZipFile(archive) as z:
  if set(z.namelist())!={f['name'] for f in receipt['files']}:raise SystemExit('Unexpected/missing archive members')
  for f in receipt['files']:
   data=z.read(f['name'])
   if len(data)!=f['bytes'] or sha(data)!=f['sha256']:raise SystemExit('Exact archived member mismatch '+f['name'])
 approval={'scope':'Independent host byte count/SHA verification of every exact completed native executable/object before owned guest cleanup','hostVerified':True,'label':label,'case':case,'archiveSHA256':receipt['archiveSHA256'],'archiveBytes':receipt['archiveBytes'],'files':receipt['files']}
 target=TRANSFER/(name+'-approved.json')
 if target.exists():raise SystemExit('Preserve previous host approval')
 target.write_text(json.dumps(approval,indent=2)+'\n');(EVIDENCE/(name+'-host-verification.json')).write_text(json.dumps(approval,indent=2)+'\n')
 print(json.dumps({'hostVerified':True,'case':case,'files':len(receipt['files']),'archiveBytes':receipt['archiveBytes']}))
