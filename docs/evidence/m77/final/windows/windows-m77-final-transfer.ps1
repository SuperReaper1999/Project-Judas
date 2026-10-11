$ErrorActionPreference='Stop';$ProgressPreference='SilentlyContinue';$root='C:\JudasValidation';$base='http://192.168.122.1:8872';$receipts="$root\Receipts\M77"
function Upload($p){Invoke-WebRequest -UseBasicParsing "$base/$([IO.Path]::GetFileName($p))" -Method Post -InFile $p|Out-Null}
try{
 Invoke-WebRequest -UseBasicParsing "$base/m77-final-validation-delta.json" -OutFile "$root\m77-final-validation-delta.json";Invoke-WebRequest -UseBasicParsing "$base/m77-final-validation-delta.zip" -OutFile "$root\m77-final-validation-delta.zip"
 Add-Type -AssemblyName System.IO.Compression.FileSystem;$archive=[IO.Compression.ZipFile]::OpenRead("$root\m77-final-validation-delta.zip");$changed=Get-Content "$root\m77-final-validation-delta.json" -Raw|ConvertFrom-Json
 try{foreach($file in $changed.files){$dest=Join-Path "$root\Source" $file.path;New-Item -ItemType Directory -Force ([IO.Path]::GetDirectoryName($dest))|Out-Null;[IO.Compression.ZipFileExtensions]::ExtractToFile($archive.GetEntry($file.path),$dest,$true);if((Get-FileHash $dest).Hash.ToLower() -ne $file.sha256){throw "delta mismatch $($file.path)"}}}finally{$archive.Dispose()}
 Invoke-WebRequest -UseBasicParsing "$base/m77-source-verified-final.json" -OutFile "$root\m77-source-verified-final.json";$expected=Get-Content "$root\m77-source-verified-final.json" -Raw|ConvertFrom-Json;foreach($file in $expected.files){if((Get-FileHash (Join-Path "$root\Source" $file.path)).Hash.ToLower() -ne $file.sha256){throw "final source mismatch $($file.path)"}}
 @{verifiedFiles=$expected.files.Count;deltaFiles=$changed.files.Count;freeBytes=(Get-PSDrive C).Free}|ConvertTo-Json|Set-Content "$receipts\m77-windows-validation-source-proof.json" -Encoding UTF8;Upload "$receipts\m77-windows-validation-source-proof.json";Remove-Item "$root\m77-final-validation-delta.zip"
exit 0
}catch{throw $_}
