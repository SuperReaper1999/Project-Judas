$ErrorActionPreference='Stop'; $ProgressPreference='SilentlyContinue'
$bin='C:\JudasValidation\Build\Release'; $out='C:\JudasValidation\M78ContinuationResults'; $base='http://192.168.122.1:8872'
function Upload($p,$name){Invoke-WebRequest -UseBasicParsing "$base/$name" -Method Post -InFile $p|Out-Null}
try {
  $receipt="$out\m78-continuation-verified-exe-cleanup.json"
  if(Test-Path $receipt){throw 'Preserve preceding cleanup receipt'}
  $approved=(Invoke-WebRequest -UseBasicParsing "$base/m78-continuation-exe-removal-approved.json").Content|ConvertFrom-Json
  $running=@(Get-Process | Where-Object {$_.Path -and $_.Path.StartsWith($bin+'\',[StringComparison]::OrdinalIgnoreCase)} | ForEach-Object {$_.Path})
  # Verify every approved row before removing any file. No library, DLL or source is eligible.
  foreach($row in $approved.rows){
    if([IO.Path]::GetDirectoryName($row.path) -ne $bin -or [IO.Path]::GetExtension($row.path) -ne '.exe'){throw ('Unexpected removal path '+$row.path)}
    if($running -contains $row.path){throw ('Executable is running '+$row.path)}
    if(!(Test-Path -LiteralPath $row.path)){throw ('Expected generated executable missing '+$row.path)}
    if((Get-Item -LiteralPath $row.path).Length -ne $row.bytes -or (Get-FileHash -LiteralPath $row.path).Hash.ToLower() -ne $row.hostVerifiedSHA256){throw ('Guest/host archive mismatch '+$row.path)}
  }
  $freeBefore=(Get-PSDrive C).Free
  foreach($row in $approved.rows){Remove-Item -LiteralPath $row.path -Force}
  @{scope='Removed only exact generated executables after independent host archive SHA and byte verification; no libraries, DLLs, sources, tools, results or working review package removed';files=$approved.rows;totalBytes=$approved.totalBytes;freeBefore=$freeBefore;freeAfter=(Get-PSDrive C).Free}|ConvertTo-Json -Depth 6|Set-Content -LiteralPath $receipt -Encoding UTF8
  Upload $receipt 'm78-continuation-verified-exe-cleanup.json'
}catch {
  [string]$_.Exception.Message|Set-Content "$out\m78-continuation-verified-exe-cleanup-error.txt"
  Upload "$out\m78-continuation-verified-exe-cleanup-error.txt" 'm78-continuation-verified-exe-cleanup-error.txt'
  exit 1
}
