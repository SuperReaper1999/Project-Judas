$ErrorActionPreference='Stop';$ProgressPreference='SilentlyContinue'
$root='P:\JudasValidation\CleanDistribution';$source="$root\Source";$build="$root\Build"
$env:TEMP="$root\Temp";$env:TMP=$env:TEMP;$env:VCPKG_BINARY_SOURCES='clear';$env:VCPKG_DOWNLOADS="$root\Downloads"
New-Item -ItemType Directory -Force $env:TEMP,$env:VCPKG_DOWNLOADS|Out-Null
$start=[DateTime]::UtcNow
try {
 & tar.exe -xzf "$root\Judas-Experimental-source.tar.gz" -C $source
 if($LASTEXITCODE -ne 0){throw 'Source extraction failed'}
 if(Test-Path "$source\.git"){throw 'Unexpected Git metadata in archive'}
 $manifest=Get-Content "$source\JUDAS_SOURCE_FILES.json" -Raw|ConvertFrom-Json
 foreach($row in $manifest.files){if((Get-FileHash -LiteralPath (Join-Path $source $row.path) -Algorithm SHA256).Hash.ToLowerInvariant() -ne $row.sha256){throw "Source hash mismatch $($row.path)"}}
 @{files=$manifest.files.Count;allHashesMatch=$true;hasGit=$false;source=(Get-Content "$source\JUDAS_SOURCE_INFO.json" -Raw|ConvertFrom-Json)}|ConvertTo-Json -Depth 6|Set-Content -Encoding UTF8 "$root\SOURCE_VERIFIED.json"
 Import-Module 'C:\JudasValidation\VS2022\Common7\Tools\Microsoft.VisualStudio.DevShell.dll'
 Enter-VsDevShell -VsInstallPath 'C:\JudasValidation\VS2022' -SkipAutomaticLocation -DevCmdArguments '-arch=x64 -host_arch=x64'
 $env:PATH='C:\JudasValidation\Python;C:\JudasValidation\Git\cmd;C:\JudasValidation\VS2022\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin;'+$env:PATH
 & py.exe -3 --version 2>&1|Out-File "$root\python-launcher.log" -Encoding UTF8
 if($LASTEXITCODE -ne 0){throw 'Documented Python launcher prerequisite missing'}
 $pkg=(Get-ChildItem 'C:\JudasValidation\Pkgconf' -Filter pkgconf.exe -Recurse|Select-Object -First 1).FullName
 $ErrorActionPreference='Continue'
 & "$source\packaging\windows\Build-Windows.ps1" -VcpkgRoot "$root\vcpkg" -PkgConfig $pkg -Channel Experimental -BuildDirectory $build *>&1|Out-File "$root\build.log" -Encoding UTF8
 $helper=$LASTEXITCODE
 if($helper -ne 0){throw "Documented Windows build failed: $helper"}
 $ErrorActionPreference='Stop'
 & cmake -S $source -B $build -DJUDAS_SDK_RELEASE=ON *>&1|Out-File "$root\strict-configure.log" -Encoding UTF8
 if($LASTEXITCODE -ne 0){throw 'Strict release configure failed'}
 & cmake --build $build --config Release --target judas_windows_sdk --parallel 2 *>&1|Out-File "$root\strict-stage.log" -Encoding UTF8
 if($LASTEXITCODE -ne 0){throw 'Strict SDK staging failed'}
 @{exit=0;start=$start.ToString('o');end=[DateTime]::UtcNow.ToString('o');source='verified no-Git engine archive';reusedEngineCache=$false;vcpkgBinaryCache='clear';build=$build}|ConvertTo-Json|Set-Content -Encoding UTF8 "$root\BUILD_RESULT.json"
} catch {
 @{exit=1;start=$start.ToString('o');end=[DateTime]::UtcNow.ToString('o');error=($_|Out-String)}|ConvertTo-Json|Set-Content -Encoding UTF8 "$root\BUILD_RESULT.json"
 exit 1
}
