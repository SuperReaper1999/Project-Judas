$ErrorActionPreference='Stop';$ProgressPreference='SilentlyContinue';$base='http://192.168.122.1:8872';$root='C:\JudasValidation'
$m=(Invoke-WebRequest -UseBasicParsing "$base/m78-lifecycle-disk-approval.json").Content|ConvertFrom-Json
if(!$m.hostVerified -or $m.retire.Count -ne 1 -or $m.compress.Count -ne 1){throw 'Exact independent host archival receipt required'}
$before=(Get-PSDrive C).Free;$rows=@()
foreach($r in @($m.retire)+@($m.compress)){if((Get-Item $r.path).Length -ne $r.bytes -or (Get-FileHash $r.path).Hash.ToLower() -ne $r.sha256){throw 'Guest/host logical bytes differ; no cleanup'}}
if($m.retire[0].path -ne 'C:\JudasValidation\M78ClosureMovedPackageV2\judas.exe'){throw 'Only approved completed new moved duplicate'}
Remove-Item -LiteralPath $m.retire[0].path
foreach($r in $m.compress){if($r.path -ne 'C:\JudasValidation\M78ContinuationBuild\cold-repair-final-v2\judas_engine.lib'){throw 'Only approved named per-file archive'};$output=@(& compact.exe /C /I /Q $r.path);if($LASTEXITCODE -ne 0){throw 'Per-file compression failed'};if((Get-Item $r.path).Length -ne $r.bytes -or (Get-FileHash $r.path).Hash.ToLower() -ne $r.sha256){throw 'Logical bytes changed by compression'};$rows+=@{path=$r.path;bytes=$r.bytes;sha256=$r.sha256;beforeAfterLogicalIdentity=$true;compactOutput=$output}}
$out="$root\m78-lifecycle-disk-receipt.json";@{scope=$m.scope;retired=$m.retire;compressed=$rows;freeBefore=$before;freeAfter=(Get-PSDrive C).Free;vmConfigurationChanged=$false}|ConvertTo-Json -Depth 6|Set-Content $out -Encoding UTF8
Invoke-WebRequest -UseBasicParsing "$base/m78-lifecycle-disk-receipt.json" -Method Post -InFile $out|Out-Null
