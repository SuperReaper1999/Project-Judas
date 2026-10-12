param([string]$ManifestName='')
$ErrorActionPreference='Stop';$ProgressPreference='SilentlyContinue';$root='C:\JudasValidation';$base='http://192.168.122.1:8872';$out="$root\M78ContinuationResults"
function Upload($p,$name){Invoke-WebRequest -UseBasicParsing "$base/$name" -Method Post -InFile $p|Out-Null}
try {
 if($ManifestName -notmatch '^m78-continuation-direct-[a-z0-9_-]+\.json$'){throw 'Explicit prepared manifest filename required'}
 $m=(Invoke-WebRequest -UseBasicParsing "$base/$ManifestName").Content|ConvertFrom-Json
 if($m.label -notmatch '^[a-z0-9_-]+$'){throw 'Invalid isolated label'}
 $scratch="$root\M78ContinuationBuild\$($m.label)";if(!(Test-Path $scratch)){throw 'Exact preceding owned source staging missing'};if(Test-Path "$scratch\judas_engine.lib"){throw 'Preserve preceding isolated archive'}
 if((Get-PSDrive C).Free -lt $m.minimumFreeBeforeLibraryBytes){throw 'Insufficient isolated-library headroom including untouched reserve'}
 if((Get-FileHash -LiteralPath $m.baselineLibrary.path).Hash.ToLower() -ne $m.baselineLibrary.sha256){throw 'Baseline archive changed'}
 foreach($file in $m.files){if((Get-FileHash -LiteralPath (Join-Path "$scratch\source" $file.path)).Hash.ToLower() -ne $file.sha256){throw ('Input SHA mismatch '+$file.path)}}
 Import-Module "$root\VS2022\Common7\Tools\Microsoft.VisualStudio.DevShell.dll";Enter-VsDevShell -VsInstallPath "$root\VS2022" -SkipAutomaticLocation -DevCmdArguments '-arch=x64 -host_arch=x64'
 Set-Location "$root\Build"; $rows=@();$objects=@();$remove=@();$ErrorActionPreference='Continue'
 foreach($unit in $m.replacements){
  $source=Join-Path "$scratch\source" $unit.path;$object=Join-Path "$scratch\objects" $unit.objectName
  $flags='/I"C:\JudasValidation\Source\third_party" '+[string]$unit.flags
  $flags=[regex]::Replace($flags,'/Fo"[^"]*"',('/Fo"'+$object+'"'),[Text.RegularExpressions.RegexOptions]::IgnoreCase)
  $flags=[regex]::Replace($flags,'/Fd"[^"]*"',('/Fd"'+$scratch+'\objects\compile.pdb"'),[Text.RegularExpressions.RegexOptions]::IgnoreCase)
  $flags=[regex]::Replace($flags,'\s+C:\\JUDASVALIDATION\\SOURCE\\[^\r\n]*\.CPP\s*$','',[Text.RegularExpressions.RegexOptions]::IgnoreCase)
  $flags='/I"'+$scratch+'\source\src" '+$flags+' "'+$source+'"'
  $rsp="$scratch\objects\$($unit.objectName).rsp";[IO.File]::WriteAllText($rsp,$flags,[Text.UTF8Encoding]::new($false));$log="$out\m78-$($m.label)-include-recovery-$($unit.objectName)-compile.log"
  & cl.exe "@$rsp" 2>&1|Tee-Object -FilePath $log;$code=$LASTEXITCODE;Upload $log ([IO.Path]::GetFileName($log));if($code -ne 0){throw ('Narrow translation unit failed '+$unit.path)}
  $rows+=@{source=$unit.path;sourceSHA256=$unit.sha256;commandResponse=$flags;object=$object;objectSHA256=(Get-FileHash $object).Hash.ToLower();exitCode=$code}
  $objects+=$object;$remove+=('/REMOVE:'+ $unit.member)
 }
 $archive="$scratch\judas_engine.lib";$log="$out\m78-$($m.label)-library.log";[IO.File]::WriteAllText($log,'')
 & lib.exe /nologo ("/OUT:"+$archive) $m.baselineLibrary.path @remove @objects 2>&1|Tee-Object -FilePath $log;$code=$LASTEXITCODE;Upload $log ([IO.Path]::GetFileName($log));if($code -ne 0){throw 'Isolated archive construction failed'}
 $members=@(& lib.exe /nologo /list $archive);if($LASTEXITCODE -ne 0 -or $members.Count -ne $m.baselineMemberCount){throw 'Isolated archive member count changed'}
 foreach($unit in $m.replacements){$found=@($members|Where-Object {[IO.Path]::GetFileName($_) -ieq $unit.objectName});if($found.Count -ne 1){throw ('Duplicate/missing replacement member '+$unit.objectName)}}
 $receipt="$out\m78-$($m.label)-library.json";@{scope=$m.scope;label=$m.label;baseline=$m.baselineLibrary;archive=$archive;archiveSHA256=(Get-FileHash $archive).Hash.ToLower();archiveBytes=(Get-Item $archive).Length;memberCount=$members.Count;units=$rows;freeBytes=(Get-PSDrive C).Free}|ConvertTo-Json -Depth 7|Set-Content $receipt -Encoding UTF8
 Upload $receipt ([IO.Path]::GetFileName($receipt))
}catch {
 $name='m78-direct-library-include-recovery-'+$(if($m -and $m.label){$m.label}else{'manifest'})+'-error.txt';[string]$_.Exception.Message|Set-Content "$out\$name";Upload "$out\$name" $name;exit 1
}
