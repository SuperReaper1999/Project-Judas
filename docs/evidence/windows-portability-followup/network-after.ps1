$r='P:\JudasValidation\WindowsSeven'
$ProgressPreference='SilentlyContinue'
Invoke-WebRequest http://192.168.122.1:8765/candidate.zip -OutFile "$r\candidate-network.zip"
Expand-Archive "$r\candidate-network.zip" "$r\PythonCandidate" -Force
@"
@echo off
call C:\JudasValidation\VS2022\VC\Auxiliary\Build\vcvars64.bat
cd /d $r
cl /nologo /O2 /std:c++17 /EHsc /utf-8 /DNOMINMAX /DWIN32_LEAN_AND_MEAN /I PythonCandidate\src PythonCandidate\src\NetworkTransport.cpp PythonCandidate\tests\NetworkTransportTests.cpp /link /OUT:network-after.exe ws2_32.lib > network-after-build.log 2>&1
if errorlevel 1 exit /b 1
network-after.exe > network-after.log 2>&1
echo %errorlevel% > network-after.exit
"@ | Set-Content "$r\network-after.cmd" -Encoding ASCII
& cmd /c "$r\network-after.cmd"
Get-Content "$r\network-after.log" | Select-String 'FAIL|SUMMARY'
