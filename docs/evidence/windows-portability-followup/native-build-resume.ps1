$ErrorActionPreference='Continue'
$r='P:\JudasValidation\WindowsSeven'
$cm='C:\JudasValidation\VS2022\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
Move-Item "$r\build-before.log" "$r\build-before-setup-failure.log"
Move-Item "$r\build-before.exit" "$r\build-before-setup-failure.exit"
Copy-Item "$r\stack-metadata.json" "$r\stack-before.json"
$env:PATH='C:\JudasValidation\Python;C:\Windows;'+$env:PATH
& $cm -S "$r\Source" -B "$r\Build" '-DVCPKG_INSTALLED_DIR=P:/JudasValidation/CleanDistribution/Build/vcpkg_installed' *> "$r\configure-prefix.log"
if($LASTEXITCODE -ne 0){throw 'configure failed'}
& $cm --build "$r\Build" --config Release --target judas_mk2_rotation_tests judas_liquid_rotation_tests judas_liquid_surface_tests judas_editor_preferences_tests --parallel 2 *> "$r\build-before.log"
$LASTEXITCODE | Set-Content "$r\build-before.exit"
Get-Content "$r\build-before.log" -Tail 8
