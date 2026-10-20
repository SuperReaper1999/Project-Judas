$ErrorActionPreference='Stop';$r='P:\JudasValidation\CleanDistribution'
if(Test-Path "$r\FINAL_PREPARED.json"){Get-Content "$r\FINAL_PREPARED.json" -Encoding UTF8 -Raw}
foreach($n in @('final-v2-configure.stdout.log','final-v2-configure.stderr.log','final-v2-stage.stdout.log','final-v2-stage.stderr.log')){if(Test-Path "$r\$n"){Write-Output $n;Get-Content "$r\$n" -Tail 4}}
