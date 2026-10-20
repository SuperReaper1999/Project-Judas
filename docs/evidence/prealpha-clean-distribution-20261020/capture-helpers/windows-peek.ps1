$ErrorActionPreference='Stop';$r='P:\JudasValidation\CleanDistribution'
if(Test-Path "$r\BUILD_RESULT.json"){Get-Content "$r\BUILD_RESULT.json" -Encoding UTF8 -Raw}
if(Test-Path "$r\documented-build.stdout.log"){Get-Content "$r\documented-build.stdout.log" -Encoding UTF8 -Tail 15}
if(Test-Path "$r\documented-build.stderr.log"){Get-Content "$r\documented-build.stderr.log" -Encoding UTF8 -Tail 10}
