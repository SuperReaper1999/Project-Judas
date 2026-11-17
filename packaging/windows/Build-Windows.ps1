[CmdletBinding()]
param(
    [ValidateSet('Visual Studio 17 2022', 'Visual Studio 18 2026', 'Ninja')][string]$Generator = 'Visual Studio 17 2022',
    [ValidateSet('Alpha', 'Beta', 'Experimental', 'Stable', 'Local')][string]$Channel = 'Experimental',
    [string]$BuildDirectory = "",
    [string]$EngineVersion = "",
    [string]$CompilerLauncher = "",
    [ValidateRange(1,32)][int]$Jobs = 2,
    [switch]$ReleasePackage,
    [switch]$DependenciesOnly
)
$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path "$PSScriptRoot\..\..").Path
function Run-Native([string]$Program, [string[]]$Arguments) {
    & $Program @Arguments
    if ($LASTEXITCODE -ne 0) { throw "$Program failed with exit $LASTEXITCODE" }
}
if (!(Get-Command cmake -ErrorAction SilentlyContinue)) { throw 'Install Visual Studio 2022 Desktop development with C++, including CMake, then use Developer PowerShell.' }
$build = if ($BuildDirectory) { [IO.Path]::GetFullPath($BuildDirectory) } else { "$repo\build\windows-native-$Channel" }
$configure = @('-S', $repo, '-B', $build, '-G', $Generator, '-DCMAKE_BUILD_TYPE=Release', "-DJUDAS_BUILD_CHANNEL=$Channel")
if ($Generator -ne 'Ninja') { $configure += @('-A', 'x64') }
if ($CompilerLauncher) { $configure += @("-DCMAKE_C_COMPILER_LAUNCHER=$CompilerLauncher", "-DCMAKE_CXX_COMPILER_LAUNCHER=$CompilerLauncher") }
if ($ReleasePackage) { $configure += "-DJUDAS_SDK_RELEASE=ON" }
if ($EngineVersion) { $configure += "-DJUDAS_ENGINE_VERSION=$EngineVersion" }
Run-Native cmake $configure
if ($DependenciesOnly) {
    Run-Native cmake @('--build', $build, '--config', 'Release', '--target', 'SDL2-static', 'SDL2main', 'glm', '--parallel', $Jobs)
    Write-Host "Windows SDL2/GLM built; Judas compilation and application qualification were not run: $build"
    return
}
Run-Native cmake @('--build', $build, '--config', 'Release', '--target', 'judas_windows_sdk', '--parallel', $Jobs)
Run-Native "$build\windows-sdk\judas_windows_readiness_tests.exe" @()
Write-Host "Windows SDK staged (native qualification still required): $build\windows-sdk"
Write-Host "Open docs/WINDOWS.md and perform the native editor/export checks. This script cannot supply human acceptance."
