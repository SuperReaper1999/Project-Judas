$ErrorActionPreference='Stop';$ProgressPreference='SilentlyContinue';$root='C:\JudasValidation';$base='http://192.168.122.1:8872';$receipts="$root\Receipts\M75"
Invoke-WebRequest -UseBasicParsing "$base/m75-final-union.json" -OutFile "$root\m75-final-union.json";$expected=Get-Content "$root\m75-final-union.json" -Raw|ConvertFrom-Json
$proof=@();foreach($e in $expected.files.PSObject.Properties){$p=Join-Path "$root\Source" $e.Name;$hash=(Get-FileHash $p).Hash.ToLower();$proof+=@{path=$e.Name;expected=$e.Value;actual=$hash;matches=($hash -eq $e.Value)}}
$record=@{startingHead=$expected.startingHead;files=$proof;count=$proof.Count;allMatch=(@($proof|Where-Object {!$_.matches}).Count -eq 0);freeBytes=(Get-PSDrive C).Free;time=(Get-Date).ToString('o')}
$record|ConvertTo-Json -Depth 12|Set-Content "$receipts\m75-windows-final-input-proof.json" -Encoding UTF8
Invoke-WebRequest -UseBasicParsing "$base/m75-windows-final-input-proof.json" -Method Post -InFile "$receipts\m75-windows-final-input-proof.json"|Out-Null
if(!$record.allMatch){exit 1};exit 0
