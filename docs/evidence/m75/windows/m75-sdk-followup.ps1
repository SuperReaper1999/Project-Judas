$ErrorActionPreference='Stop';$ProgressPreference='SilentlyContinue';$root='C:\JudasValidation';$base='http://192.168.122.1:8872';$receipts="$root\Receipts\M75"
function Upload($p){Invoke-WebRequest -UseBasicParsing "$base/$([IO.Path]::GetFileName($p))" -Method Post -InFile $p|Out-Null}
try{
 Invoke-WebRequest -UseBasicParsing "$base/m75-sdk.cmake" -OutFile "$root\Source\cmake\JudasWindowsSdk.cmake"
 Invoke-WebRequest -UseBasicParsing "$base/m75-sdk.sha256" -OutFile "$root\m75-sdk.sha256"
 if((Get-FileHash "$root\Source\cmake\JudasWindowsSdk.cmake").Hash.ToLower() -ne (Get-Content "$root\m75-sdk.sha256" -Raw).Trim()){throw 'SDK definition transfer mismatch'}
 Import-Module "$root\VS2022\Common7\Tools\Microsoft.VisualStudio.DevShell.dll";Enter-VsDevShell -VsInstallPath "$root\VS2022" -SkipAutomaticLocation -DevCmdArguments '-arch=x64 -host_arch=x64'
 $env:PATH="$root\Git\cmd;$root\VS2022\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin;$env:PATH";$ErrorActionPreference='Continue'
 & cmake --build "$root\Build" --config Release --target judas_windows_sdk --parallel 2 2>&1|Tee-Object "$receipts\m75-windows-sdk-followup.log";$code=$LASTEXITCODE;$ErrorActionPreference='Stop';Upload "$receipts\m75-windows-sdk-followup.log";if($code){throw "SDK build $code"}
 $exe="$root\Build\windows-sdk\judas_navigation_bake.exe";@{exit=$code;sdkDefinitionSha256=(Get-FileHash "$root\Source\cmake\JudasWindowsSdk.cmake").Hash.ToLower();navigationToolSha256=(Get-FileHash $exe).Hash.ToLower();navigationToolWriteUtc=(Get-Item $exe).LastWriteTimeUtc.ToString('o')}|ConvertTo-Json|Set-Content "$receipts\m75-windows-sdk-proof.json" -Encoding UTF8;Upload "$receipts\m75-windows-sdk-proof.json"
 exit 0
}catch{$_|Out-String|Set-Content "$receipts\m75-windows-sdk-failure.log";Upload "$receipts\m75-windows-sdk-failure.log";exit 1}
