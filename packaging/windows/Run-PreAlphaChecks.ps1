# Run only in the isolated extracted PreAlphaPass1 directory. No VM configuration,
# existing Source/Build directory, dependency installation or GPU claim is involved.
param([string]$WorkRoot = 'C:\JudasValidation\PreAlphaPass1')
$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'
$source = Join-Path $WorkRoot 'Source'
$build = Join-Path $WorkRoot 'Build'
$logs = Join-Path $WorkRoot 'Results'
New-Item -ItemType Directory -Force $logs | Out-Null
$report = @{ schema = 1; scope = 'Native Windows dependency-light identity/probes and TCP transport; not full engine/SDK or hardware acceptance'; passed = $false; tests = @(); startedUtc = [DateTime]::UtcNow.ToString('o') }

function Check-Inputs {
    $inventory = Get-Content -LiteralPath "$source\JUDAS_SOURCE_FILES.json" -Raw | ConvertFrom-Json
    foreach ($entry in $inventory.files) {
        $path = Join-Path $source $entry.path
        if (!(Test-Path -LiteralPath $path -PathType Leaf)) { throw "Missing input: $($entry.path)" }
        if ((Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant() -ne $entry.sha256) {
            throw "Input fingerprint mismatch: $($entry.path)"
        }
    }
}

function Invoke-Checked([string]$Name, [string]$Program, [string[]]$Arguments) {
    $ErrorActionPreference = 'Continue'
    & $Program @Arguments 2>&1 | Tee-Object -FilePath (Join-Path $logs "$Name.log")
    $code = $LASTEXITCODE
    $ErrorActionPreference = 'Stop'
    $script:report.tests += @{ name = $Name; exit = $code }
    if ($code -ne 0) { throw "$Name failed with exit $code" }
}

try {
    Check-Inputs
    $report.sourceManifestSha256 = (Get-FileHash "$source\JUDAS_SOURCE_FILES.json" -Algorithm SHA256).Hash.ToLowerInvariant()
    $vs = 'C:\JudasValidation\VS2022'
    Import-Module "$vs\Common7\Tools\Microsoft.VisualStudio.DevShell.dll"
    Enter-VsDevShell -VsInstallPath $vs -SkipAutomaticLocation -DevCmdArguments '-arch=x64 -host_arch=x64'
    $cmake = "$vs\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
    Invoke-Checked 'configure' $cmake @('-S', $source, '-B', $build, '-G', 'Visual Studio 17 2022', '-A', 'x64', '-DJUDAS_BUILD_CHANNEL=Experimental')
    Invoke-Checked 'build' $cmake @('--build', $build, '--config', 'Release', '--parallel', '2')
    Invoke-Checked 'build-identity' "$build\Release\judas_build_identity_tests.exe" @()
    Invoke-Checked 'network-transport' "$build\Release\judas_network_transport_tests.exe" @()
    Invoke-Checked 'binary-identity' "$build\Release\judas_build_identity_tests.exe" @('--build-info-json')
    $report.binaries = @{}
    foreach ($name in @('judas_build_identity_tests.exe', 'judas_network_transport_tests.exe')) {
        $report.binaries[$name] = (Get-FileHash "$build\Release\$name" -Algorithm SHA256).Hash.ToLowerInvariant()
    }
    Check-Inputs
    $report.passed = $true
} catch {
    $report.error = $_.Exception.Message
} finally {
    $report.finishedUtc = [DateTime]::UtcNow.ToString('o')
    $report | ConvertTo-Json -Depth 8 | Set-Content -Encoding UTF8 (Join-Path $logs 'RESULTS.json')
}
if (!$report.passed) { exit 1 }
