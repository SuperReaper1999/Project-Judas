$ErrorActionPreference='Stop';$r='P:\JudasValidation\CleanDistribution'
@{result=$(if(Test-Path "$r\BUILD_RESULT.json"){Get-Content "$r\BUILD_RESULT.json" -Raw});tail=$(if(Test-Path "$r\build.log"){Get-Content "$r\build.log" -Tail 12});free=(Get-PSDrive P).Free;processes=(Get-Process cl,MSBuild,cmake,git,powershell -ErrorAction SilentlyContinue|Select Id,ProcessName,CPU)}|ConvertTo-Json -Depth 6
