$ErrorActionPreference='Stop';$ProgressPreference='SilentlyContinue';$root='C:\JudasValidation';$base='http://192.168.122.1:8872';$out="$root\M78ContinuationResults";$target="$root\M78ContinuationSource\Scripts\windows_direct_case_v3.ps1"
if(Test-Path $target){throw 'Preserve preceding v3 script'}
Invoke-WebRequest -UseBasicParsing "$base/m78-continuation-windows_direct_case_v3.ps1" -OutFile $target
$tokens=$null;$parseErrors=$null;$null=[Management.Automation.Language.Parser]::ParseFile($target,[ref]$tokens,[ref]$parseErrors)
@{scope='Native PowerShell parser only; multi-TU build script not executed';path=$target;sha256=(Get-FileHash $target).Hash.ToLower();parseErrors=@($parseErrors|ForEach-Object {$_.Message});freeBytes=(Get-PSDrive C).Free}|ConvertTo-Json -Depth 5|Set-Content "$out\m78-continuation-direct-case-v3-parse.json" -Encoding UTF8
Invoke-WebRequest -UseBasicParsing "$base/m78-continuation-direct-case-v3-parse.json" -Method Post -InFile "$out\m78-continuation-direct-case-v3-parse.json"|Out-Null
if($parseErrors.Count){exit 1}
