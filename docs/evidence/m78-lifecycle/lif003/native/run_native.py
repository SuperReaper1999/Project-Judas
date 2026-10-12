#!/usr/bin/python3
"""Bounded informational native run; serial with Linux tests. No VM configuration changes."""
import base64,json,time,hashlib,shutil
from pathlib import Path
import libvirt,libvirt_qemu
r=Path.cwd();t=r/'.cache/windows-vm-transfer';e=r/'docs/evidence/m78-lifecycle/lif003/native';reports=t/'reports';cache=r/'.cache/m78-lifecycle/lif003-native';cache.mkdir(exist_ok=True)
conn=libvirt.open('qemu:///system');vm=conn.lookupByUUIDString('877d6b10-f85b-4108-9d07-11953f247974');rows=[]
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def collect():
 for p in reports.glob('m78-lif003-*'):
  dest=cache/p.name if p.suffix=='.bin'else e/p.name
  if dest.exists():assert dest.read_bytes()==p.read_bytes(),str(dest)
  else:shutil.copyfile(p,dest)
def run(label,script):
 start=time.monotonic();code=base64.b64encode(script.encode('utf-16le')).decode();reply=json.loads(libvirt_qemu.qemuAgentCommand(vm,json.dumps({'execute':'guest-exec','arguments':{'path':r'C:\Windows\System32\WindowsPowerShell\v1.0\powershell.exe','arg':['-NoProfile','-ExecutionPolicy','Bypass','-EncodedCommand',code],'capture-output':True}}),30,0));pid=reply['return']['pid'];print(label,pid,flush=True)
 while time.monotonic()-start<600:
  result=json.loads(libvirt_qemu.qemuAgentCommand(vm,json.dumps({'execute':'guest-exec-status','arguments':{'pid':pid}}),30,0))['return']
  if result['exited']:
   collect();rows.append(dict(label=label,pid=pid,seconds=time.monotonic()-start,result=result));(e/'OPERATIONS.json').write_text(json.dumps(rows,indent=2)+'\n');print(label,result.get('exitcode'),flush=True);return result.get('exitcode')==0
  time.sleep(2)
 collect();raise RuntimeError('bounded operation exceeded; inspect owned process, do not relaunch')
prepare="$p='C:\\JudasValidation\\m78-lif003-prepare.ps1';Invoke-WebRequest -UseBasicParsing 'http://192.168.122.1:8872/m78-lif003-windows_prepare.ps1' -OutFile $p;& $p"
if run('prepare',prepare):
 for name in ['tags','publication']:
  script="$p='C:\\JudasValidation\\m78-lif003-case.ps1';Invoke-WebRequest -UseBasicParsing 'http://192.168.122.1:8872/m78-lif003-windows_case.ps1' -OutFile $p;& $p -CaseId '"+name+"' -ManifestName 'm78-lif003-v1.json' -OmitMap"
  if not run(name,script):break
# Verify generated output bytes without putting builds into tracked evidence.
verified=[]
for p in e.glob('m78-lif003-*-build.json'):
 m=json.loads(p.read_text(encoding='utf-8-sig'))
 for f in m['generatedFiles']:
  q=cache/('m78-lif003-'+m['case']+'-'+f['name']+'.bin');assert q.stat().st_size==f['bytes']and sha(q)==f['sha256'];verified.append(dict(path=str(q.relative_to(r)),bytes=f['bytes'],sha256=f['sha256']))
(e/'BINARY_VERIFICATION.json').write_text(json.dumps(dict(outputs=verified,scope='Native functional only; Windows outcomes do not gate Linux checkpoint.'),indent=2)+'\n')
