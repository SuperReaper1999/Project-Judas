$ErrorActionPreference='Stop';$ProgressPreference='SilentlyContinue'
$r='P:\JudasValidation\CleanDistribution';$source="$r\Source";$build="$r\Build";$python='C:\JudasValidation\Python\python.exe'
$result=Get-Content "$r\BUILD_RESULT.json" -Encoding UTF8 -Raw|ConvertFrom-Json
if($result.exit -ne 0){throw 'Cold build failed; do not refresh'}
Copy-Item "$r\BUILD_RESULT.json" "$r\BUILD_RESULT-COLD.json"
Copy-Item "$build\windows-sdk\SDK_MANIFEST.json" "$r\SDK_MANIFEST-COLD.json"
$core=(Get-FileHash "$build\Release\judas_engine.lib" -Algorithm SHA256).Hash
if((Get-FileHash "$r\Judas-Experimental-source-final.tar.gz" -Algorithm SHA256).Hash.ToLowerInvariant() -ne 'a1f43aeaf008c47a9ce83f5663990758d517340ab4e7bd8ca5777d237d2f42eb'){throw 'Corrected archive hash mismatch'}
& tar.exe -xzf "$r\Judas-Experimental-source-final.tar.gz" -C $source
if($LASTEXITCODE -ne 0){throw 'Archive extraction failed'}
$m=Get-Content "$source\JUDAS_SOURCE_FILES.json" -Encoding UTF8 -Raw|ConvertFrom-Json
foreach($f in $m.files){if((Get-FileHash -LiteralPath (Join-Path $source $f.path) -Algorithm SHA256).Hash.ToLowerInvariant() -ne $f.sha256){throw "Source hash mismatch $($f.path)"}}
Import-Module 'C:\JudasValidation\VS2022\Common7\Tools\Microsoft.VisualStudio.DevShell.dll'
Enter-VsDevShell -VsInstallPath 'C:\JudasValidation\VS2022' -SkipAutomaticLocation -DevCmdArguments '-arch=x64 -host_arch=x64'
$env:PATH='C:\JudasValidation\Python;C:\JudasValidation\Git\cmd;C:\JudasValidation\VS2022\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin;'+$env:PATH
$env:TEMP="$r\Temp";$env:TMP=$env:TEMP
& cmake.exe -S $source -B $build '-DJUDAS_SDK_RELEASE=ON' *> "$r\final-configure.log"
if($LASTEXITCODE -ne 0){throw 'Final configure failed'}
& cmake.exe --build $build --config Release --target judas_windows_sdk --parallel 2 *> "$r\final-stage.log"
if($LASTEXITCODE -ne 0){throw 'Final stage failed'}
if($core -ne (Get-FileHash "$build\Release\judas_engine.lib" -Algorithm SHA256).Hash){throw 'Engine core changed during documentation-only refresh'}
$sdk="$r\Moved SDK r$([char]0xe9)sum$([char]0xe9)";Copy-Item "$build\windows-sdk" $sdk -Recurse
New-Item -ItemType Directory -Force "$r\Unrelated CWD","$r\Projects","$r\Results"|Out-Null
& tar.exe -xzf "$r\FirstProject.tar.gz" -C "$r\Projects"
if($LASTEXITCODE -ne 0){throw 'First project extraction failed'}
@{files=$m.files.Count;allSourceHashesMatch=$true;coreBefore=$core;coreAfter=(Get-FileHash "$build\Release\judas_engine.lib" -Algorithm SHA256).Hash;coreUnchanged=$true;sdk=$sdk;source=(Get-Content "$source\JUDAS_SOURCE_INFO.json" -Encoding UTF8 -Raw|ConvertFrom-Json)}|ConvertTo-Json -Depth 6|Set-Content -Encoding UTF8 "$r\FINAL_PREPARED.json"
