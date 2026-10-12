$rows=@();Get-ChildItem -LiteralPath C:\JudasValidation\vcpkg\downloads -File|Sort-Object LastWriteTime -Descending|Select-Object -First 10|ForEach-Object {$rows+=@{path=[string]::Join('',[char[]]$_.FullName);bytes=$_.Length;written=$_.LastWriteTimeUtc.ToString('o');sha256=(Get-FileHash -LiteralPath $_.FullName).Hash.ToLower()}}
$result=@{free=(Get-PSDrive C).Free;downloads=$rows}|ConvertTo-Json -Depth 3 -Compress
Invoke-WebRequest -UseBasicParsing http://192.168.122.1:8872/m78-download-inventory.json -Method Post -Body $result -ContentType application/json|Out-Null
