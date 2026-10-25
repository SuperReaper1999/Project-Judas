$ErrorActionPreference='Continue'
$r='P:\JudasValidation\WindowsSeven'
$cm='C:\JudasValidation\VS2022\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
$env:PATH='C:\JudasValidation\Python;C:\Windows;'+$env:PATH
& $cm --build "$r\Build" --config Release --target judas_mk2_rotation_tests judas_liquid_rotation_tests judas_liquid_surface_tests judas_editor_preferences_tests --parallel 2 *> "$r\build-before.log"
$LASTEXITCODE | Set-Content "$r\build-before.exit"
Get-Content "$r\build-before.log" -Tail 8
