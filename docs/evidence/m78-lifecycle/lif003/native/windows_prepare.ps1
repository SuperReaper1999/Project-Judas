$ErrorActionPreference='Stop';$ProgressPreference='SilentlyContinue';$base='http://192.168.122.1:8872';$root='C:\JudasValidation'
function Upload($p,$n){Invoke-WebRequest -UseBasicParsing "$base/$n" -Method Post -InFile $p|Out-Null}
try{
$m=(Invoke-WebRequest -UseBasicParsing "$base/m78-lif003-v1.json").Content|ConvertFrom-Json;$s=$m.scratch;$o=$m.output
if(Test-Path $s){throw 'Preserve existing closure scratch'};if((Get-PSDrive C).Free -lt 14MB){throw 'No headroom for explicit replacement objects'}
if((Get-FileHash $m.baselineLibrary.path).Hash.ToLower() -ne $m.baselineLibrary.sha256){throw 'Frozen library changed'}
New-Item -ItemType Directory "$s\source","$s\objects",$o|Out-Null
$z="$s\source.zip";Invoke-WebRequest -UseBasicParsing "$base/$($m.archiveName)" -OutFile $z;if((Get-FileHash $z).Hash.ToLower() -ne $m.archiveSHA256){throw 'Prepared source ZIP changed'};Expand-Archive $z "$s\source"
foreach($f in $m.files){$p=Join-Path "$s\source" $f.path;if((Get-FileHash $p).Hash.ToLower() -ne $f.sha256 -or (Get-Item $p).Length -ne $f.bytes){throw 'Fresh source identity mismatch'}}
foreach($f in $m.auxiliaryFixtures){$p=Join-Path "$s\source" $f.path;New-Item -ItemType Directory ([IO.Path]::GetDirectoryName($p)) -Force|Out-Null;Invoke-WebRequest -UseBasicParsing $f.url -OutFile $p;if((Get-FileHash $p).Hash.ToLower() -ne $f.sha256 -or(Get-Item $p).Length -ne $f.bytes){throw 'Auxiliary exact fixture mismatch'}}
Import-Module "$root\VS2022\Common7\Tools\Microsoft.VisualStudio.DevShell.dll";Enter-VsDevShell -VsInstallPath "$root\VS2022" -SkipAutomaticLocation -DevCmdArguments '-arch=x64 -host_arch=x64';Set-Location "$root\Build"
$members=@(& lib.exe /nologo /list $m.baselineLibrary.path);if($LASTEXITCODE -ne 0 -or $members.Count -ne $m.baselineMemberCount){throw 'Frozen archive member mismatch'}
$rows=@();foreach($u in $m.retainedReplacementObjects){if((Get-FileHash $u.path).Hash.ToLower() -ne $u.sha256 -or (Get-Item $u.path).Length -ne $u.bytes){throw 'Retained current production object changed'};$rows+=$u};foreach($u in $m.replacements){
 $source=Join-Path "$s\source" $u.path;$obj="$s\objects\$($u.objectName)";$old=@($members|Where-Object {[IO.Path]::GetFileName($_) -ieq $u.objectName});if($old.Count -ne 1){throw 'Unique frozen original member required'}
 $flags=[string]$m.engineFlags;$flags=[regex]::Replace($flags,'/Fo"[^"]*"',('/Fo"'+$obj+'"'),'IgnoreCase');$flags=[regex]::Replace($flags,'/Fd"[^"]*"',('/Fd"'+$s+'\objects\compile.pdb"'),'IgnoreCase');$flags=[regex]::Replace($flags,'\s+C:\\JUDASVALIDATION\\SOURCE\\[^\r\n]*\.CPP\s*$','','IgnoreCase');$flags='/I"'+$s+'\source\src" /I"'+$m.oldSource+'\src" /I"'+$root+'\Source\third_party" '+$flags+' "'+$source+'"'
 $rsp="$s\objects\$($u.objectName).rsp";[IO.File]::WriteAllText($rsp,$flags,[Text.UTF8Encoding]::new($false));$log="$o\m78-lif003-$($u.objectName)-compile.log";$ErrorActionPreference='Continue';& cl.exe "@$rsp" 2>&1|Tee-Object -FilePath $log;$exit=$LASTEXITCODE;$ErrorActionPreference='Stop';Upload $log ([IO.Path]::GetFileName($log));if($exit -ne 0){throw 'Replacement TU compile failed'}
 Upload $obj ('m78-lif003-'+$u.objectName+'.bin');$rows+=@{path=$obj;source=$u.path;sourceSHA256=(Get-FileHash $source).Hash.ToLower();bytes=(Get-Item $obj).Length;sha256=(Get-FileHash $obj).Hash.ToLower();compilerResponse=$flags;frozenMember=$old[0]}
}
$rp="$o\m78-lif003-engine.json";@{scope=$m.scope;archive=$m.baselineLibrary;objects=$rows;sourceBundleSHA256=$m.archiveSHA256;freeBytes=(Get-PSDrive C).Free}|ConvertTo-Json -Depth 6|Set-Content $rp -Encoding UTF8;Upload $rp ([IO.Path]::GetFileName($rp))
}catch{$_|Out-String|Set-Content "$root\m78-lif003-prepare-error.txt";Upload "$root\m78-lif003-prepare-error.txt" 'm78-lif003-prepare-error.txt';exit 1}
