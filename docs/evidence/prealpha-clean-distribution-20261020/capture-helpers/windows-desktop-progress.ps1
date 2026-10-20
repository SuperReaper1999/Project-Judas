$ErrorActionPreference='Stop';$r='P:\JudasValidation\CleanDistribution\Results'
foreach($n in @('PROGRESS.txt','RUNS.json','ERROR.txt','COMPLETE.json')){if(Test-Path "$r\$n"){Write-Output $n;Get-Content "$r\$n" -Encoding UTF8 -Raw}}
