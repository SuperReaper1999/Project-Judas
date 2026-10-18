$ErrorActionPreference='Stop';$task='P:\JudasValidation\PreAlphaPass4';$work="$task\GuiWork";$name='JudasValidation-PreAlphaPass4-Gui'
if(Get-ScheduledTask -TaskName $name -ErrorAction SilentlyContinue){throw 'Task already exists; preserve previous run'}
New-Item -ItemType Directory -Force $work|Out-Null
$account=(Get-CimInstance Win32_ComputerSystem).UserName
if(!$account){throw 'Ordinary Windows desktop is unavailable'}
& icacls.exe $work /grant ($account+':(OI)(CI)M')|Out-Null
$action=New-ScheduledTaskAction -Execute 'C:\Windows\System32\WindowsPowerShell\v1.0\powershell.exe' -Argument ('-NoProfile -ExecutionPolicy Bypass -File "'+$task+'\gui-qualify.ps1"') -WorkingDirectory $work
$principal=New-ScheduledTaskPrincipal -UserId $account -LogonType Interactive -RunLevel Limited
$settings=New-ScheduledTaskSettingsSet -ExecutionTimeLimit (New-TimeSpan -Minutes 30)
Register-ScheduledTask -TaskName $name -Action $action -Principal $principal -Settings $settings|Out-Null
Start-ScheduledTask -TaskName $name
[pscustomobject]@{task=$name;principal=$account;runLevel='Limited';guiWorkOnlyAclGranted=$work;utc=[DateTime]::UtcNow.ToString('o')}|ConvertTo-Json|Set-Content -Encoding UTF8 "$task\GUI_TASK.json"
