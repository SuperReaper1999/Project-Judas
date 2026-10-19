$ErrorActionPreference='Stop';$ProgressPreference='SilentlyContinue';$root='P:\JudasValidation\WindowsCross'
Add-Type -AssemblyName System.IO.Compression.FileSystem
[IO.Compression.ZipFile]::ExtractToDirectory("$root\qualification.zip","$root\Qualification")
[IO.Compression.ZipFile]::ExtractToDirectory("$root\Hearthworld-Windows.zip","$root\Pristine Package")
Copy-Item "$root\Pristine Package" "$root\Moved Package résumé" -Recurse
$identity=Get-Content "$root\TRANSFER.json" -Raw|ConvertFrom-Json
$checked=@();foreach($property in $identity.PSObject.Properties){
 $path=if($property.Name -eq 'qualification.zip'){"$root\qualification.zip"}else{"$root\Qualification\$($property.Name)"}
 $hash=(Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant()
 if($hash -ne $property.Value){throw "Transfer hash mismatch: $path"};$checked+=@{file=$property.Name;sha256=$hash}
}
$checks=@();foreach($row in (Get-Content "$root\Pristine Package\WINDOWS_PACKAGE.json" -Raw|ConvertFrom-Json).files){
 $path=Join-Path "$root\Pristine Package" $row.path
 $hash=(Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant()
 if($hash -ne $row.sha256){throw "Package file mismatch $path"};$checks+=@{file=$row.path;sha256=$hash}
}
$imports=@();foreach($dll in (Get-Content "$root\Pristine Package\WINDOWS_PACKAGE.json" -Raw|ConvertFrom-Json).systemImports){
 if($dll -match '^(api|ext)-ms-'){$imports+=@{name=$dll;apiSet=$true};continue}
 $p="C:\Windows\System32\$dll";if(!(Test-Path $p)){throw "Missing Windows system dependency: $dll"};$imports+=@{name=$dll;path=$p}
}
# Only disposable qualification copies receive VM-specific software graphics.
foreach($dir in @("$root\Qualification","$root\Moved Package résumé")){
 Copy-Item 'C:\JudasValidation\Mesa\x64\opengl32.dll' $dir
 Copy-Item 'C:\JudasValidation\Mesa\x64\libgallium_wgl.dll' $dir
}
& icacls.exe $root /grant 'Conner:(OI)(CI)M' /T /Q|Out-Null
@{transferred=$checked;package=$checks;systemImports=$imports;mesaAddedOnlyToCopies=$true}|ConvertTo-Json -Depth 6|Set-Content -Encoding UTF8 "$root\Results\TRANSFER_VERIFIED.json"
Start-ScheduledTask -TaskName JudasWindowsCrossQualification
'Qualification task launched; no existing VM applications stopped'
