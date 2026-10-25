$ErrorActionPreference='Continue'
$r='P:\JudasValidation\WindowsSeven'
$cm='C:\JudasValidation\VS2022\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
$env:PATH='C:\JudasValidation\Python;C:\Windows;'+$env:PATH
& $cm -S "$r\Source" -B "$r\Build" -G 'Visual Studio 17 2022' -A x64 '-DVCPKG_TARGET_TRIPLET=x64-windows-static-md' '-DCMAKE_PREFIX_PATH=P:/JudasValidation/CleanDistribution/Build/vcpkg_installed/x64-windows-static-md' '-DJUDAS_BUILD_CHANNEL=Experimental' '-DJUDAS_ENABLE_VALIDATION=ON' *> "$r\configure.log"
$LASTEXITCODE | Set-Content "$r\configure.exit"
Get-Content "$r\configure.log" -Tail 12
