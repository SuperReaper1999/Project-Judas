$ErrorActionPreference='Stop';$ProgressPreference='SilentlyContinue'
$root='C:\JudasValidation';$base='http://192.168.122.1:8872';$name='m78-final-current-standing'
$out="$root\M78FinalCurrentStandingResults";$scratch="$root\M78FinalCurrentStandingBuild"
function Upload($p,$n){Invoke-WebRequest -UseBasicParsing "$base/$n" -Method Post -InFile $p|Out-Null}
try {
 if(Test-Path $scratch){throw 'Preserve previous observer scratch'}
 if((Get-PSDrive C).Free -lt 22MB){throw 'Insufficient owned observer headroom'}
 New-Item -ItemType Directory $out,$scratch|Out-Null
 $proof=(Invoke-WebRequest -UseBasicParsing "$base/m78-final-current-observer-manifest.json").Content|ConvertFrom-Json
 $m=(Invoke-WebRequest -UseBasicParsing "$base/m78-final-cold-repair-v2.json").Content|ConvertFrom-Json
 $case=@($m.cases|Where-Object {$_.id -eq 'balance_lab'})[0]
 $library=[IO.File]::ReadAllText("$($m.output)\m78-final2-repair-library.json")|ConvertFrom-Json
 if($library.archiveSHA256 -ne $proof.finalNativeLibrarySHA256 -or (Get-FileHash $library.archive).Hash.ToLower() -ne $proof.finalNativeLibrarySHA256){throw 'Final native archive changed'}
 $zip="$scratch\source.zip";Invoke-WebRequest -UseBasicParsing "$base/$($proof.archiveName)" -OutFile $zip
 if((Get-FileHash $zip).Hash.ToLower() -ne $proof.archiveSHA256){throw 'Observer bundle mismatch'}
 Expand-Archive $zip "$scratch\source"
 $source="$scratch\source\FinalCurrentStandingObserver.cpp"
 if((Get-FileHash $source).Hash.ToLower() -ne $proof.observerSHA256){throw 'Observer source mismatch'}
 Import-Module "$root\VS2022\Common7\Tools\Microsoft.VisualStudio.DevShell.dll"
 Enter-VsDevShell -VsInstallPath "$root\VS2022" -SkipAutomaticLocation -DevCmdArguments '-arch=x64 -host_arch=x64'
 Set-Location "$root\Build";$obj="$scratch\FinalCurrentStandingObserver.obj"
 $flags=[string]$case.compilerFlags
 $flags=[regex]::Replace($flags,'/Fo"[^"]*"',('/Fo"'+$obj+'"'),'IgnoreCase')
 $flags=[regex]::Replace($flags,'/Fd"[^"]*"',('/Fd"'+$scratch+'\compile.pdb"'),'IgnoreCase')
 $flags='/I"'+$m.scratch+'\source\src" /I"'+$root+'\Source\third_party" '+$flags+' "'+$source+'"'
 $rsp="$scratch\compile.rsp";[IO.File]::WriteAllText($rsp,$flags,[Text.UTF8Encoding]::new($false))
 $log="$out\$name-compile.log";[IO.File]::WriteAllText($log,'');$ErrorActionPreference='Continue'
 & cl.exe "@$rsp" 2>&1|Tee-Object -FilePath $log;$code=$LASTEXITCODE;Upload $log ([IO.Path]::GetFileName($log))
 if($code -ne 0){throw 'Read-only observer compile failed'}
 $exe="$root\Build\Release\$name.exe";if(Test-Path $exe){throw 'Preserve previous observer executable'}
 $link=[string]$case.linkFlags
 $link=[regex]::Replace($link,'/OUT:"[^"]*"',('/OUT:"'+$exe+'"'),'IgnoreCase')
 $link=[regex]::Replace($link,'RELEASE\\JUDAS_ENGINE\.LIB',('"'+$library.archive+'"'),'IgnoreCase')
 $link=[regex]::Replace($link,'/PDB:"[^"]*"',('/PDB:"'+$scratch+'\link.pdb"'),'IgnoreCase')
 $link=[regex]::Replace($link,'/IMPLIB:"[^"]*"',('/IMPLIB:"'+$scratch+'\observer.lib"'),'IgnoreCase')
 $link+=' "'+$obj+'"';$rsp="$scratch\link.rsp";[IO.File]::WriteAllText($rsp,$link,[Text.UTF8Encoding]::new($false))
 $log="$out\$name-link.log";[IO.File]::WriteAllText($log,'');& link.exe "@$rsp" 2>&1|Tee-Object -FilePath $log;$code=$LASTEXITCODE;Upload $log ([IO.Path]::GetFileName($log))
 if($code -ne 0){throw 'Read-only observer link failed'}
 $ErrorActionPreference='Stop'
 foreach($e in Get-ChildItem Env:|Where-Object {$_.Name -like 'JUDAS_DIAGNOSTIC_*' -or $_.Name -like 'JUDAS_BOUNDARY_*' -or $_.Name -like 'M78_DIAGNOSTIC_*' -or $_.Name -eq 'JUDAS_TEST_SCRIPT'}){[Environment]::SetEnvironmentVariable($e.Name,$null,'Process')}
 $env:GALLIUM_DRIVER='llvmpipe';$env:JUDAS_ENGINE_ROOT="$root\Source";$env:TEMP="$out\temp";$env:TMP=$env:TEMP;New-Item -ItemType Directory $env:TEMP|Out-Null
 $data="$out\case";$rows=@();$savedSHA=''
 foreach($mode in @('cold-write','cold-read')) {
  $label=if($mode -eq 'cold-write'){'source'}else{'restored'}
  if($mode -eq 'cold-write'){$env:JUDAS_DIAGNOSTIC_CONTINUE_WRITE='1'}else{[Environment]::SetEnvironmentVariable('JUDAS_DIAGNOSTIC_CONTINUE_WRITE',$null,'Process')}
  $watch=[Diagnostics.Stopwatch]::StartNew()
  $p=Start-Process -FilePath $exe -WorkingDirectory "$root\Source" -ArgumentList @($data,$mode) -PassThru -RedirectStandardOutput "$out\$name-$label.stdout.log" -RedirectStandardError "$out\$name-$label.stderr.log";[void]$p.Handle
  if(!$p.WaitForExit(600000)){Stop-Process -Id $p.Id -Force;throw 'Owned observer timeout'};$p.Refresh();$exit=$p.ExitCode;$watch.Stop()
  Upload "$out\$name-$label.stdout.log" "$name-$label.stdout.log";Upload "$out\$name-$label.stderr.log" "$name-$label.stderr.log"
  $dest="$out\$label";New-Item -ItemType Directory $dest|Out-Null
  $files=@();foreach($f in @(Get-ChildItem $data -File|Where-Object {$_.Extension -in '.json','.jsonl'})+@(Get-ChildItem "$data\slots" -File)) {
   $copy="$dest\$($f.Name)";Copy-Item $f.FullName $copy;$report="$name-$label-$($f.Name)";Upload $copy $report
   $files+=@{path=$copy;name=$report;bytes=(Get-Item $copy).Length;sha256=(Get-FileHash $copy).Hash.ToLower()}
  }
  $save=(Get-FileHash "$data\slots\m77.save").Hash.ToLower()
  if($mode -eq 'cold-write'){$savedSHA=$save}elseif($save -ne $savedSHA){throw 'Original saved bytes changed during read'}
  $rows+=@{mode=$mode;exitCode=$exit;seconds=$watch.Elapsed.TotalSeconds;files=$files;saveSHA256=$save}
 }
 $rp="$out\$name.json";@{scope='Read-only final current body/History/completed-contact endpoint witness. No engine object substitution or runtime mutation/drive replay; original24/13 acceptance unchanged. Both runs share exact newly captured save bytes. WindowsVM functional-only.';archiveSHA256=$library.archiveSHA256;sourceSHA256=$proof.observerSHA256;sourceBundleSHA256=$proof.archiveSHA256;exe=$exe;exeSHA256=(Get-FileHash $exe).Hash.ToLower();object=$obj;objectSHA256=(Get-FileHash $obj).Hash.ToLower();compilerResponse=$flags;linkerResponse=$link;runs=$rows;freeBytes=(Get-PSDrive C).Free}|ConvertTo-Json -Depth 8|Set-Content $rp -Encoding UTF8;Upload $rp ([IO.Path]::GetFileName($rp))
 $zip="$scratch\$name-generated.zip";Add-Type -AssemblyName System.IO.Compression;Add-Type -AssemblyName System.IO.Compression.FileSystem;$z=[IO.Compression.ZipFile]::Open($zip,[IO.Compression.ZipArchiveMode]::Create);try{foreach($p in @($exe,$obj)){[void][IO.Compression.ZipFileExtensions]::CreateEntryFromFile($z,$p,[IO.Path]::GetFileName($p),[IO.Compression.CompressionLevel]::Optimal)}}finally{$z.Dispose()}
 Upload $zip ([IO.Path]::GetFileName($zip));$files=@();foreach($p in @($exe,$obj)){$files+=@{path=$p;name=[IO.Path]::GetFileName($p);bytes=(Get-Item $p).Length;sha256=(Get-FileHash $p).Hash.ToLower()}}
 $rp="$out\$name-archive.json";@{scope='Archive only; generated copies require host SHA verification before cleanup';archiveName=[IO.Path]::GetFileName($zip);archivePath=$zip;archiveSHA256=(Get-FileHash $zip).Hash.ToLower();archiveBytes=(Get-Item $zip).Length;files=$files}|ConvertTo-Json -Depth 5|Set-Content $rp -Encoding UTF8;Upload $rp ([IO.Path]::GetFileName($rp))
}catch{New-Item -ItemType Directory $out -Force|Out-Null;[string]$_.Exception.Message|Set-Content "$out\$name-error.txt";Upload "$out\$name-error.txt" "$name-error.txt";exit 1}
