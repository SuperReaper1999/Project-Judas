$ErrorActionPreference='Stop';$base='http://192.168.122.1:8872';$root='C:\JudasValidation';$r="$root\Receipts\M75"
Copy-Item "$root\Source\docs\WINDOWS.md" "$r\m75-windows-preexisting-doc.md"
Invoke-WebRequest -UseBasicParsing "$base/m75-windows-preexisting-doc.md" -Method Post -InFile "$r\m75-windows-preexisting-doc.md"|Out-Null
Copy-Item "$r\m75-windows-final-input-proof.json" "$r\m75-windows-final-input-failure.json"
Invoke-WebRequest -UseBasicParsing "$base/m75-windows-final-input-failure.json" -Method Post -InFile "$r\m75-windows-final-input-failure.json"|Out-Null
Invoke-WebRequest -UseBasicParsing "$base/m75-current-windows.md" -OutFile "$root\Source\docs\WINDOWS.md"
