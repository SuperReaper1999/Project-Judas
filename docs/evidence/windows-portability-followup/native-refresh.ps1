$ErrorActionPreference='Stop'
$r='P:\JudasValidation\WindowsSeven'
New-Item -ItemType Directory -Force "$r\stale-overlay-attempt" | Out-Null
Get-ChildItem "$r\after-*" -File | Copy-Item -Destination "$r\stale-overlay-attempt"
Copy-Item "$r\receipts.zip" "$r\stale-overlay-receipts.zip"
$expected=Get-Content "$r\expected-source.json" -Raw | ConvertFrom-Json
$times=@()
foreach($p in $expected.PSObject.Properties) { $file=Get-Item (Join-Path "$r\Source" $p.Name);$times+=@{file=$p.Name;originalTime=$file.LastWriteTimeUtc};$file.LastWriteTimeUtc=[DateTime]::UtcNow }
$times|ConvertTo-Json|Set-Content "$r\overlay-timestamps.json"
$ErrorActionPreference='Continue'
$cm='C:\JudasValidation\VS2022\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
& $cm --build "$r\Build" --config Release --target judas_mk2_rotation_tests judas_liquid_rotation_tests judas_liquid_surface_tests judas_editor_preferences_tests judas_mk2_rotation_cold_tests judas_mk2_physics_tests judas_impact_mesh_cost_tests --parallel 2 *> "$r\build-after-forced.log"
$LASTEXITCODE|Set-Content "$r\build-after-forced.exit"
Get-Content "$r\build-after-forced.log" -Tail 7
