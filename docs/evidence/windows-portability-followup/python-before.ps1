$r='P:\JudasValidation\WindowsSeven'
@"
@echo off
set PATH=C:\JudasValidation\VS2022\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin;%PATH%
set PYTHONUTF8=0
cd /d $r\Source
C:\JudasValidation\Python\python.exe -m unittest discover -s tests -p test_cmake_test_targets.py -v > $r\organization-before.log 2>&1
echo %errorlevel% > $r\organization-before.exit
C:\JudasValidation\Python\python.exe -m unittest discover -s tests -p test_validation_registration.py -v > $r\registration-before.log 2>&1
echo %errorlevel% > $r\registration-before.exit
"@ | Set-Content "$r\python-before.cmd" -Encoding ASCII
& cmd /c "$r\python-before.cmd"
Get-Content "$r\organization-before.log" -Tail 5
Get-Content "$r\registration-before.log" -Tail 5
Get-Content "$r\build-before.log" -Tail 5
