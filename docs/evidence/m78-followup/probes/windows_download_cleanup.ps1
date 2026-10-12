$files=@(@{path='C:\JudasValidation\vcpkg\downloads\PortableGit-2.47.1.2-64-bit.7z';hash='bbbc9516c62350401980af4ae220b0a35a452ee37cf355aa22fe45ecbf4feffc'},@{path='C:\JudasValidation\vcpkg\downloads\PortableGit-2.47.1.2-64-bit.7z.exe';hash='d73f0c1a42afbabe43862bd5abf5a646798125bc33cc02b7da7bbaeddae948f0'})
foreach($row in $files){if((Get-FileHash -LiteralPath $row.path).Hash.ToLower() -ne $row.hash){throw 'Download changed; stop'}}
foreach($row in $files){Remove-Item -LiteralPath $row.path -Force}
$result=@{removed=$files;free=(Get-PSDrive C).Free}|ConvertTo-Json -Depth 3 -Compress
Invoke-WebRequest -UseBasicParsing http://192.168.122.1:8872/m78-download-cleanup.json -Method Post -Body $result -ContentType application/json|Out-Null
