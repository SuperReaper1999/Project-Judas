$ProgressPreference='SilentlyContinue'
$ErrorActionPreference='Stop'
New-Item -ItemType Directory -Force C:\JudasValidation\PreAlphaPass3 | Out-Null
Get-ChildItem 'C:\JudasValidation\VS2022\VC\Auxiliary\Build\vcvars64.bat','C:\JudasValidation\Python\python.exe','C:\JudasValidation\Tooling\*','C:\JudasValidation\vcpkg\installed\*','C:\JudasValidation\vcpkg\installed\*\include\glm' -ErrorAction SilentlyContinue | Select-Object FullName,Length | ConvertTo-Json
