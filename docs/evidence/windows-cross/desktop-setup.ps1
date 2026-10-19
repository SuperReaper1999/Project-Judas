$ErrorActionPreference='Stop';$ProgressPreference='SilentlyContinue';$root='P:\JudasValidation\WindowsCross'
New-Item -ItemType Directory -Force "$root\Results","$root\UnrelatedCwd"|Out-Null
& icacls.exe $root /grant 'Conner:(OI)(CI)M' /T /Q |Out-Null
$action=New-ScheduledTaskAction -Execute 'C:\Windows\System32\WindowsPowerShell\v1.0\powershell.exe' -Argument '-NoProfile -ExecutionPolicy Bypass -File P:\JudasValidation\WindowsCross\desktop-run.ps1' -WorkingDirectory "$root\UnrelatedCwd"
$principal=New-ScheduledTaskPrincipal -UserId 'Conner' -LogonType Interactive -RunLevel Limited
Register-ScheduledTask -TaskName 'JudasWindowsCrossQualification' -Action $action -Principal $principal -Settings (New-ScheduledTaskSettingsSet -ExecutionTimeLimit (New-TimeSpan -Minutes 30)) -Force|Out-Null
[pscustomobject]@{task='JudasWindowsCrossQualification';os=(Get-CimInstance Win32_OperatingSystem|Select-Object Caption,Version,BuildNumber);explorer=(Get-Process explorer|Select-Object Id,SessionId)}|ConvertTo-Json -Depth 4
