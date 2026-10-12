$ErrorActionPreference='Stop';$ProgressPreference='SilentlyContinue';$root='C:\JudasValidation';$base='http://192.168.122.1:8872';$out="$root\M78ContinuationResults"
function Upload($p,$name){Invoke-WebRequest -UseBasicParsing "$base/$name" -Method Post -InFile $p|Out-Null}
try {
 $m=(Invoke-WebRequest -UseBasicParsing "$base/m78-continuation-direct-combined-v2.json").Content|ConvertFrom-Json
 $scratch="$root\M78ContinuationBuild\$($m.label)";$archive="$scratch\judas_engine.lib"
 if(Test-Path "$out\m78-$($m.label)-library.json"){throw 'Preserve existing library receipt'}
 if((Get-FileHash $m.baselineLibrary.path).Hash.ToLower() -ne $m.baselineLibrary.sha256){throw 'Retained baseline mismatch'}
 foreach($f in $m.files){if((Get-FileHash (Join-Path "$scratch\source" $f.path)).Hash.ToLower() -ne $f.sha256){throw ('Frozen input mismatch '+$f.path)}}
 if(!(Test-Path $archive)){throw 'Previously constructed isolated archive missing'}
 Import-Module "$root\VS2022\Common7\Tools\Microsoft.VisualStudio.DevShell.dll";Enter-VsDevShell -VsInstallPath "$root\VS2022" -SkipAutomaticLocation -DevCmdArguments '-arch=x64 -host_arch=x64'
 $members=@(& lib.exe /nologo /list $archive);if($LASTEXITCODE -ne 0 -or $members.Count -ne $m.baselineMemberCount){throw 'Archive member count mismatch'}
 $rows=@()
 foreach($unit in $m.replacements){
  $found=@($members|Where-Object {[IO.Path]::GetFileName($_) -ieq $unit.objectName});if($found.Count -ne 1){throw 'Duplicate/missing replacement'}
  $object="$scratch\objects\$($unit.objectName)";$rsp="$object.rsp";$log="$out\m78-$($m.label)-$($unit.objectName)-compile.log"
  if(!(Test-Path $object) -or !(Test-Path $rsp) -or !(Test-Path $log)){throw 'Preserved successful compilation artifact missing'}
  $rows+=@{source=$unit.path;sourceSHA256=$unit.sha256;commandResponse=[IO.File]::ReadAllText($rsp);object=$object;objectSHA256=(Get-FileHash $object).Hash.ToLower();exitCode=0;logSHA256=(Get-FileHash $log).Hash.ToLower()}
 }
 $log="$out\m78-$($m.label)-library-empty-success.log";[IO.File]::WriteAllText($log,'')
 Upload $log ([IO.Path]::GetFileName($log))
 $receipt="$out\m78-$($m.label)-library.json";@{scope=$m.scope;label=$m.label;baseline=$m.baselineLibrary;archive=$archive;archiveSHA256=(Get-FileHash $archive).Hash.ToLower();archiveBytes=(Get-Item $archive).Length;memberCount=$members.Count;units=$rows;freeBytes=(Get-PSDrive C).Free;recovery='The three source compilations and archive construction completed. Upload failed because successful lib.exe emitted no text and Tee-Object did not create its log. This receipt independently rechecks every frozen input, object, and archive member; no compilation was repeated.'}|ConvertTo-Json -Depth 7|Set-Content $receipt -Encoding UTF8
 Upload $receipt ([IO.Path]::GetFileName($receipt))
 $owned="$out\combined-v2";if(Test-Path $owned){throw 'Preserve previous v2 execution staging'};New-Item -ItemType Directory $owned -Force|Out-Null
 @('FRAMES 120','LOG_EVERY 30','FRAME_SECONDS 0.016666667')|Set-Content "$owned\startup.harness" -Encoding ASCII
 $nav="$root\M78ContinuationNav-combined-v2";if(Test-Path $nav){throw 'Preserve previous v2 nav fixture'};$zip="$owned\navigation-inputs.zip";Invoke-WebRequest -UseBasicParsing "$base/m78-dependent-fixture.zip" -OutFile $zip
 if((Get-FileHash $zip).Hash.ToLower() -ne '571d2eeb6d467055d2d00eca46a7b81026e1a226228c336b9bc1f9d73fbbb946'){throw 'Original fixture SHA mismatch'};Expand-Archive -LiteralPath $zip -DestinationPath $nav
 @{freeBytes=(Get-PSDrive C).Free;navigationInputSHA256='571d2eeb6d467055d2d00eca46a7b81026e1a226228c336b9bc1f9d73fbbb946';archive=$archive;archiveSHA256=(Get-FileHash $archive).Hash.ToLower()}|ConvertTo-Json|Set-Content "$out\m78-combined-v2-recovery.json" -Encoding UTF8
 Upload "$out\m78-combined-v2-recovery.json" 'm78-combined-v2-recovery.json'
}catch{$p="$out\m78-combined-v2-recovery-error.txt";[string]$_.Exception.Message|Set-Content $p;Upload $p ([IO.Path]::GetFileName($p));exit 1}
