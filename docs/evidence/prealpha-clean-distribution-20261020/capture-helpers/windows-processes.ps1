$ErrorActionPreference='Continue';$r='P:\JudasValidation\CleanDistribution'
Get-Process cl,link,MSBuild,cmake,ninja,git,powershell -ErrorAction SilentlyContinue|Select Id,ProcessName,CPU,WorkingSet64,StartTime|ConvertTo-Json
Get-Item "$r\Build\CMakeFiles\CMakeConfigureLog.yaml","$r\documented-build.stdout.log","$r\documented-build.stderr.log" -ErrorAction SilentlyContinue|Select FullName,Length,LastWriteTimeUtc|ConvertTo-Json
if(Test-Path "$r\BUILD_RESULT.json"){Get-Content "$r\BUILD_RESULT.json" -Encoding UTF8 -Raw}
