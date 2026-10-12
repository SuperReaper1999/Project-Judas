param([string]$Label='',[string]$CaseId='')
$ErrorActionPreference='Stop';$ProgressPreference='SilentlyContinue';$root='C:\JudasValidation';$out="$root\M78ContinuationResults";$base='http://192.168.122.1:8872'
function Upload($p,$name){Invoke-WebRequest -UseBasicParsing "$base/$name" -Method Post -InFile $p|Out-Null}
try {
 if($Label -notmatch '^[a-z0-9_-]+$' -or $CaseId -notmatch '^[a-z0-9_-]+$'){throw 'Explicit owned label/case required'};$name="m78-$Label-$CaseId-archive"
 $m=Get-Content "$out\$name.json" -Raw|ConvertFrom-Json;$approval=(Invoke-WebRequest -UseBasicParsing "$base/$name-approved.json").Content|ConvertFrom-Json
 if(!$approval.hostVerified -or $approval.label -ne $Label -or $approval.case -ne $CaseId -or $approval.archiveSHA256 -ne $m.archiveSHA256 -or $approval.files.Count -ne $m.files.Count){throw 'Independent exact host verification missing'}
 $expectedExe="$root\Build\Release\m78_${Label}_${CaseId}.exe";$prefix="$root\M78ContinuationBuild\$Label\objects\$CaseId-"
 foreach($p in @(Get-Process|Where-Object {$_.Path -eq $expectedExe})){throw 'Owned executable is running'}
 foreach($file in $m.files){if($file.path -ne $expectedExe -and !$file.path.StartsWith($prefix,[StringComparison]::OrdinalIgnoreCase)){throw 'Unowned cleanup path'};$approved=@($approval.files|Where-Object {$_.path -eq $file.path -and $_.sha256 -eq $file.sha256 -and $_.bytes -eq $file.bytes});if($approved.Count -ne 1 -or (Get-Item -LiteralPath $file.path).Length -ne $file.bytes -or (Get-FileHash -LiteralPath $file.path).Hash.ToLower() -ne $file.sha256){throw 'Changed or unverified owned file'}}
 foreach($file in $m.files){Remove-Item -LiteralPath $file.path}
 $zip="$out\$name.zip";if((Get-FileHash $zip).Hash.ToLower() -ne $m.archiveSHA256){throw 'Changed generated archive; preserve'};Remove-Item -LiteralPath $zip
 $receipt="$out\m78-$Label-$CaseId-cleanup.json";@{scope='Only independently archived exact owned completed executable and its case objects removed; sources, libraries, DLLs and receipts preserved';files=$m.files;archiveSHA256=$m.archiveSHA256;freeBytes=(Get-PSDrive C).Free}|ConvertTo-Json -Depth 5|Set-Content $receipt -Encoding UTF8;Upload $receipt ([IO.Path]::GetFileName($receipt))
}catch{$name="m78-$Label-$CaseId-cleanup-error.txt";[string]$_.Exception.Message|Set-Content "$out\$name";Upload "$out\$name" $name;exit 1}
