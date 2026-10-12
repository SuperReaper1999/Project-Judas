$ErrorActionPreference='Stop'; $ProgressPreference='SilentlyContinue'
$root='C:\JudasValidation'; $bin="$root\Build\Release"; $out="$root\M78ContinuationResults"; $base='http://192.168.122.1:8872'
New-Item -ItemType Directory $out -Force|Out-Null
function Upload($p,$name){Invoke-WebRequest -UseBasicParsing "$base/$name" -Method Post -InFile $p|Out-Null}
try {
  $inventory="$out\m78-continuation-old-exes.json"
  if(Test-Path -LiteralPath $inventory){throw 'Preserve prior continuation inventory'}
  $running=@(Get-Process | Where-Object {$_.Path -and $_.Path.StartsWith($bin+'\',[StringComparison]::OrdinalIgnoreCase)} | ForEach-Object {$_.Path})
  $rows=@(Get-ChildItem -LiteralPath $bin -File -Filter *.exe | ForEach-Object {
    if($_.Length -gt 16MB){throw ('Archive exceeds bounded upload: '+$_.Name)}
    if($running -contains $_.FullName){throw ('Owned executable is currently running: '+$_.Name)}
    @{name=$_.Name;path=$_.FullName;bytes=$_.Length;sha256=(Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash.ToLower();archiveName=('m78-continuation-old-'+$_.Name)}
  })
  @{scope='Read-only inventory and direct host archive of generated Build/Release executables only; no deletion in this phase';freeBytes=(Get-PSDrive C).Free;rows=$rows}|ConvertTo-Json -Depth 5|Set-Content -LiteralPath $inventory -Encoding UTF8
  Upload $inventory 'm78-continuation-old-exes.json'
  foreach($row in $rows){Upload $row.path $row.archiveName}
  @{archiveUploads=$rows.Count;freeBytes=(Get-PSDrive C).Free;status='Awaiting independent host SHA verification; no guest file removed'}|ConvertTo-Json -Depth 4|Set-Content "$out\m78-continuation-old-exes-upload.json" -Encoding UTF8
  Upload "$out\m78-continuation-old-exes-upload.json" 'm78-continuation-old-exes-upload.json'
} catch {
  [string]$_.Exception.Message|Set-Content "$out\m78-continuation-old-exes-error.txt"
  Upload "$out\m78-continuation-old-exes-error.txt" 'm78-continuation-old-exes-error.txt'
  exit 1
}
