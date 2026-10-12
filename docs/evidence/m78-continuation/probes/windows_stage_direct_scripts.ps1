$ErrorActionPreference='Stop'; $ProgressPreference='SilentlyContinue'
$root='C:\JudasValidation';$base='http://192.168.122.1:8872';$out="$root\M78ContinuationResults";$scripts="$root\M78ContinuationSource\Scripts"
New-Item -ItemType Directory $scripts -Force|Out-Null;$rows=@()
foreach($name in @('windows_direct_library.ps1','windows_direct_case.ps1')){
 $target=Join-Path $scripts $name
 if(Test-Path $target){throw 'Preserve preceding native script'}
 Invoke-WebRequest -UseBasicParsing "$base/m78-continuation-$name" -OutFile $target
 $tokens=$null;$parseErrors=$null;$null=[Management.Automation.Language.Parser]::ParseFile($target,[ref]$tokens,[ref]$parseErrors)
 $rows+=@{name=$name;path=$target;sha256=(Get-FileHash $target).Hash.ToLower();parseErrors=@($parseErrors|ForEach-Object {$_.Message})}
}
@{scope='Only native PowerShell parser verification and staging; no compilation or library mutation';rows=$rows;freeBytes=(Get-PSDrive C).Free}|ConvertTo-Json -Depth 5|Set-Content "$out\m78-continuation-direct-script-parse.json" -Encoding UTF8
Invoke-WebRequest -UseBasicParsing "$base/m78-continuation-direct-script-parse.json" -Method Post -InFile "$out\m78-continuation-direct-script-parse.json"|Out-Null
if(@($rows|Where-Object {$_.parseErrors.Count}).Count){exit 1}
