$ErrorActionPreference='Stop';$ProgressPreference='SilentlyContinue';$task='P:\JudasValidation\PreAlphaPass5';$env:PYTHONDONTWRITEBYTECODE='1';$python='C:\JudasValidation\Python\python.exe'
& $python "$task\final-integrity.py"
if($LASTEXITCODE-ne 0){exit $LASTEXITCODE}
$name='JudasValidation-PreAlphaPass5-Gui'
if((Get-ScheduledTask -TaskName $name).State-eq 'Running'){throw 'Desktop task still running'}
Unregister-ScheduledTask -TaskName $name -Confirm:$false
[pscustomobject]@{utc=[DateTime]::UtcNow.ToString('o');ownedTransientDesktopTaskRemoved=$true;drives=@(Get-PSDrive C,P|Select-Object Name,Free,Used);remainingTask=@(Get-ScheduledTask -TaskName $name -ErrorAction SilentlyContinue|Select-Object TaskName,State);preservedScope='No VM configuration, installed toolchain, Windows system/personal data or unrelated scheduled task changes'}|ConvertTo-Json -Depth 4|Set-Content -Encoding UTF8 "$task\FINAL_GUEST_STATE.json"
& $python "$task\collect-evidence.py"
exit $LASTEXITCODE
