$ErrorActionPreference='Stop';$ProgressPreference='SilentlyContinue';$root='C:\JudasValidation';$base='http://192.168.122.1:8872';$receipts="$root\Receipts\M77"
New-Item -ItemType Directory -Force $receipts|Out-Null
function Upload($p){Invoke-WebRequest -UseBasicParsing "$base/$([IO.Path]::GetFileName($p))" -Method Post -InFile $p|Out-Null}
try{
 Invoke-WebRequest -UseBasicParsing "$base/m77-source.json" -OutFile "$root\m77-source.json"
 Invoke-WebRequest -UseBasicParsing "$base/m77-source.zip" -OutFile "$root\m77-source.zip"
 $expected=Get-Content "$root\m77-source.json" -Raw|ConvertFrom-Json;$changed=0
 Add-Type -AssemblyName System.IO.Compression.FileSystem
 $archive=[IO.Compression.ZipFile]::OpenRead("$root\m77-source.zip")
 try{foreach($file in $expected.files){$dest=Join-Path "$root\Source" $file.path
  if(!(Test-Path $dest) -or (Get-FileHash $dest).Hash.ToLower() -ne $file.sha256){New-Item -ItemType Directory -Force ([IO.Path]::GetDirectoryName($dest))|Out-Null;$entry=$archive.GetEntry($file.path);[IO.Compression.ZipFileExtensions]::ExtractToFile($entry,$dest,$true);$changed++}
  if((Get-FileHash $dest).Hash.ToLower() -ne $file.sha256){throw "source mismatch $($file.path)"}
 }}finally{$archive.Dispose()}
 @{base=$expected.base;verifiedFiles=$expected.files.Count;changedFiles=$changed;freeBytes=(Get-PSDrive C).Free}|ConvertTo-Json -Depth 5|Set-Content "$receipts\m77-windows-source-proof.json";Upload "$receipts\m77-windows-source-proof.json"
 Remove-Item "$root\m77-source.zip"
 Import-Module "$root\VS2022\Common7\Tools\Microsoft.VisualStudio.DevShell.dll";Enter-VsDevShell -VsInstallPath "$root\VS2022" -SkipAutomaticLocation -DevCmdArguments '-arch=x64 -host_arch=x64'
 $env:PATH="$root\Git\cmd;$root\VS2022\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin;$env:PATH";$ErrorActionPreference='Continue'
 & cmake -S "$root\Source" -B "$root\Build" 2>&1|Tee-Object "$receipts\m77-windows-configure.log";$code=$LASTEXITCODE;Upload "$receipts\m77-windows-configure.log";if($code){throw "configure $code"}
 & cmake --build "$root\Build" --config Release --target judas judas_editor judas_export judas_physical_balance_foundation_tests judas_physical_balance_controller_tests judas_balance_lab_tests judas_physical_animation_tests judas_character_motor_tests --parallel 2 2>&1|Tee-Object "$receipts\m77-windows-build.log";$code=$LASTEXITCODE;Upload "$receipts\m77-windows-build.log";if($code){throw "build $code"}
 $ErrorActionPreference='Stop';@{exitcode=$code;freeBytes=(Get-PSDrive C).Free}|ConvertTo-Json|Set-Content "$receipts\m77-windows-build-summary.json";Upload "$receipts\m77-windows-build-summary.json";exit 0
}catch{$_|Out-String|Set-Content "$receipts\m77-windows-build-failure.log";Upload "$receipts\m77-windows-build-failure.log";exit 1}
