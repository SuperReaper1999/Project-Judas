$ErrorActionPreference='Stop'
$disk=Get-CimInstance Win32_LogicalDisk -Filter "DeviceID='C:'"
$items=@('C:\JudasValidation','C:\Program Files\Microsoft Visual Studio\2022\Community','C:\Program Files\CMake\bin\cmake.exe','C:\JudasValidation\vcpkg','C:\JudasValidation\M78ContinuationBuild')|ForEach-Object { [pscustomobject]@{path=$_;exists=(Test-Path $_)} }
[pscustomobject]@{os=(Get-CimInstance Win32_OperatingSystem).Caption;freeBytes=$disk.FreeSpace;totalBytes=$disk.Size;tools=$items;commands=@('python','cmake','git','ninja'|ForEach-Object { $cmd=Get-Command $_ -ErrorAction SilentlyContinue;[pscustomobject]@{name=$_;path=$cmd.Source} });processes=@(Get-Process|Where-Object {$_.ProcessName -match 'judas|cl|link|cmake|ninja'}|Select-Object ProcessName,Id)}|ConvertTo-Json -Depth 5
