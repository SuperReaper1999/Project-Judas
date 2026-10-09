$ErrorActionPreference='Stop';$root='C:\JudasValidation';$ProgressPreference='SilentlyContinue'
Invoke-WebRequest -UseBasicParsing 'http://192.168.122.1:8872/m73-teardown-delta.zip' -OutFile "$root\m73-teardown-delta.zip"
Expand-Archive -Force "$root\m73-teardown-delta.zip" "$root\Source"
Import-Module "$root\VS2022\Common7\Tools\Microsoft.VisualStudio.DevShell.dll"
Enter-VsDevShell -VsInstallPath "$root\VS2022" -SkipAutomaticLocation -DevCmdArguments '-arch=x64 -host_arch=x64'
$env:PATH="$root\Git\cmd;$root\VS2022\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin;$env:PATH"
$ErrorActionPreference='Continue'
& cmake -S "$root\Source" -B "$root\Build"
if($LASTEXITCODE -ne 0){exit $LASTEXITCODE}
(Get-Item "$root\Source\src\ScriptSystem.cpp").LastWriteTime=Get-Date
(Get-Item "$root\Source\tests\SignalTests.cpp").LastWriteTime=Get-Date
& cmake --build "$root\Build" --config Release --target judas_windows_sdk judas_signal_tests judas_signal_application_tests judas_script_tests --parallel 2 2>&1 | Tee-Object "$root\m73-forced-final-build.log"
$code=$LASTEXITCODE
Invoke-WebRequest -UseBasicParsing 'http://192.168.122.1:8872/m73-forced-final-build.log' -Method Post -InFile "$root\m73-forced-final-build.log"|Out-Null
@{exit=$code;end=(Get-Date).ToString('o')}|ConvertTo-Json|Set-Content "$root\m73-forced-final-build-result.json"
Invoke-WebRequest -UseBasicParsing 'http://192.168.122.1:8872/m73-forced-final-build-result.json' -Method Post -InFile "$root\m73-forced-final-build-result.json"|Out-Null
if($code -ne 0){exit $code}
$env:PATH="$root\Build\windows-sdk;$env:PATH"
Set-Location "$root\Source"
& "$root\Build\Release\judas_signal_tests.exe" 2>&1|Tee-Object "$root\m73-forced-final-signals.log"
$test=$LASTEXITCODE
Invoke-WebRequest -UseBasicParsing 'http://192.168.122.1:8872/m73-forced-final-signals.log' -Method Post -InFile "$root\m73-forced-final-signals.log"|Out-Null
& "$root\Build\Release\judas_script_tests.exe" 2>&1|Tee-Object "$root\m73-forced-final-scripts.log"
Invoke-WebRequest -UseBasicParsing 'http://192.168.122.1:8872/m73-forced-final-scripts.log' -Method Post -InFile "$root\m73-forced-final-scripts.log"|Out-Null
exit $test
