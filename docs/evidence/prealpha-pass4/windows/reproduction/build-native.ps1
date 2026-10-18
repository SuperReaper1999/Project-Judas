$ErrorActionPreference='Stop';$ProgressPreference='SilentlyContinue'
$root='C:\JudasValidation';$task='P:\JudasValidation\PreAlphaPass4';$build="$task\Build"
New-Item -ItemType Directory -Force "$task\Temp"|Out-Null
$env:TEMP="$task\Temp";$env:TMP=$env:TEMP
Import-Module "$root\VS2022\Common7\Tools\Microsoft.VisualStudio.DevShell.dll"
Enter-VsDevShell -VsInstallPath "$root\VS2022" -SkipAutomaticLocation -DevCmdArguments '-arch=x64 -host_arch=x64'
$env:PATH="$root\Python;$root\Git\cmd;$root\VS2022\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin;$env:PATH"
$start=(Get-Date).ToUniversalTime();$ErrorActionPreference='Continue'
& cmake --build $build --config Release --target judas_windows_sdk --parallel 1 2>&1|Out-File -FilePath "$task\build.log" -Encoding UTF8
$exit=$LASTEXITCODE
[pscustomobject]@{startUtc=$start.ToString('o');endUtc=(Get-Date).ToUniversalTime().ToString('o');exit=$exit;systemFreeBytes=(Get-PSDrive C).Free;workFreeBytes=(Get-PSDrive P).Free;target='judas_windows_sdk';outerParallelism=1;icuUpstreamParallelism=4;windowsTimingDiagnosticOnly=$true}|ConvertTo-Json|Set-Content -Encoding UTF8 "$task\build-result.json"
exit $exit
