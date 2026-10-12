# Named reversible per-file compression only; no deletion or global setting.
$ErrorActionPreference='Stop';$ProgressPreference='SilentlyContinue';$base='http://192.168.122.1:8872';$root='C:\JudasValidation'
$p='C:\JudasValidation\M78ContinuationBuild\cold-repair-final-v2\judas_engine.lib'
$sha='71a722dded64cfcc634d679e55ddbd257058da888ada914f7f7150b8d88c2d62';$bytes=67019246
$m=(Invoke-WebRequest -UseBasicParsing "$base/m78-lifecycle-disk-approval.json").Content|ConvertFrom-Json
$r=@($m.compress|Where-Object {$_.path -eq $p});if(!$m.hostVerified -or $r.Count -ne 1 -or $r[0].sha256 -ne $sha -or $r[0].bytes -ne $bytes){throw 'Independent exact host archival proof required'}
if((Get-Item -LiteralPath $p).Length -ne $bytes -or (Get-FileHash -LiteralPath $p).Hash.ToLower() -ne $sha){throw 'Guest logical identity differs; no compression'}
$before=(Get-PSDrive C).Free;$beforeAttributes=[string](Get-Item -LiteralPath $p).Attributes
$output=@(& compact.exe /C /I /Q $p);$code=$LASTEXITCODE
if($code -ne 0){throw 'Named per-file compression returned nonzero'}
if((Get-Item -LiteralPath $p).Length -ne $bytes -or (Get-FileHash -LiteralPath $p).Hash.ToLower() -ne $sha){throw 'Logical bytes changed'}
$rp="$root\m78-lifecycle-compression-receipt.json";@{scope='Exactly one named frozen native archive. Per-file NTFS compression only, independently archived on host; no deletion, VM/global settings or source/test changes';path=$p;logicalBytes=$bytes;sha256=$sha;logicalBytesBeforeAfterEqual=$true;attributesBefore=$beforeAttributes;attributesAfter=[string](Get-Item -LiteralPath $p).Attributes;compactOutput=$output;freeBefore=$before;freeAfter=(Get-PSDrive C).Free;exitCode=$code;minimumBuildPreflightBytes=25MB}|ConvertTo-Json -Depth 6|Set-Content $rp -Encoding UTF8
Invoke-WebRequest -UseBasicParsing "$base/m78-lifecycle-compression-receipt.json" -Method Post -InFile $rp|Out-Null
if((Get-PSDrive C).Free -lt 25MB){throw 'Storage preflight remains insufficient; stop, do not delete more files'}
