$ProgressPreference='SilentlyContinue';$root='C:\JudasValidation'
Get-Content "$root\Receipts\M77\m77-windows-bounded-optimizer-build.log" -Tail 8
Get-CimInstance Win32_Process -Filter "Name='cl.exe'" | Select-Object ExecutablePath
Get-PSDrive C | Select-Object Free
