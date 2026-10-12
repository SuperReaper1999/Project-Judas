#!/usr/bin/python3
"""Serial named native operation; no tests start without --go. Immutable host receipts."""
import json,base64,time,hashlib,argparse,shutil
from pathlib import Path
import libvirt,libvirt_qemu
R=Path.cwd();T=R/'.cache/windows-vm-transfer';P=T/'reports';E=R/'docs/evidence/m78-closure/native';B=R/'.cache/m78-closure/native-binaries'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def rd(p):return json.loads(p.read_text(encoding='utf-8-sig'))
def store(src,dst):
 dst.parent.mkdir(parents=True,exist_ok=True)
 if dst.exists():assert dst.read_bytes()==src.read_bytes(),str(dst)
 else:shutil.copyfile(src,dst)
def guest(script):
 conn=libvirt.open('qemu:///system');vm=conn.lookupByUUIDString('877d6b10-f85b-4108-9d07-11953f247974');code=base64.b64encode(script.encode('utf-16le')).decode();req={'execute':'guest-exec','arguments':{'path':r'C:\Windows\System32\WindowsPowerShell\v1.0\powershell.exe','arg':['-NoProfile','-ExecutionPolicy','Bypass','-EncodedCommand',code],'capture-output':False}}
 reply=json.loads(libvirt_qemu.qemuAgentCommand(vm,json.dumps(req),30,0));assert 'return'in reply,reply;pid=reply['return']['pid'];print('started',pid,flush=True);deadline=time.monotonic()+2400
 while time.monotonic()<deadline:
  result=json.loads(libvirt_qemu.qemuAgentCommand(vm,json.dumps({'execute':'guest-exec-status','arguments':{'pid':pid}}),30,0))['return']
  if result['exited']:
   print('completed',pid,result.get('exitcode'),flush=True);collect();assert result.get('exitcode')==0,result;return
  time.sleep(5)
 raise RuntimeError('Guest operation remains active at deadline; inspect, do not relaunch')
def collect():
 for src in P.glob('m78-closure-*'):
  if src.suffix not in ['.bin','.exe','.obj','.zip']:store(src,E/src.name)
def check_generated(case):
 m=rd(P/f'm78-closure-{case}-build.json');approved=[]
 for row in m['generatedFiles']:
  src=P/('m78-closure-'+case+'-'+row['name']+'.bin');assert src.stat().st_size==row['bytes'] and sha(src)==row['sha256'];store(src,B/case/row['name']);approved.append(row)
 large=m.get('ownedLargeEvidence',[]);large=[large]if isinstance(large,dict)else large
 for row in large:
  src=E/row['name'];assert src.stat().st_size==row['bytes'] and sha(src)==row['sha256'];approved.append(row)
 for run in m['runs']:
  print('RESULT',run['run'],run['exitCode'],'expected',run['expectedExitCode'],flush=True)
  assert run['exitCode']==run['expectedExitCode'],run
 approval={'scope':'Independent host SHA/size verification of exact named generated case before retirement','hostVerified':True,'case':case,'files':approved};name=f'm78-closure-{case}-approval.json';(T/name).write_text(json.dumps(approval,indent=2)+'\n');store(T/name,E/name);return name
def main():
 a=argparse.ArgumentParser();a.add_argument('mode',choices=['prepare','cases','editor']);a.add_argument('--go',action='store_true');a.add_argument('--cases',nargs='*');a.add_argument('--manifest',default='m78-closure-v1.json');a.add_argument('--reuse-objects',action='store_true');a.add_argument('--omit-map',action='store_true');v=a.parse_args();assert v.go,'Explicit root CPU GO required'
 assert v.manifest.startswith('m78-closure-') and '/'not in v.manifest; m=rd(T/v.manifest);
 for f in m['files']:assert sha(R/f['path'])==f['sha256'],'Prepared input changed '+f['path']
 for src in (E/'probes').glob('windows_*.ps1'):
  dst=T/('m78-closure-'+src.name)
  if dst.exists()and dst.read_bytes()!=src.read_bytes():
   store(dst,E/'probes/historical-tools'/(sha(dst)+'-'+dst.name));shutil.copyfile(src,dst)
  else:store(src,dst)
 if v.mode=='prepare':
  guest("$p='C:\\JudasValidation\\m78-closure-prepare.ps1';Invoke-WebRequest -UseBasicParsing 'http://192.168.122.1:8872/m78-closure-windows_prepare.ps1' -OutFile $p;& $p")
  engine=rd(P/'m78-closure-engine.json');
  for row in engine['objects']:
   src=P/('m78-closure-'+Path(row['path'].replace('\\','/')).name+'.bin');assert src.stat().st_size==row['bytes'] and sha(src)==row['sha256'];store(src,B/Path(row['path'].replace('\\','/')).name)
 elif v.mode=='editor':
  guest("$p='C:\\JudasValidation\\m78-closure-editor.ps1';Invoke-WebRequest -UseBasicParsing 'http://192.168.122.1:8872/m78-closure-windows_editor_v3.ps1' -OutFile $p;& $p")
  name=check_generated('editor_v3');guest("$p='C:\\JudasValidation\\m78-closure-cleanup.ps1';Invoke-WebRequest -UseBasicParsing 'http://192.168.122.1:8872/m78-closure-windows_cleanup.ps1' -OutFile $p;& $p -ApprovalName '"+name+"'")
 else:
  for c in m['cases']:
   cid=c['id']
   if v.cases and cid not in v.cases:continue
   assert not(P/f'm78-closure-{cid}-build.json').exists(),'Preserve existing case';print('CASE',cid,flush=True)
   guest("$p='C:\\JudasValidation\\m78-closure-case.ps1';Invoke-WebRequest -UseBasicParsing 'http://192.168.122.1:8872/m78-closure-windows_case.ps1' -OutFile $p;& $p -CaseId '"+cid+"' -ManifestName '"+v.manifest+"'"+(' -ReuseCaseObjects'if v.reuse_objects else '')+(' -OmitMap'if v.omit_map else '') )
   name=check_generated(cid);guest("$p='C:\\JudasValidation\\m78-closure-cleanup.ps1';Invoke-WebRequest -UseBasicParsing 'http://192.168.122.1:8872/m78-closure-windows_cleanup.ps1' -OutFile $p;& $p -ApprovalName '"+name+"'")
 collect();print('Named native functional operation complete; no performance workload',flush=True)
if __name__=='__main__':main()
