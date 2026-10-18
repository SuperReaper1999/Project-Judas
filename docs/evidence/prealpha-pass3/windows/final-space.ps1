$ProgressPreference='SilentlyContinue';$ErrorActionPreference='Stop'
$partial='C:\JudasValidation\PreAlphaPass3\focused-source.zip'
$partialBytes=if(Test-Path $partial){(Get-Item $partial).Length}else{0}
[pscustomobject]@{utc=(Get-Date).ToUniversalTime().ToString('o');freeBytes=(Get-CimInstance Win32_LogicalDisk -Filter "DeviceID='C:'").FreeSpace;partialUploadBytes=$partialBytes;scope='Pass3 current engine build and native tests not executed; source upload failed with disk-full error';processes=@(Get-Process -Name cl,link,cmake,ninja,TiWorker -ErrorAction SilentlyContinue|Select-Object ProcessName,Id)}|ConvertTo-Json -Depth 4
if(Test-Path $partial){Remove-Item -LiteralPath $partial}
