#!/usr/bin/python3
"""Run explicit frozen native cases serially and verify archives before owned cleanup."""
import argparse, base64, hashlib, json, time, zipfile
from pathlib import Path
import libvirt, libvirt_qemu
ROOT=Path.cwd();TRANSFER=ROOT/'.cache/windows-vm-transfer';REPORTS=TRANSFER/'reports';EVIDENCE=ROOT/'docs/evidence/m78-cold-boundary/native-final-cold-repair';BINARY=ROOT/'.cache/m78-cold-boundary/native-binaries/final-repair'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def read(p):return json.loads(p.read_text(encoding='utf-8-sig'))
def guest(script):
 conn=libvirt.open('qemu:///system');vm=conn.lookupByName('win10');code=base64.b64encode(script.encode('utf-16le')).decode()
 request={'execute':'guest-exec','arguments':{'path':'C:\\Windows\\System32\\WindowsPowerShell\\v1.0\\powershell.exe','arg':['-NoProfile','-ExecutionPolicy','Bypass','-EncodedCommand',code],'capture-output':False}}
 pid=json.loads(libvirt_qemu.qemuAgentCommand(vm,json.dumps(request),30,0))['return']['pid'];print('started',pid,flush=True)
 deadline=time.monotonic()+1800
 while time.monotonic()<deadline:
  result=json.loads(libvirt_qemu.qemuAgentCommand(vm,json.dumps({'execute':'guest-exec-status','arguments':{'pid':pid}}),30,0))['return']
  if result['exited']:
   print('completed',pid,result.get('exitcode'),flush=True)
   if result.get('exitcode')!=0:raise RuntimeError('Guest tooling job failed; preserve all outputs: '+str(result))
   return
  time.sleep(5)
 raise RuntimeError('Status deadline reached; preserve running job and inspect, do not spawn duplicate')
def collect():
 for p in REPORTS.glob('m78-final-repair*'):
  if p.suffix not in ['.zip','.bin','.exe','.obj']:
   target=EVIDENCE/p.name
   if target.exists():assert target.read_bytes()==p.read_bytes(),str(target)
   else:target.write_bytes(p.read_bytes())
def main():
 parser=argparse.ArgumentParser();parser.add_argument('--cases',nargs='*');args=parser.parse_args();
 m=read(TRANSFER/'m78-final-cold-repair-v1.json');EVIDENCE.mkdir(exist_ok=True);BINARY.mkdir(exist_ok=True)
 for f in m['files']:assert sha(ROOT/f['path'])==f['sha256'],'Final frozen source changed '+f['path']
 for name in ['windows_final_repair_case.ps1','windows_final_repair_owned_case_cleanup.ps1']:
  (TRANSFER/name).write_bytes((ROOT/'docs/evidence/m78-cold-boundary'/name).read_bytes())
 for case in m['cases']:
  cid=case['id']
  if args.cases and cid not in args.cases:continue
  print('CASE',cid,flush=True)
  assert not (REPORTS/f'm78-final-repair-{cid}-build.json').exists(),'Preserve previous case'
  guest("$p='C:\\JudasValidation\\m78-final-case.ps1'; Invoke-WebRequest -UseBasicParsing 'http://192.168.122.1:8872/windows_final_repair_case.ps1' -OutFile $p; & $p -CaseId '"+cid+"'\n")
  receipt=read(REPORTS/f'm78-final-repair-{cid}-archive.json');archive=REPORTS/receipt['archiveName'];assert archive.stat().st_size==receipt['archiveBytes'] and sha(archive)==receipt['archiveSHA256']
  with zipfile.ZipFile(archive) as z:
   assert set(z.namelist())=={f['name'] for f in receipt['files']}
   for f in receipt['files']:
    data=z.read(f['name']);assert len(data)==f['bytes'] and hashlib.sha256(data).hexdigest()==f['sha256'];target=BINARY/f['name'];assert not target.exists(),'Preserve prior output';target.write_bytes(data)
  build=read(REPORTS/f'm78-final-repair-{cid}-build.json')
  assert build['executableSHA256']==next(f['sha256'] for f in receipt['files'] if f['path']==build['executable'])
  for r in build['runs']:
   expected=1 if r['run'] in ['balance-exact-cold-read','character-cold-read'] else 0
   print('RESULT',r['run'],r['exitCode'],'expected',expected,flush=True)
   if r['exitCode']!=expected:raise RuntimeError('Unexpected actual numerical/functional result preserved; no cleanup or next case')
  approved={'scope':'Fresh native final case, all generated member bytes independently host SHA/size verified before exact guest cleanup','hostVerified':True,'case':cid,'files':receipt['files']+[{'path':receipt['archivePath'],'bytes':receipt['archiveBytes'],'sha256':receipt['archiveSHA256']}]}
  approval=f'm78-final-repair-{cid}-owned-approval.json';(TRANSFER/approval).write_text(json.dumps(approved,indent=2)+'\n');(EVIDENCE/(approval.replace('owned-approval','host-verification'))).write_text(json.dumps(approved,indent=2)+'\n');collect()
  guest("$p='C:\\JudasValidation\\m78-final-case-cleanup.ps1'; Invoke-WebRequest -UseBasicParsing 'http://192.168.122.1:8872/windows_final_repair_owned_case_cleanup.ps1' -OutFile $p; & $p -ApprovalName '"+approval+"'\n");collect()
 print('Frozen focused native matrix complete; no performance workload.',flush=True)
if __name__=='__main__':main()
