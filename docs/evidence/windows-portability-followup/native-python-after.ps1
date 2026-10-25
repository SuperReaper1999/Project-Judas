$ErrorActionPreference='Continue';$ProgressPreference='SilentlyContinue'
$r='P:\JudasValidation\WindowsSeven'
New-Item -ItemType Directory -Force "$r\PythonCandidate" | Out-Null
Copy-Item "$r\Source\CMakeLists.txt" "$r\PythonCandidate"
foreach($p in @('cmake','src','tests','tools','scripts')) { Copy-Item "$r\Source\$p" "$r\PythonCandidate\$p" -Recurse }
Invoke-WebRequest http://192.168.122.1:8765/candidate.zip -OutFile "$r\candidate.zip"
Expand-Archive "$r\candidate.zip" "$r\PythonCandidate" -Force
@"
@echo off
set PATH=C:\JudasValidation\VS2022\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin;C:\JudasValidation\VS2022\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja;%PATH%
set PYTHONUTF8=0
cd /d $r\PythonCandidate
C:\JudasValidation\Python\python.exe -m unittest discover -s tests -p test_cmake_test_targets.py -v > $r\organization-after.log 2>&1
echo %errorlevel% > $r\organization-after.exit
C:\JudasValidation\Python\python.exe -m unittest discover -s tests -p test_validation_registration.py -v > $r\registration-after.log 2>&1
echo %errorlevel% > $r\registration-after.exit
"@ | Set-Content "$r\python-after.cmd" -Encoding ASCII
& cmd /c "$r\python-after.cmd"
Get-Content "$r\organization-after.log" -Tail 5
Get-Content "$r\registration-after.log" -Tail 5
