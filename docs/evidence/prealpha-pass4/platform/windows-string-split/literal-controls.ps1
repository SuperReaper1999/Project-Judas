$ErrorActionPreference='Stop';$root='C:\JudasValidation';$task='P:\JudasValidation\PreAlphaPass4'
$env:TEMP="$task\Temp";$env:TMP=$env:TEMP
Import-Module "$root\VS2022\Common7\Tools\Microsoft.VisualStudio.DevShell.dll"
Enter-VsDevShell -VsInstallPath "$root\VS2022" -SkipAutomaticLocation -DevCmdArguments '-arch=x64 -host_arch=x64'
Set-Location $task
$results=New-Object 'System.Collections.Generic.List[object]'
foreach($name in @('original','split')){
 $ErrorActionPreference='Continue'
 & cl.exe /nologo /std:c++17 /EHsc /MD "/Fe:$task\literal-$name.exe" "/Fo:$task\literal-$name.obj" "$task\literal-$name.cpp" 2>&1|Out-File "$task\literal-$name.log" -Encoding UTF8
 $code=$LASTEXITCODE;$ErrorActionPreference='Stop';$r=[ordered]@{name=$name;compileExit=$code;compilerOutput=[IO.File]::ReadAllText("$task\literal-$name.log")}
 if($code-eq 0){& "$task\literal-$name.exe" "$task\literal-$name.js";$r.runExit=$LASTEXITCODE;$r.outputBytes=(Get-Item "$task\literal-$name.js").Length;$r.outputSHA256=(Get-FileHash "$task\literal-$name.js").Hash.ToLowerInvariant()}
 $results.Add([pscustomobject]$r)
}
$results.ToArray()|ConvertTo-Json -Depth 4|Set-Content "$task\LITERAL_CONTROLS.json" -Encoding UTF8
