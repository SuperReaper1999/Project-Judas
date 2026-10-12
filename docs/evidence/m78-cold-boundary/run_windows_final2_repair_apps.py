#!/usr/bin/python3
"""Fresh final native editor and ordinary moved-package functional smokes."""
import json,sys,zipfile,hashlib
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parent))
from run_windows_final2_repair_matrix import guest,collect,read,sha,TRANSFER,REPORTS,EVIDENCE,BINARY

def archive_and_cleanup(label,receipt_name):
 receipt=read(REPORTS/receipt_name);archive=REPORTS/receipt['archiveName'];assert archive.stat().st_size==receipt['archiveBytes'] and sha(archive)==receipt['archiveSHA256']
 with zipfile.ZipFile(archive) as z:
  assert set(z.namelist())=={f['name'] for f in receipt['files']}
  for f in receipt['files']:
   data=z.read(f['name']);assert len(data)==f['bytes'] and hashlib.sha256(data).hexdigest()==f['sha256'];p=BINARY/f['name'];assert not p.exists() or p.read_bytes()==data;p.write_bytes(data)
 approved={'scope':'Only independently fullhostSHAverified completed generated app copies; ordinary movedpackage and source/rawsaves untouched','hostVerified':True,'case':label,'files':receipt['files']+[{'path':receipt['archivePath'],'bytes':receipt['archiveBytes'],'sha256':receipt['archiveSHA256']}]}
 name=f'm78-final2-repair-{label}-owned-approval.json';(TRANSFER/name).write_text(json.dumps(approved,indent=2)+'\n');(EVIDENCE/name.replace('owned-approval','host-verification')).write_text(json.dumps(approved,indent=2)+'\n');collect()
 guest("$p='C:\\JudasValidation\\m78-final2-app-cleanup.ps1';Invoke-WebRequest -UseBasicParsing 'http://192.168.122.1:8872/windows_final2_repair_owned_case_cleanup.ps1' -OutFile $p;& $p -ApprovalName '"+name+"'");collect()

def main():
 EVIDENCE.mkdir(exist_ok=True);BINARY.mkdir(exist_ok=True)
 library=read(REPORTS/'m78-final2-repair-library.json');m=read(TRANSFER/'m78-final-cold-repair-v2.json')
 for f in m['files']:assert sha(Path.cwd()/f['path'])==f['sha256'],'Final source changed'
 for n in ['windows_final2_repair_editor.ps1','windows_final2_repair_export_smoke.ps1','windows_final2_repair_owned_case_cleanup.ps1']:(TRANSFER/n).write_bytes((Path(__file__).parent/n).read_bytes())
 runtime=read(REPORTS/'m78-final2-repair-runtime-build.json');p=BINARY/'m78-final2-repair-runtime.exe';assert sha(p)==runtime['executableSHA256'] and runtime['librarySHA256']==library['archiveSHA256'];(TRANSFER/p.name).write_bytes(p.read_bytes());(TRANSFER/'m78-final2-repair-runtime-download.json').write_text(json.dumps({'url':'http://192.168.122.1:8872/'+p.name,'sha256':sha(p),'bytes':p.stat().st_size,'librarySHA256':library['archiveSHA256']},indent=2)+'\n')
 assert not (REPORTS/'m78-final2-repair-editor-build.json').exists(),'Preserve previous editor'
 guest("$p='C:\\JudasValidation\\m78-final2-editor.ps1';Invoke-WebRequest -UseBasicParsing 'http://192.168.122.1:8872/windows_final2_repair_editor.ps1' -OutFile $p;& $p")
 r=read(REPORTS/'m78-final2-repair-editor-smoke.json');assert r['exitCode']==0 and r['sceneUnchanged'] and r['librarySHA256']==library['archiveSHA256'];assert len(r['files'])==3
 for f in r['files']:assert (REPORTS/f['name']).stat().st_size==f['bytes'] and sha(REPORTS/f['name'])==f['sha256']
 archive_and_cleanup('editor','m78-final2-repair-editor-archive.json');print('EDITOR Play/Stop PASS',flush=True)
 assert not (REPORTS/'m78-final2-repair-moved-package.json').exists(),'Preserve previous package'
 guest("$p='C:\\JudasValidation\\m78-final2-export-smoke.ps1';Invoke-WebRequest -UseBasicParsing 'http://192.168.122.1:8872/windows_final2_repair_export_smoke.ps1' -OutFile $p;& $p")
 r=read(REPORTS/'m78-final2-repair-moved-package.json');assert r['exportExitCode']==0 and r['exitCode']==0 and r['archiveSHA256']==library['archiveSHA256'] and r['runtimeSHA256']==runtime['executableSHA256']
 log=(REPORTS/'m78-final2-repair-moved-package.stdout.log').read_text(encoding='utf-8-sig');assert log.count('service scene ready')>=4 and log.count('service save ready')>=2
 assert not any(s in log for s in ['[TestHarness] FAIL','Script error','TypeError:','Segmentation fault'])
 archive_and_cleanup('export','m78-final2-repair-export-archive.json');print('MOVED PACKAGE four-scenes/save/load/pause/reload PASS',flush=True)
if __name__=='__main__':main()
