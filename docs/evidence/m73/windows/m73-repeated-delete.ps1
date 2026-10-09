$ErrorActionPreference='Stop';$base='http://192.168.122.1:8872';$root='C:\JudasValidation';$ProgressPreference='SilentlyContinue'
$moved="$root\Packages\Moved Signals Lab M73 $([char]0x00e9) $([char]0x03b1)"
Invoke-WebRequest -UseBasicParsing "$base/m73-lab-delete-fixed.js" -OutFile "$moved\Assets\scripts\lab.js"
Invoke-WebRequest -UseBasicParsing "$base/m73-repeated-delete.harness" -OutFile "$root\m73-repeated-delete.harness"
$env:GALLIUM_DRIVER='llvmpipe';$env:JUDAS_TEST_SCRIPT="$root\m73-repeated-delete.harness"
$p=Start-Process -PassThru -FilePath "$moved\judas.exe" -WorkingDirectory "$root\Unrelated working directory" -RedirectStandardOutput "$root\Receipts\m73-repeated-delete.stdout.log" -RedirectStandardError "$root\Receipts\m73-repeated-delete.stderr.log"
[void]$p.Handle;if(!$p.WaitForExit(180000)){Stop-Process -Id $p.Id -Force;throw 'Timeout'};$p.Refresh()
$err=[IO.File]::ReadAllText("$root\Receipts\m73-repeated-delete.stderr.log");$result=@{exit=$p.ExitCode;fault=($err -match 'script asset=|ReferenceError|TypeError');scriptHash=(Get-FileHash "$moved\Assets\scripts\lab.js").Hash;finished=(Get-Date).ToString('o')}
$result|ConvertTo-Json|Set-Content "$root\Receipts\m73-repeated-delete.result.json" -Encoding UTF8
foreach($file in @('m73-repeated-delete.stdout.log','m73-repeated-delete.stderr.log','m73-repeated-delete.result.json')){Invoke-WebRequest -UseBasicParsing "$base/$file" -Method Post -InFile "$root\Receipts\$file"|Out-Null}
if($result.exit -ne 0 -or $result.fault){throw 'Repeated delete regression failed'}
