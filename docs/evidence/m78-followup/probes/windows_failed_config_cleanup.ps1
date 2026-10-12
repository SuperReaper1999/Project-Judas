$path='C:\JudasValidation\vcpkg\downloads\tools\git-2.7.4-windows.partial.2180';$files=Get-ChildItem -LiteralPath $path -Recurse -File;$bytes=($files|Measure-Object -Property Length -Sum).Sum
if($bytes -ne 200764552){throw 'Scratch changed since inventory; stop'}
Remove-Item -LiteralPath $path -Recurse -Force
$result=@{removed=$path;bytes=$bytes;free=(Get-PSDrive C).Free}|ConvertTo-Json -Depth 3 -Compress
Invoke-WebRequest -UseBasicParsing http://192.168.122.1:8872/m78-config-scratch-cleanup.json -Method Post -Body $result -ContentType application/json|Out-Null
