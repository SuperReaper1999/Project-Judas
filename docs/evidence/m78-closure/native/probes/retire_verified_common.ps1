$ErrorActionPreference='Stop';$ProgressPreference='SilentlyContinue';$base='http://192.168.122.1:8872'
$m=(Invoke-WebRequest -UseBasicParsing "$base/m78-closure-common-approval.json").Content|ConvertFrom-Json
if(!$m.hostVerified -or $m.source -ne 'C:\JudasValidation\M78Results\common.json' -or $m.sourceBytes -ne 22619770){throw 'Named authorization mismatch'}
if((Get-Item $m.source).Length -ne $m.sourceBytes -or (Get-FileHash $m.source).Hash.ToLower() -ne $m.sourceSHA256){throw 'Original source identity changed'}
$before=(Get-PSDrive C).Free;Remove-Item -LiteralPath $m.source
$r='C:\JudasValidation\m78-closure-common-retirement.json';@{scope='Only precisely authorized obsolete guest duplicate rawcommon report retired after lossless host exactSHA/22619770byte proof. Original raw andgzip remain onhost; prior compact receipt unchanged; no source/fixtures/packages/config removed.';source=$m.source;sourceSHA256=$m.sourceSHA256;sourceBytes=$m.sourceBytes;hostRawPath=$m.hostRawPath;freeBefore=$before;freeAfter=(Get-PSDrive C).Free}|ConvertTo-Json|Set-Content $r -Encoding UTF8
Invoke-WebRequest -UseBasicParsing "$base/m78-closure-common-retirement.json" -Method Post -InFile $r|Out-Null
