param([string]$ManifestName='m78-continuation-immutable-test-inputs-v1.json')
$ErrorActionPreference='Stop';$ProgressPreference='SilentlyContinue';$root='C:\JudasValidation';$base='http://192.168.122.1:8872';$out="$root\M78ContinuationResults"
function Upload($p,$name){Invoke-WebRequest -UseBasicParsing "$base/$name" -Method Post -InFile $p|Out-Null}
try{
 if($ManifestName -ne 'm78-continuation-immutable-test-inputs-v1.json'){throw 'Only the reviewed original-input bundle is permitted'}
 $m=(Invoke-WebRequest -UseBasicParsing "$base/$ManifestName").Content|ConvertFrom-Json;$scratch="$root\M78ContinuationSource\ImmutableInputsV1"
 if(Test-Path $scratch){throw 'Preserve preceding staging directory'}
 if((Get-PSDrive C).Free -lt ($m.archiveBytes+$m.unpackedBytes+48MB)){throw 'Insufficient bounded source-staging reserve'}
 New-Item -ItemType Directory "$scratch\payload","$scratch\prior" -Force|Out-Null;$zip="$scratch\inputs.zip"
 Invoke-WebRequest -UseBasicParsing "$base/$($m.archiveName)" -OutFile $zip;if((Get-FileHash $zip).Hash.ToLower() -ne $m.archiveSHA256){throw 'Original-input archive SHA mismatch'}
 Expand-Archive -LiteralPath $zip -DestinationPath "$scratch\payload"
 foreach($row in $m.files){if($row.path -match '^[/\\]' -or $row.path -match '\.\.' -or $row.path -match ':'){throw 'Invalid relative immutable input'};$p=Join-Path "$scratch\payload" $row.path;if((Get-FileHash $p).Hash.ToLower() -ne $row.sha256){throw 'Immutable input SHA mismatch'}}
 $rows=@();foreach($row in $m.files){$source=Join-Path "$scratch\payload" $row.path;$destination=Join-Path "$root\Source" $row.path;$prior='';$action='already exact'
  if(Test-Path -LiteralPath $destination){$prior=(Get-FileHash -LiteralPath $destination).Hash.ToLower();if($prior -ne $row.sha256){$backup=Join-Path "$scratch\prior" $row.path;New-Item -ItemType Directory ([IO.Path]::GetDirectoryName($backup)) -Force|Out-Null;Copy-Item -LiteralPath $destination -Destination $backup;$action='prior guest input preserved, exact original source staged'}}else{$action='missing original input staged'}
  if($prior -ne $row.sha256){New-Item -ItemType Directory ([IO.Path]::GetDirectoryName($destination)) -Force|Out-Null;Copy-Item -LiteralPath $source -Destination $destination}
  if((Get-FileHash -LiteralPath $destination).Hash.ToLower() -ne $row.sha256){throw 'Final staged immutable input SHA mismatch'};$rows+=@{path=$row.path;sha256=$row.sha256;bytes=$row.bytes;priorSHA256=$prior;action=$action}
 }
 $receipt="$out\m78-continuation-immutable-input-stage-v1.json";@{scope=$m.scope;archiveSHA256=$m.archiveSHA256;files=$rows;freeBytes=(Get-PSDrive C).Free}|ConvertTo-Json -Depth 6|Set-Content $receipt -Encoding UTF8;Upload $receipt ([IO.Path]::GetFileName($receipt))
}catch{$errorPath="$out\m78-continuation-immutable-input-stage-v1-error.txt";[string]$_.Exception.Message|Set-Content $errorPath;Upload $errorPath ([IO.Path]::GetFileName($errorPath));exit 1}
