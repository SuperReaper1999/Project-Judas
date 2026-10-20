$ErrorActionPreference='Stop';$r='P:\JudasValidation\CleanDistribution'
& icacls.exe $r /grant 'Conner:(OI)(CI)M' /T /Q|Out-Null
$action=New-ScheduledTaskAction -Execute 'C:\Windows\System32\WindowsPowerShell\v1.0\powershell.exe' -Argument '-NoProfile -ExecutionPolicy Bypass -File P:\JudasValidation\CleanDistribution\windows-desktop.ps1' -WorkingDirectory "$r\Unrelated CWD"
$principal=New-ScheduledTaskPrincipal -UserId Conner -LogonType Interactive -RunLevel Limited
Register-ScheduledTask -TaskName JudasCleanDistributionQualification -Action $action -Principal $principal -Settings (New-ScheduledTaskSettingsSet -ExecutionTimeLimit (New-TimeSpan -Minutes 20)) -Force|Out-Null
Start-ScheduledTask -TaskName JudasCleanDistributionQualification
'Interactive qualification started'
