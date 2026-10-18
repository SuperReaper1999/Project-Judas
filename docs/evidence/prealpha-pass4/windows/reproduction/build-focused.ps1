$ErrorActionPreference='Stop';$ProgressPreference='SilentlyContinue'
$root='C:\JudasValidation';$task='P:\JudasValidation\PreAlphaPass4';$env:TMP="$task\Temp";$env:TEMP=$env:TMP
Import-Module "$root\VS2022\Common7\Tools\Microsoft.VisualStudio.DevShell.dll"
Enter-VsDevShell -VsInstallPath "$root\VS2022" -SkipAutomaticLocation -DevCmdArguments '-arch=x64 -host_arch=x64'
$cmake="$root\VS2022\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
$targets=@('judas_impact_mesh_cost_tests','judas_impact_robustness_tests','judas_origin_admission_tests','judas_character_motor_tests','judas_world_persistence_tests','judas_character_lab_tests','judas_balance_lab_tests')
$start=[DateTime]::UtcNow;$ErrorActionPreference='Continue'
& $cmake --build "$task\Build" --config Release --target @targets --parallel 1 2>&1|Out-File "$task\build-focused.log" -Encoding UTF8
$exit=$LASTEXITCODE
[pscustomobject]@{utc=[DateTime]::UtcNow.ToString('o');start=$start.ToString('o');exit=$exit;targets=$targets;timingInformationalOnly=$true}|ConvertTo-Json -Depth 3|Set-Content -Encoding UTF8 "$task\BUILD_FOCUSED.json"
exit $exit
