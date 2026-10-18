$ErrorActionPreference='Stop';$ProgressPreference='SilentlyContinue'
$root='C:\JudasValidation';$task='P:\JudasValidation\PreAlphaPass4';$source="$task\Source";$build="$task\Build"
New-Item -ItemType Directory -Force "$task\Temp"|Out-Null
$env:TEMP="$task\Temp";$env:TMP=$env:TEMP
Import-Module "$root\VS2022\Common7\Tools\Microsoft.VisualStudio.DevShell.dll"
Enter-VsDevShell -VsInstallPath "$root\VS2022" -SkipAutomaticLocation -DevCmdArguments '-arch=x64 -host_arch=x64'
$env:PATH="$root\Python;$root\Git\cmd;$root\VS2022\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin;$env:PATH"
$pkgconf=(Get-ChildItem "$root\Pkgconf" -Recurse -Filter pkgconf.exe|Select-Object -First 1).FullName
if(!$pkgconf){throw 'Native pkgconf missing'}
$env:PKG_CONFIG=$pkgconf;$env:VCPKG_KEEP_ENV_VARS='PKG_CONFIG'
$tools=@('cl.exe','cmake.exe','git.exe','python.exe')|ForEach-Object {$p=(Get-Command $_).Source;[pscustomobject]@{name=$_;path=$p;sha256=(Get-FileHash $p).Hash.ToLowerInvariant()}}
$provenance=[pscustomobject]@{utc=(Get-Date).ToUniversalTime().ToString('o');os=(Get-CimInstance Win32_OperatingSystem|Select-Object Caption,Version,BuildNumber);tools=$tools;source=$source;build=$build;systemFreeBytes=(Get-PSDrive C).Free;workFreeBytes=(Get-PSDrive P).Free;configuration='Release';generator='Visual Studio 17 2022';platform='x64';vcpkgInstalled="$root\Build\vcpkg_installed";command=@('cmake','-S',$source,'-B',$build,'-G','Visual Studio 17 2022','-A','x64',"-DCMAKE_TOOLCHAIN_FILE=$root/vcpkg/scripts/buildsystems/vcpkg.cmake",'-DVCPKG_TARGET_TRIPLET=x64-windows-static-md',"-DVCPKG_MANIFEST_DIR=$source/packaging/windows","-DVCPKG_INSTALLED_DIR=$root/Build/vcpkg_installed",'-DJUDAS_BUILD_CHANNEL=Experimental',"-DPython3_EXECUTABLE=$root/Python/python.exe")}
$provenance|ConvertTo-Json -Depth 6|Set-Content -Encoding UTF8 "$task\configure-provenance.json"
$ErrorActionPreference='Continue'
& cmake -S $source -B $build -G 'Visual Studio 17 2022' -A x64 "-DCMAKE_TOOLCHAIN_FILE=$root/vcpkg/scripts/buildsystems/vcpkg.cmake" -DVCPKG_TARGET_TRIPLET=x64-windows-static-md "-DVCPKG_MANIFEST_DIR=$source/packaging/windows" "-DVCPKG_INSTALLED_DIR=$root/Build/vcpkg_installed" -DJUDAS_BUILD_CHANNEL=Experimental "-DPython3_EXECUTABLE=$root/Python/python.exe" 2>&1|Tee-Object "$task\configure.log"
$exit=$LASTEXITCODE
[pscustomobject]@{utc=(Get-Date).ToUniversalTime().ToString('o');exit=$exit;systemFreeBytes=(Get-PSDrive C).Free;workFreeBytes=(Get-PSDrive P).Free}|ConvertTo-Json|Set-Content -Encoding UTF8 "$task\configure-result.json"
exit $exit
