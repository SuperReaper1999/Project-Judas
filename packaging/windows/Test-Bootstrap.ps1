# No Pester/module installation required. Run on a fresh Windows CI runner.
param([Parameter(Mandatory)][string]$WorkDirectory)
$ErrorActionPreference = 'Stop'
. "$PSScriptRoot\PkgConfig.ps1"
New-Item -ItemType Directory -Path $WorkDirectory -ErrorAction Stop | Out-Null
$work = (Resolve-Path -LiteralPath $WorkDirectory).Path
function Expect-Failure([scriptblock]$Action, [string]$Message) {
    try { & $Action } catch {
        if ($_.ToString() -notlike "*$Message*") { throw "Unexpected failure: $_" }
        return
    }
    throw "Expected failure containing: $Message"
}

# The actual native MSI/tool, not an alternate PATH pkg-config installation.
$cache = Join-Path $work 'tool cache with spaces'
$native = Get-JudasPkgConfig -CacheRoot $cache
$cached = Get-JudasPkgConfig -CacheRoot $cache
if ($native -ne $cached) { throw 'Cache reuse changed tool selection' }
$override = Get-JudasPkgConfig -Path $native -CacheRoot (Join-Path $work 'unused')
if ($native -ne $override -or (Test-Path (Join-Path $work 'unused'))) { throw 'Explicit override was not respected' }

# Never silently fall back after an explicit tool fails to execute. This models
# failure handling, not a claim to exercise Application Control itself.
$rejected = Join-Path $work 'rejected.cmd'
Set-Content -LiteralPath $rejected -Value '@exit /b 17'
Expect-Failure { Get-JudasPkgConfig -Path $rejected -CacheRoot $cache } 'No alternate executable or MSYS2 fallback'

# Both the cached download and executable must retain their pinned integrity.
$badCache = Join-Path $work 'bad-cache'
New-Item -ItemType Directory -Path (Join-Path $badCache 'pkgconf-x64-3.0.8') -Force | Out-Null
Set-Content -LiteralPath (Join-Path $badCache 'pkgconf-x64-3.0.8/pkgconf-x64-3.0.8.msi') -Value 'corrupt installer'
Expect-Failure { Get-JudasPkgConfig -CacheRoot $badCache } 'checksum mismatch'
$bytes = [IO.File]::ReadAllBytes($native)
try {
    [IO.File]::WriteAllText($native, 'corrupt executable')
    Expect-Failure { Get-JudasPkgConfig -CacheRoot $cache } 'checksum mismatch'
} finally { [IO.File]::WriteAllBytes($native, $bytes) }

# A clean vcpkg install must compile SDL2/GLM from source without any binary
# archive cache or usable ambient PKG_CONFIG. No Judas compilation is needed.
$oldSources = $env:VCPKG_BINARY_SOURCES
$oldPkgConfig = $env:PKG_CONFIG
$oldKeepEnv = $env:VCPKG_KEEP_ENV_VARS
try {
    $env:VCPKG_BINARY_SOURCES = 'clear'
    $env:PKG_CONFIG = $rejected
    $env:VCPKG_KEEP_ENV_VARS = 'JUDAS_BOOTSTRAP_TEST_SENTINEL'
    & "$PSScriptRoot\Build-Windows.ps1" -Generator Ninja -DependenciesOnly -ToolCache $cache -VcpkgRoot (Join-Path $work 'vcpkg') -BuildDirectory (Join-Path $work 'build')
    if ($env:PKG_CONFIG -ne $rejected -or $env:VCPKG_KEEP_ENV_VARS -ne 'JUDAS_BOOTSTRAP_TEST_SENTINEL') { throw 'Build did not restore the caller environment' }
    $msys = @(Get-ChildItem (Join-Path $work 'vcpkg/downloads') -Recurse -Filter '*msys*')
    if ($msys.Count) { throw "Unexpected MSYS2 downloads: $($msys.FullName)" }
    $installed = Join-Path $work 'build/vcpkg_installed/x64-windows-static-md/share'
    foreach ($port in @('sdl2', 'glm')) {
        if (!(Test-Path (Join-Path $installed "$port/copyright"))) { throw "Missing installed dependency: $port" }
    }
} finally {
    $env:VCPKG_BINARY_SOURCES = $oldSources
    $env:PKG_CONFIG = $oldPkgConfig
    $env:VCPKG_KEEP_ENV_VARS = $oldKeepEnv
}
Write-Host 'Windows fresh dependency bootstrap and integrity/failure controls passed.'
