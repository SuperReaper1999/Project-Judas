param([string]$ManifestName='',[string]$CaseId='')
$ErrorActionPreference='Stop';$ProgressPreference='SilentlyContinue';$root='C:\JudasValidation';$base='http://192.168.122.1:8872';$out="$root\M78ContinuationResults"
function Upload($p,$name){Invoke-WebRequest -UseBasicParsing "$base/$name" -Method Post -InFile $p|Out-Null}
try {
 if($ManifestName -notmatch '^m78-continuation-direct-[a-z0-9_-]+\.json$' -or $CaseId -notmatch '^[a-z0-9_-]+$'){throw 'Explicit manifest and case required'}
 $m=(Invoke-WebRequest -UseBasicParsing "$base/$ManifestName").Content|ConvertFrom-Json;$case=@($m.cases|Where-Object {$_.id -eq $CaseId});if($case.Count -ne 1){throw 'Unique manifest case required'};$case=$case[0]
 $scratch="$root\M78ContinuationBuild\$($m.label)";$archive="$scratch\judas_engine.lib";$receiptName="m78-$($m.label)-$CaseId"
 if(Test-Path "$out\$receiptName.json"){throw 'Preserve preceding native case receipt'}
 if((Get-PSDrive C).Free -lt $m.minimumFreeBeforeCaseBytes){throw 'Insufficient bounded case headroom'}
 $library=Get-Content "$out\m78-$($m.label)-library.json" -Raw|ConvertFrom-Json;if((Get-FileHash $archive).Hash.ToLower() -ne $library.archiveSHA256){throw 'Isolated candidate library changed'}
 foreach($input in $case.sources){$source=([string]$input.sourcePath).Replace('@SOURCE@',"$scratch\source");if((Get-FileHash -LiteralPath $source).Hash.ToLower() -ne $input.sha256){throw ('Original case input SHA mismatch '+$source)}}
 Import-Module "$root\VS2022\Common7\Tools\Microsoft.VisualStudio.DevShell.dll";Enter-VsDevShell -VsInstallPath "$root\VS2022" -SkipAutomaticLocation -DevCmdArguments '-arch=x64 -host_arch=x64';Set-Location "$root\Build"
 $exe="$root\Build\Release\m78_$($m.label)_$CaseId.exe";if(Test-Path $exe){throw 'Preserve preceding generated executable'};$objects=@();$compiles=@();$index=0;$ErrorActionPreference='Continue'
 foreach($input in $case.sources){
  $source=([string]$input.sourcePath).Replace('@SOURCE@',"$scratch\source");$object="$scratch\objects\$CaseId-$index.obj"
  $flags=[string]$case.compilerFlags;$flags=[regex]::Replace($flags,'/Fo"[^"]*"',('/Fo"'+$object+'"'),[Text.RegularExpressions.RegexOptions]::IgnoreCase);$flags=[regex]::Replace($flags,'/Fd"[^"]*"',('/Fd"'+$scratch+'\objects\case.pdb"'),[Text.RegularExpressions.RegexOptions]::IgnoreCase)
  $flags='/I"'+$scratch+'\source\src" '+$flags+' "'+$source+'"';$rsp="$scratch\objects\$CaseId-$index-compile.rsp";[IO.File]::WriteAllText($rsp,$flags,[Text.UTF8Encoding]::new($false));$log="$out\$receiptName-$index-compile.log"
  & cl.exe "@$rsp" 2>&1|Tee-Object -FilePath $log;$code=$LASTEXITCODE;Upload $log ([IO.Path]::GetFileName($log));if($code -ne 0){throw ('Case compilation failed '+$source)}
  $objects+=$object;$compiles+=@{source=$source;sourceSHA256=$input.sha256;object=$object;objectSHA256=(Get-FileHash $object).Hash.ToLower();compilerResponse=$flags};$index++
 }
 $link=[string]$case.linkFlags;$link=[regex]::Replace($link,'/OUT:"[^"]*"',('/OUT:"'+$exe+'"'),[Text.RegularExpressions.RegexOptions]::IgnoreCase);$link=[regex]::Replace($link,'RELEASE\\JUDAS_ENGINE\.LIB',('"'+$archive+'"'),[Text.RegularExpressions.RegexOptions]::IgnoreCase);$link=[regex]::Replace($link,'/PDB:"[^"]*"',('/PDB:"'+$scratch+'\objects\case-link.pdb"'),[Text.RegularExpressions.RegexOptions]::IgnoreCase);$link=[regex]::Replace($link,'/IMPLIB:"[^"]*"',('/IMPLIB:"'+$scratch+'\objects\case-import.lib"'),[Text.RegularExpressions.RegexOptions]::IgnoreCase);foreach($object in $objects){$link+=' "'+$object+'"'}
 $linkRsp="$scratch\objects\$CaseId-link.rsp";[IO.File]::WriteAllText($linkRsp,$link,[Text.UTF8Encoding]::new($false));& link.exe "@$linkRsp" 2>&1|Tee-Object -FilePath "$out\$receiptName-link.log";$code=$LASTEXITCODE;Upload "$out\$receiptName-link.log" "$receiptName-link.log";if($code -ne 0){throw 'Case link failed'}
 $env:TEMP="$out\temp";$env:TMP=$env:TEMP;New-Item -ItemType Directory $env:TEMP -Force|Out-Null
 $options=@{FilePath=$exe;WorkingDirectory="$root\Source";PassThru=$true;RedirectStandardOutput="$out\$receiptName.stdout.log";RedirectStandardError="$out\$receiptName.stderr.log"};if($case.arguments.Count){$options.ArgumentList=@($case.arguments)}
 $watch=[Diagnostics.Stopwatch]::StartNew();$process=Start-Process @options;[void]$process.Handle;if(!$process.WaitForExit($case.timeoutMilliseconds)){Stop-Process -Id $process.Id -Force;$exit='owned case timed out'}else{$process.Refresh();$exit=$process.ExitCode}
 $receipt="$out\$receiptName.json";@{case=$CaseId;label=$m.label;exitCode=$exit;seconds=$watch.Elapsed.TotalSeconds;compiles=$compiles;archiveSHA256=$library.archiveSHA256;executable=$exe;executableSHA256=(Get-FileHash $exe).Hash.ToLower();arguments=@($case.arguments);linkerResponse=$link;freeBytes=(Get-PSDrive C).Free}|ConvertTo-Json -Depth 7|Set-Content $receipt -Encoding UTF8
 Upload "$out\$receiptName.stdout.log" "$receiptName.stdout.log";Upload "$out\$receiptName.stderr.log" "$receiptName.stderr.log";Upload $receipt "$receiptName.json"
 # Retain executable/object until independent host archival and root-coordinated cleanup.
}catch {
 $name='m78-direct-case-'+$CaseId+'-error.txt';[string]$_.Exception.Message|Set-Content "$out\$name";Upload "$out\$name" $name;exit 1
}
