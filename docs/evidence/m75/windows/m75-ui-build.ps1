$ErrorActionPreference='Stop';$ProgressPreference='SilentlyContinue';$root='C:\JudasValidation';$base='http://192.168.122.1:8872';$receipts="$root\Receipts\M75"
function Upload($p){Invoke-WebRequest -UseBasicParsing "$base/$([IO.Path]::GetFileName($p))" -Method Post -InFile $p|Out-Null}
try{
 Invoke-WebRequest -UseBasicParsing "$base/m75-ui.json" -OutFile "$root\m75-ui.json";$expected=Get-Content "$root\m75-ui.json" -Raw|ConvertFrom-Json;$proof=@()
 foreach($entry in $expected.files){$dest=Join-Path "$root\Source" $entry.path;Invoke-WebRequest -UseBasicParsing "$base/$($entry.transfer)" -OutFile $dest;$hash=(Get-FileHash $dest).Hash.ToLower();if($hash -ne $entry.sha256){throw "UI source mismatch $($entry.path)"};$proof+=@{path=$entry.path;sha256=$hash;matches=$true}}
 Import-Module "$root\VS2022\Common7\Tools\Microsoft.VisualStudio.DevShell.dll";Enter-VsDevShell -VsInstallPath "$root\VS2022" -SkipAutomaticLocation -DevCmdArguments '-arch=x64 -host_arch=x64';$env:PATH="$root\Git\cmd;$root\VS2022\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin;$env:PATH";$ErrorActionPreference='Continue'
 & cmake --build "$root\Build" --config Release --target judas_editor --parallel 2 2>&1|Tee-Object "$receipts\m75-windows-ui-build.log";$code=$LASTEXITCODE;$ErrorActionPreference='Stop';Upload "$receipts\m75-windows-ui-build.log";if($code){throw "UI build $code"}
 $exe="$root\Build\Release\judas_editor.exe"
 @{build=$code;source=$proof;exe=@{sha256=(Get-FileHash $exe).Hash.ToLower();lastWriteUtc=(Get-Item $exe).LastWriteTimeUtc.ToString('o')};freeBytes=(Get-PSDrive C).Free}|ConvertTo-Json -Depth 12|Set-Content "$receipts\m75-windows-ui-build-summary.json" -Encoding UTF8;Upload "$receipts\m75-windows-ui-build-summary.json";exit 0
}catch{$_|Out-String|Set-Content "$receipts\m75-windows-ui-build-failure.log";Upload "$receipts\m75-windows-ui-build-failure.log";exit 1}
