# CI-only qualification: stock SDK stays pristine; Mesa is test-copy-only.
param([Parameter(Mandatory)][string]$Sdk, [Parameter(Mandatory)][string]$Mesa,
      [Parameter(Mandatory)][string]$WorkDirectory)
$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path "$PSScriptRoot/../..").Path
if (!$env:CI -or $repo -ne $env:GITHUB_WORKSPACE) { throw 'Source hiding is only for an isolated GitHub Actions checkout' }
$hidden = "$repo-qualification-hidden"
if (Test-Path $hidden) { throw 'Source-hiding destination already exists' }
New-Item -ItemType Directory -Path "$WorkDirectory/driver/scripts", "$WorkDirectory/driver/tools", "$WorkDirectory/driver/tests/fixtures" -Force | Out-Null
Copy-Item "$repo/scripts/qualify_sdk.py", "$repo/scripts/qualify_sdk_workflows.py" "$WorkDirectory/driver/scripts/"
Copy-Item "$repo/tools/build_linux_baseline.py" "$WorkDirectory/driver/tools/"
Copy-Item "$repo/tests/fixtures/rendering" "$WorkDirectory/driver/tests/fixtures/" -Recurse
Copy-Item $Sdk "$WorkDirectory/sdk" -Recurse
$originalDirectory = [Environment]::CurrentDirectory
$moved = $false
Push-Location $WorkDirectory
try {
    [Environment]::CurrentDirectory = $WorkDirectory
    Move-Item -LiteralPath $repo -Destination $hidden
    $moved = $true
    if (Test-Path $repo) { throw 'Source checkout is still visible at its compiled path' }
    & python "$WorkDirectory/driver/scripts/qualify_sdk.py" --sdk "$WorkDirectory/sdk" --work "$WorkDirectory/workflows" --mesa $Mesa
    if ($LASTEXITCODE) { throw "Source-hidden Windows SDK workflows failed: $LASTEXITCODE" }
} finally {
    if ($moved) { Move-Item -LiteralPath $hidden -Destination $repo }
    [Environment]::CurrentDirectory = $originalDirectory
    Pop-Location
}
