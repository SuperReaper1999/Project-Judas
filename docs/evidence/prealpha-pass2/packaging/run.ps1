$ErrorActionPreference='Stop';$ProgressPreference='SilentlyContinue'
$root='C:\JudasValidation\PreAlphaPass2'
if((Get-FileHash "$root\windows-functional.zip" -Algorithm SHA256).Hash.ToLowerInvariant() -ne '55834f7086ceccab05d954cc13845a950f2d177e32ac7b61a0c744a4ee6bcc1f'){throw 'Archive hash mismatch'}
if(Test-Path "$root\Source"){throw 'Task source already exists; preserve earlier run'}
Expand-Archive -LiteralPath "$root\windows-functional.zip" -DestinationPath "$root\Source"
& "$root\Source\Run-PreAlphaChecks.ps1" -WorkRoot $root
exit $LASTEXITCODE
