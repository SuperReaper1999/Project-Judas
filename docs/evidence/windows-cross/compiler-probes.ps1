$ErrorActionPreference='Stop';$ProgressPreference='SilentlyContinue';$root='P:\JudasValidation\WindowsCross'
Expand-Archive -LiteralPath "$root\compiler-probes.zip" -DestinationPath "$root\compiler-probes" -Force
$env:PATH='C:\Windows\System32;C:\Windows'
$results=@()
foreach($name in @('hello.exe','hello-clang.exe')){
 $p=Start-Process -FilePath "$root\compiler-probes\$name" -WorkingDirectory 'C:\Windows\Temp' -Wait -PassThru
 $results+=[pscustomobject]@{exe=$name;exit=$p.ExitCode;scope='C++ filesystem plus real worker thread in native Windows, no Wine';sha256=(Get-FileHash "$root\compiler-probes\$name").Hash.ToLowerInvariant()}
}
$results|ConvertTo-Json -Depth 4|Set-Content -Encoding UTF8 "$root\COMPILER_PROBES.json"
$results|ConvertTo-Json -Depth 4
