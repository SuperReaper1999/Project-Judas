# Fresh native dependency build and independent installed-package consumers.
param([Parameter(Mandatory)][string]$WorkDirectory)
$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path "$PSScriptRoot\..\..").Path
New-Item -ItemType Directory -Path $WorkDirectory | Out-Null
$work = (Resolve-Path -LiteralPath $WorkDirectory).Path
function Run-Native([string]$Program, [string[]]$Arguments) {
    & $Program @Arguments
    if ($LASTEXITCODE) { throw "$Program failed: $LASTEXITCODE" }
}
function Reject-Configure([string[]]$Arguments, [string]$Expected) {
    $ErrorActionPreference = 'Continue'
    & cmake @Arguments *> "$work/rejected-configure.log"
    $code = $LASTEXITCODE
    $ErrorActionPreference = 'Stop'
    if (!$code -or (Get-Content "$work/rejected-configure.log" -Raw) -notlike "*$Expected*") {
        Get-Content "$work/rejected-configure.log"
        throw "Missing configure rejection: $Expected"
    }
}
$oldPkgConfig = $env:PKG_CONFIG
$oldSources = $env:VCPKG_BINARY_SOURCES
$oldRoot = $env:VCPKG_ROOT
$oldPath = $env:PATH
try {
    # Any invocation of pkg-config is a test failure, including --version.
    $tools = Join-Path $work 'unusable-tools'
    New-Item -ItemType Directory -Path $tools | Out-Null
    $marker = Join-Path $work 'pkg-config-was-executed'
    $blocked = Join-Path $tools 'pkg-config.cmd'
    Set-Content $blocked @('@echo off', "echo forbidden > `"$marker`"", 'exit /b 17')
    $env:PKG_CONFIG = $blocked
    $env:PATH = "$tools;$oldPath"
    $env:VCPKG_BINARY_SOURCES = 'clear'
    $env:VCPKG_ROOT = Join-Path $work 'vcpkg-must-not-be-used'
    $fixture = "$repo/tests/fixtures/windows-dependencies"
    $build = Join-Path $work 'dependency build'
    $prefix = Join-Path $work 'installed'
    Run-Native cmake @('-S', $fixture, '-B', $build, '-G', 'Ninja Multi-Config', "-DJUDAS_SOURCE_ROOT=$repo", "-DCMAKE_INSTALL_PREFIX=$prefix")
    foreach ($config in @('Release', 'Debug')) {
        Run-Native cmake @('--build', $build, '--config', $config, '--target', 'dependency_probe', 'SDL2main', '--parallel', '2')
        Run-Native "$build/$config/dependency_probe.exe" @()
        Run-Native cmake @('--install', $build, '--config', $config)
        $sdl = if ($config -eq 'Debug') { 'SDL2-staticd.lib' } else { 'SDL2-static.lib' }
        $crt = if ($config -eq 'Debug') { 'MSVCRTD' } else { 'MSVCRT' }
        $glm = if ($config -eq 'Debug') { 'glmd.lib' } else { 'glm.lib' }
        foreach ($library in @($sdl, $glm)) {
            $directives = & dumpbin /directives "$prefix/lib/$library"
            if ($LASTEXITCODE -or ($directives -join "`n") -notmatch "DEFAULTLIB:`"?$crt(?:`"|\s|$)") { throw "$library $config CRT contract is missing" }
        }
    }
    # Remove the original install/build locations before independent linking.
    $moved = Join-Path $work 'moved dependency package'
    Move-Item $prefix $moved
    Move-Item $build (Join-Path $work 'hidden dependency build')
    Copy-Item $fixture (Join-Path $work 'consumer-source') -Recurse
    $consumer = Join-Path $work 'consumer-build'
    Run-Native cmake @('-S', "$work/consumer-source/consumer", '-B', $consumer, '-G', 'Ninja Multi-Config', "-DCMAKE_PREFIX_PATH=$moved", '-DCMAKE_FIND_USE_PACKAGE_REGISTRY=OFF')
    foreach ($config in @('Release', 'Debug')) {
        Run-Native cmake @('--build', $consumer, '--config', $config, '--parallel', '2')
        Run-Native "$consumer/$config/consumer.exe" @()
        $main = Start-Process -FilePath "$consumer/$config/consumer_main.exe" -Wait -PassThru
        if ($main.ExitCode) { throw "SDL2main $config consumer failed: $($main.ExitCode)" }
    }
    if (Test-Path $marker) { throw 'pkg-config was executed' }
    if (Test-Path $env:VCPKG_ROOT) { throw 'vcpkg was used' }
    # Exercise the real FetchContent hash failure, not a hand-written substitute.
    $bad = Join-Path $work 'corrupt-source'
    New-Item -ItemType Directory -Path "$bad/cmake", "$bad/third_party/windows" -Force | Out-Null
    Copy-Item "$repo/cmake/JudasWindowsDependencies.cmake" "$bad/cmake/"
    Set-Content "$bad/third_party/windows/SDL2-2.30.12.tar.gz" 'corrupt source archive'
    Reject-Configure @('-S', $fixture, '-B', "$work/bad-hash-build", '-G', 'Ninja', "-DJUDAS_SOURCE_ROOT=$bad") 'does not match expected value'
    Reject-Configure @('-S', $repo, '-B', "$work/bad-crt-build", '-G', 'Ninja', '-DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded') 'requires /MD'
    Reject-Configure @('-S', $repo, '-B', "$work/legacy-build", '-G', 'Ninja', '-DVCPKG_TARGET_TRIPLET=x64-windows-static-md') 'without the old vcpkg toolchain'
    Write-Host 'PASS fresh native SDL2/GLM Release/Debug, MD/MDd, SDL2main, relocated CMake consumers, source-hash and configuration rejections; no pkgconf or vcpkg execution.'
} finally {
    $env:PKG_CONFIG = $oldPkgConfig
    $env:VCPKG_BINARY_SOURCES = $oldSources
    $env:VCPKG_ROOT = $oldRoot
    $env:PATH = $oldPath
}
