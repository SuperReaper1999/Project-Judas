$r='P:\JudasValidation\WindowsSeven'
$results=@()
for($i=1;$i-le 5;$i++) {
 & "$r\network-after.exe" *> "$r\network-repeat-$i.log"
 $results+=@{run=$i;exit=$LASTEXITCODE}
}
$results | ConvertTo-Json | Set-Content "$r\network-repeats.json"
Get-Content "$r\network-repeats.json"
