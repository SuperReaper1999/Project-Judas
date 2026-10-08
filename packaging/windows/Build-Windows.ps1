param([string]$VcpkgRoot = "$env:LOCALAPPDATA\JudasBuildTools\vcpkg")
$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path "$PSScriptRoot\..\..").Path
$pin = 'd5ec528843d29e3a52d745a64b469f810b2cedbf'
function Run-Native([string]$Program, [string[]]$Arguments) {
    & $Program @Arguments
    if ($LASTEXITCODE -ne 0) { throw "$Program failed with exit $LASTEXITCODE" }
}
if (!(Get-Command cmake -ErrorAction SilentlyContinue)) { throw 'Install Visual Studio 2022 Desktop development with C++, including CMake, then use Developer PowerShell.' }
if (!(Test-Path "$VcpkgRoot\.git")) {
    if (Test-Path $VcpkgRoot) { throw "Existing non-Git directory: $VcpkgRoot" }
    Run-Native git @('clone', 'https://github.com/microsoft/vcpkg.git', $VcpkgRoot)
    Run-Native git @('-C', $VcpkgRoot, 'checkout', '--detach', $pin)
} else {
    $current = (& git -C $VcpkgRoot rev-parse HEAD).Trim()
    if ($LASTEXITCODE -ne 0 -or $current -ne $pin) { throw "Use a dedicated vcpkg checkout at $pin; existing checkout was left untouched." }
}
Run-Native "$VcpkgRoot\bootstrap-vcpkg.bat" @('-disableMetrics')
$build = "$repo\build\windows"
Run-Native cmake @('-S', $repo, '-B', $build, '-G', 'Visual Studio 17 2022', '-A', 'x64', "-DCMAKE_TOOLCHAIN_FILE=$VcpkgRoot/scripts/buildsystems/vcpkg.cmake", '-DVCPKG_TARGET_TRIPLET=x64-windows-static-md', "-DVCPKG_MANIFEST_DIR=$repo/packaging/windows", "-DVCPKG_INSTALLED_DIR=$build/vcpkg_installed")
Run-Native cmake @('--build', $build, '--config', 'Release', '--target', 'judas_windows_sdk', '--parallel', '6')
Run-Native "$build\windows-sdk\judas_windows_readiness_tests.exe" @()
Write-Host "UNVALIDATED Windows SDK ready: $build\windows-sdk"
Write-Host "Open docs/WINDOWS.md and perform the native editor/export checks. This script cannot supply human acceptance."
