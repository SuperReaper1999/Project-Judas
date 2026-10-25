$r='P:\JudasValidation\WindowsSeven'
@"
@echo off
call C:\JudasValidation\VS2022\VC\Auxiliary\Build\vcvars64.bat
cd /d $r
cl /nologo /O2 /std:c++17 /EHsc /utf-8 /DNOMINMAX /DWIN32_LEAN_AND_MEAN /I Source\src Source\src\NetworkTransport.cpp Source\tests\NetworkTransportTests.cpp /link /OUT:network-before.exe ws2_32.lib > network-before-build.log 2>&1
if errorlevel 1 exit /b 1
network-before.exe > network-before.log 2>&1
echo %errorlevel% > network-before.exit
"@ | Set-Content "$r\network-before.cmd" -Encoding ASCII
& cmd /c "$r\network-before.cmd"
Get-Content "$r\network-before.log" | Select-String 'FAIL|SUMMARY'
