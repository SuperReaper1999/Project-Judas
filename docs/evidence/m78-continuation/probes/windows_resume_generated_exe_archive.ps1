$ErrorActionPreference='Stop'; $ProgressPreference='SilentlyContinue'
$out='C:\JudasValidation\M78ContinuationResults'; $base='http://192.168.122.1:8872'
function Upload($p,$name){Invoke-WebRequest -UseBasicParsing "$base/$name" -Method Post -InFile $p|Out-Null}
try {
  $inventory="$out\m78-continuation-old-exes.json"
  Upload $inventory 'm78-continuation-old-exes.json'
  if(Test-Path "$out\m78-continuation-old-exes-error.txt"){Upload "$out\m78-continuation-old-exes-error.txt" 'm78-continuation-old-exes-error.txt'}
  $original=Get-Content -LiteralPath $inventory -Raw|ConvertFrom-Json
  foreach($row in $original.rows){
    if(!(Test-Path -LiteralPath $row.path)){throw ('Inventory member is missing: '+$row.name)}
    if((Get-FileHash -LiteralPath $row.path).Hash.ToLower() -ne $row.sha256){throw ('Inventory member changed: '+$row.name)}
    Upload $row.path $row.archiveName
  }
  @{archiveUploads=$original.rows.Count;freeBytes=(Get-PSDrive C).Free;status='Resumed after local transfer server was unavailable; awaiting independent host SHA verification; no guest file removed'}|ConvertTo-Json -Depth 4|Set-Content "$out\m78-continuation-old-exes-resumed.json" -Encoding UTF8
  Upload "$out\m78-continuation-old-exes-resumed.json" 'm78-continuation-old-exes-resumed.json'
}catch {
  [string]$_.Exception.Message|Set-Content "$out\m78-continuation-old-exes-resume-error.txt"
  Upload "$out\m78-continuation-old-exes-resume-error.txt" 'm78-continuation-old-exes-resume-error.txt'
  exit 1
}
