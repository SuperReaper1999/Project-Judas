$ErrorActionPreference='Stop';$r='P:\JudasValidation\CleanDistribution'
$p=Get-Process judas,judas_editor -ErrorAction SilentlyContinue
if($p){throw 'An application remains running; do not shut down'}
Unregister-ScheduledTask -TaskName JudasCleanDistributionQualification -Confirm:$false
@{qualificationComplete=$true;taskRemoved=$true;judasProcesses=0;evidenceRetained=$r}|ConvertTo-Json
