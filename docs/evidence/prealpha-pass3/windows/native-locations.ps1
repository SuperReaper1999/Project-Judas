$ProgressPreference='SilentlyContinue'
$ErrorActionPreference='Stop'
$roots=@('C:\JudasValidation\Build','C:\JudasValidation\M78ContinuationBuild','C:\JudasValidation\PreAlphaPass2','C:\JudasValidation\vcpkg','C:\JudasValidation\Source\third_party')
$roots|ForEach-Object { if(Test-Path $_){Get-ChildItem $_ -Directory|Select-Object -ExpandProperty FullName} }
Get-ChildItem C:\JudasValidation\VS2022\VC\Tools\MSVC -Directory|Select-Object -ExpandProperty FullName
