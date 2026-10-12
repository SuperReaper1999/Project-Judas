#!/usr/bin/python3
"""Run the single approved WP replacement; losslessly preserve both archives."""
import hashlib,json,sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parent))
from run_windows_final2_repair_matrix import guest,collect,read,sha,TRANSFER,REPORTS,EVIDENCE,BINARY

def main():
 EVIDENCE.mkdir(exist_ok=True);BINARY.mkdir(exist_ok=True)
 m=read(TRANSFER/'m78-final-cold-repair-v2.json')
 for f in m['files']:assert sha(Path.cwd()/f['path'])==f['sha256'],'Source changed '+f['path']
 for name in ['windows_final2_repair_library.ps1','windows_final2_repair_archive_library.ps1']:
  (TRANSFER/name).write_bytes((Path(__file__).parent/name).read_bytes())
 assert not (REPORTS/'m78-final2-repair-library.json').exists(),'Preserve prior archive receipt'
 guest("$p='C:\\JudasValidation\\m78-final2-library.ps1'; Invoke-WebRequest -UseBasicParsing 'http://192.168.122.1:8872/windows_final2_repair_library.ps1' -OutFile $p; & $p")
 library=read(REPORTS/'m78-final2-repair-library.json');assert library['memberCount']==138
 assert library['baseline']['sha256']=='3554c52ca3f8961ca61551074d616f24d2fe0024ecae75f17aef97431e6de83e'
 assert library['units'][0]['sourceSHA256']==m['replacements'][0]['sha256']
 guest("$p='C:\\JudasValidation\\m78-final2-library-archive.ps1'; Invoke-WebRequest -UseBasicParsing 'http://192.168.122.1:8872/windows_final2_repair_archive_library.ps1' -OutFile $p; & $p")
 receipt=read(REPORTS/'m78-final2-repair-library-archive.json');assert receipt['sha256']==library['archiveSHA256']
 target=BINARY/'judas_engine.lib';assert not target.exists(),'Preserve previous target'
 with target.open('wb') as out:
  for part in receipt['parts']:
   p=REPORTS/part['reportName'];assert p.stat().st_size==part['bytes'] and sha(p)==part['sha256'];out.write(p.read_bytes())
 assert target.stat().st_size==receipt['bytes'] and sha(target)==receipt['sha256']
 old=Path.cwd()/'.cache/m78-cold-boundary/native-binaries/final-repair/judas_engine.lib'
 assert old.stat().st_size==67015480 and sha(old)==library['baseline']['sha256']
 verification={'scope':'New WP-only138member archive and exact previous full archive independently preserved on host; retire only redundant previous guest archive after user/root authorization. Sources, raw evidence, old snapshots, packages remain untouched.','new':{'hostPath':str(target),'bytes':target.stat().st_size,'sha256':sha(target)},'previous':{'hostPath':str(old),'bytes':old.stat().st_size,'sha256':sha(old)},'libraryReceipt':library,'archiveReceipt':receipt}
 (EVIDENCE/'LIBRARIES_HOST_VERIFICATION.json').write_text(json.dumps(verification,indent=2)+'\n');collect()
 script="$ErrorActionPreference='Stop';$old='"+library['baseline']['path']+"';$new='"+library['archive']+"';if((Get-Item $old).Length -ne 67015480 -or (Get-FileHash $old).Hash.ToLower() -ne '"+library['baseline']['sha256']+"'){throw 'Previous guest archive changed'};if((Get-FileHash $new).Hash.ToLower() -ne '"+library['archiveSHA256']+"'){throw 'New archive changed'};$before=(Get-PSDrive C).Free;Remove-Item -LiteralPath $old;$out='C:\\JudasValidation\\M78ColdRepairFinalV2Results\\m78-final2-repair-old-library-retired.json';@{scope='Only redundant previous guest3554 archive removed after two independently verified full host copies; no source/raw evidence/save/package removal';removed=$old;removedSHA256='"+library['baseline']['sha256']+"';newSHA256='"+library['archiveSHA256']+"';freeBefore=$before;freeAfter=(Get-PSDrive C).Free}|ConvertTo-Json|Set-Content $out -Encoding UTF8;Invoke-WebRequest -UseBasicParsing 'http://192.168.122.1:8872/m78-final2-repair-old-library-retired.json' -Method Post -InFile $out|Out-Null"
 guest(script);collect();print(json.dumps(verification['new'],indent=2),flush=True)
if __name__=='__main__':main()
