param([string]$Label='',[string]$CaseId='')
$ErrorActionPreference='Stop';$ProgressPreference='SilentlyContinue';$root='C:\JudasValidation';$out="$root\M78ContinuationResults";$base='http://192.168.122.1:8872'
function Upload($p,$name){Invoke-WebRequest -UseBasicParsing "$base/$name" -Method Post -InFile $p|Out-Null}
try {
 if($Label -notmatch '^[a-z0-9_-]+$' -or $CaseId -notmatch '^[a-z0-9_-]+$'){throw 'Explicit owned label/case required'}
 $name="m78-$Label-$CaseId-archive";$receipt="$out\$name.json";if(Test-Path $receipt){throw 'Preserve prior archive receipt'}
 $compiled=Get-Content "$out\m78-$Label-$CaseId.json" -Raw|ConvertFrom-Json;$scratch="$root\M78ContinuationBuild\$Label";$files=@();$paths=@($compiled.executable)+@($compiled.compiles|ForEach-Object {$_.object})
 $expectedExe="$root\Build\Release\m78_${Label}_${CaseId}.exe";if($compiled.executable -ne $expectedExe -or (Get-FileHash $expectedExe).Hash.ToLower() -ne $compiled.executableSHA256){throw 'Owned case executable changed'}
 foreach($p in @(Get-Process|Where-Object {$_.Path -eq $expectedExe})){throw 'Owned case executable still running'}
 foreach($path in $paths){if($path -ne $expectedExe -and !$path.StartsWith("$scratch\objects\$CaseId-",[StringComparison]::OrdinalIgnoreCase)){throw 'Not an owned case object'};$files+=@{path=$path;name=[IO.Path]::GetFileName($path);bytes=(Get-Item -LiteralPath $path).Length;sha256=(Get-FileHash -LiteralPath $path).Hash.ToLower()}}
 $zip="$out\$name.zip";if(Test-Path $zip){throw 'Preserve prior case archive'};Compress-Archive -LiteralPath $paths -DestinationPath $zip -CompressionLevel Optimal
 if((Get-Item $zip).Length -gt 16MB){throw 'Case archive exceeds existing transfer endpoint bound; preserve files'}
 @{scope='Archive only; no cleanup until independent host verification';label=$Label;case=$CaseId;archiveName=[IO.Path]::GetFileName($zip);archiveSHA256=(Get-FileHash $zip).Hash.ToLower();archiveBytes=(Get-Item $zip).Length;files=$files;freeBytes=(Get-PSDrive C).Free}|ConvertTo-Json -Depth 5|Set-Content $receipt -Encoding UTF8
 Upload $zip ([IO.Path]::GetFileName($zip));Upload $receipt ([IO.Path]::GetFileName($receipt))
}catch{$name="m78-$Label-$CaseId-archive-error.txt";[string]$_.Exception.Message|Set-Content "$out\$name";Upload "$out\$name" $name;exit 1}
