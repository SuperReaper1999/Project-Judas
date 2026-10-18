$ErrorActionPreference='Stop';$ProgressPreference='SilentlyContinue';$task='P:\JudasValidation\PreAlphaPass5';$name='JudasValidation-PreAlphaPass5-Gui'
if((Get-ScheduledTask -TaskName $name).State-eq 'Running'){throw 'Do not interrupt active desktop qualification'}
$action=New-ScheduledTaskAction -Execute 'C:\Windows\System32\WindowsPowerShell\v1.0\powershell.exe' -Argument ('-NoProfile -ExecutionPolicy Bypass -File "'+$task+'\contact-followup.ps1"') -WorkingDirectory "$task\GuiWork"
Set-ScheduledTask -TaskName $name -Action $action|Out-Null
Start-ScheduledTask -TaskName $name
