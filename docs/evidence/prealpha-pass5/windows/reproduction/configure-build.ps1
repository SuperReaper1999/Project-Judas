$ErrorActionPreference='Stop';$ProgressPreference='SilentlyContinue'
$root='C:\JudasValidation';$task='P:\JudasValidation\PreAlphaPass5';$previous='P:\JudasValidation\PreAlphaPass4';$source="$previous\Source";$build="$previous\Build"
New-Item -ItemType Directory -Force "$task\Temp"|Out-Null
$env:TEMP="$task\Temp";$env:TMP=$env:TEMP;$env:PYTHONDONTWRITEBYTECODE='1'
Import-Module "$root\VS2022\Common7\Tools\Microsoft.VisualStudio.DevShell.dll"
Enter-VsDevShell -VsInstallPath "$root\VS2022" -SkipAutomaticLocation -DevCmdArguments '-arch=x64 -host_arch=x64'
$env:PATH="$root\Python;$root\Git\cmd;$root\VS2022\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin;$env:PATH"
$pkgconf=(Get-ChildItem "$root\Pkgconf" -Recurse -Filter pkgconf.exe|Select-Object -First 1).FullName
if(!$pkgconf){throw 'Native pkgconf missing'}
$env:PKG_CONFIG=$pkgconf;$env:VCPKG_KEEP_ENV_VARS='PKG_CONFIG'
$targets=@('judas','judas_editor','judas_export','judas_impact_candidate_cache_tests','judas_impact_timing_tests','judas_contact_cache_tests','judas_physics_continuation_tests','judas_contact_lifecycle_tests','judas_character_motor_tests','judas_rigid_motion_tests','judas_impact_robustness_tests','judas_world_persistence_tests','judas_origin_admission_tests','judas_impact_mesh_cost_tests','judas_character_lab_tests','judas_balance_lab_tests')
$tools=@('cl.exe','cmake.exe','git.exe','python.exe')|ForEach-Object {$p=(Get-Command $_).Source;[pscustomobject]@{name=$_;path=$p;sha256=(Get-FileHash $p).Hash.ToLowerInvariant()}}
$provenance=[pscustomobject]@{utc=[DateTime]::UtcNow.ToString('o');os=(Get-CimInstance Win32_OperatingSystem|Select-Object Caption,Version,BuildNumber);tools=$tools;source=$source;build=$build;targets=$targets;sourceVerification=(Get-Content -Raw "$task\SOURCE_VERIFIED.json"|ConvertFrom-Json);cacheReuse='Existing CMake paths; no path/cache rewriting; no whole SDK rebuild';windowsTimingDiagnosticOnly=$true}
$provenance|ConvertTo-Json -Depth 8|Set-Content -Encoding UTF8 "$task\BUILD_PROVENANCE.json"
$ErrorActionPreference='Continue'
& cmake -S $source -B $build -G 'Visual Studio 17 2022' -A x64 "-DCMAKE_TOOLCHAIN_FILE=$root/vcpkg/scripts/buildsystems/vcpkg.cmake" -DVCPKG_TARGET_TRIPLET=x64-windows-static-md "-DVCPKG_MANIFEST_DIR=$source/packaging/windows" "-DVCPKG_INSTALLED_DIR=$root/Build/vcpkg_installed" -DJUDAS_BUILD_CHANNEL=Experimental "-DPython3_EXECUTABLE=$root/Python/python.exe" 2>&1|Out-File "$task\configure.log" -Encoding UTF8
$configExit=$LASTEXITCODE
if($configExit-ne 0){[pscustomobject]@{configureExit=$configExit;buildAttempted=$false}|ConvertTo-Json|Set-Content -Encoding UTF8 "$task\BUILD_RESULT.json";exit $configExit}
$start=[DateTime]::UtcNow
& cmake --build $build --config Release --target @targets --parallel 1 2>&1|Out-File "$task\build.log" -Encoding UTF8
$buildExit=$LASTEXITCODE
[pscustomobject]@{utc=[DateTime]::UtcNow.ToString('o');start=$start.ToString('o');configureExit=$configExit;buildExit=$buildExit;targets=$targets;systemFreeBytes=(Get-PSDrive C).Free;workFreeBytes=(Get-PSDrive P).Free;timingInformationalOnly=$true}|ConvertTo-Json -Depth 4|Set-Content -Encoding UTF8 "$task\BUILD_RESULT.json"
exit $buildExit
