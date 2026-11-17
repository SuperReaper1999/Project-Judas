# Build-time tool only. Keep this preparation shared by developers and CI.
function Assert-JudasToolHash([string]$Path, [string]$Expected) {
    if ((Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash -ne $Expected) {
        throw "Dependency checksum mismatch: $Path. The cached file was left untouched; do not execute it."
    }
}

function Test-JudasPkgConfig([string]$Path) {
    try {
        $version = & $Path --version
        if ($LASTEXITCODE -ne 0) { throw "exit $LASTEXITCODE" }
        $version = ($version -join "`n").Trim()
        if ($version -notmatch '^\d+\.\d+(\.\d+)?$') {
            throw "Expected a numeric pkg-config version; received '$version'"
        }
    } catch {
        throw "Cannot execute pkg-config tool '$Path': $_. No alternate executable or MSYS2 fallback will be tried. If Windows Application Control blocks this tool, stop and ask your administrator for an approved build tool; do not disable or bypass the policy."
    }
    Write-Host "Build-time pkg-config: $Path ($version)"
}

function Get-JudasPkgConfig([string]$Path, [string]$CacheRoot) {
    if ($Path) {
        $resolved = (Resolve-Path -LiteralPath $Path).Path
        Test-JudasPkgConfig $resolved
        return $resolved
    }
    # Upstream native x64 release: no shell, MSYS2 runtime or machine installation.
    # Verify both the MSI and executable on every reuse, not merely a cache marker.
    $directory = Join-Path $CacheRoot 'pkgconf-x64-3.0.8'
    $installer = Join-Path $directory 'pkgconf-x64-3.0.8.msi'
    $payload = Join-Path $directory 'payload'
    New-Item -ItemType Directory -Force -Path $directory | Out-Null
    if (!(Test-Path -LiteralPath $installer)) {
        $partial = "$installer.download"
        Invoke-WebRequest 'https://github.com/pkgconf/pkgconf/releases/download/pkgconf-3.0.8/pkgconf-x64-3.0.8.msi' -UseBasicParsing -OutFile $partial
        Assert-JudasToolHash $partial 'a236479bbe403b202816745bef19afe2d6240606f28ab1f008ba0f33fbfae43e'
        Move-Item -LiteralPath $partial -Destination $installer
    }
    Assert-JudasToolHash $installer 'a236479bbe403b202816745bef19afe2d6240606f28ab1f008ba0f33fbfae43e'
    $executables = @(Get-ChildItem -LiteralPath $payload -Recurse -Filter pkgconf.exe -ErrorAction SilentlyContinue)
    if (!$executables.Count) {
        $log = Join-Path $directory 'extract.log'
        # Administrative extraction only; never /i (installation) or elevation.
        $extract = Start-Process msiexec.exe -ArgumentList @('/a', "`"$installer`"", '/qn', '/L*V', "`"$log`"", "TARGETDIR=`"$payload`"") -Wait -PassThru
        if ($extract.ExitCode -ne 0) { throw "pkgconf extraction failed: exit $($extract.ExitCode). Inspect $log; no fallback was attempted." }
        $executables = @(Get-ChildItem -LiteralPath $payload -Recurse -Filter pkgconf.exe)
    }
    if ($executables.Count -ne 1) { throw "Expected exactly one native pkgconf.exe under $payload" }
    $resolved = $executables[0].FullName
    Assert-JudasToolHash $resolved '8f7caa492f72b83ef7f6b59317866142c29827a172233ad5edbc670f29dfc194'
    Test-JudasPkgConfig $resolved
    return $resolved
}
