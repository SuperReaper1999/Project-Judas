# Final source parser/concurrency hardening: narrow CPU/tool rebuild only.
$ErrorActionPreference='Stop';$ProgressPreference='SilentlyContinue';$root='C:\JudasValidation';$base='http://192.168.122.1:8872';$receipts="$root\Receipts\M75"
function Upload($p){Invoke-WebRequest -UseBasicParsing "$base/$([IO.Path]::GetFileName($p))" -Method Post -InFile $p|Out-Null}
try{
 Invoke-WebRequest -UseBasicParsing "$base/m75-parser.json" -OutFile "$root\m75-parser.json";$expected=Get-Content "$root\m75-parser.json" -Raw|ConvertFrom-Json;$proof=@()
 foreach($entry in $expected.files){$dest=Join-Path "$root\Source" $entry.path;Invoke-WebRequest -UseBasicParsing "$base/$($entry.transfer)" -OutFile $dest;$hash=(Get-FileHash $dest).Hash.ToLower();if($hash -ne $entry.sha256){throw "Final source mismatch $($entry.path)"};$proof+=@{path=$entry.path;sha256=$hash;matches=$true}}
 Import-Module "$root\VS2022\Common7\Tools\Microsoft.VisualStudio.DevShell.dll";Enter-VsDevShell -VsInstallPath "$root\VS2022" -SkipAutomaticLocation -DevCmdArguments '-arch=x64 -host_arch=x64';$env:PATH="$root\Git\cmd;$root\VS2022\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin;$env:PATH";$ErrorActionPreference='Continue'
 & cmake --build "$root\Build" --config Release --target judas_windows_sdk judas_terrain_authoring_tests judas_terrain_pipeline_tests --parallel 2 2>&1|Tee-Object "$receipts\m75-windows-parser-build.log";$code=$LASTEXITCODE;$ErrorActionPreference='Stop';Upload "$receipts\m75-windows-parser-build.log";if($code){throw "Parser build $code"}
 $run="C:\M75 $([char]0xe9) $([char]0x3b1)";$start=Get-Date
 & "$root\Build\Release\judas_terrain_authoring_tests.exe" "$run\Final Unit" >"$receipts\m75-windows-final-authoring.log" 2>&1;$unit=$LASTEXITCODE;Upload "$receipts\m75-windows-final-authoring.log"
 & "$root\Build\Release\judas_terrain_pipeline_tests.exe" "$run\Final Pipeline" >"$receipts\m75-windows-final-pipeline.log" 2>&1;$pipeline=$LASTEXITCODE;Upload "$receipts\m75-windows-final-pipeline.log"
 $cli="$root\Build\windows-sdk\judas_terrain_author.exe"
 $bad="$run\invalid-overflow.judasterrain";$ErrorActionPreference='Continue'
 & $cli create $bad 4294967361 65 64 64 >"$receipts\m75-windows-cli-overflow.log" 2>&1;$negative=$LASTEXITCODE;$ErrorActionPreference='Stop';Upload "$receipts\m75-windows-cli-overflow.log"
 if(!$negative -or (Test-Path $bad)){throw 'CLI overflow accepted'}
 $exes=@();foreach($n in @('judas_editor.exe','judas_terrain_author.exe','judas_terrain_authoring_tests.exe')){$p="$root\Build\Release\$n";$exes+=@{name=$n;sha256=(Get-FileHash $p).Hash.ToLower();lastWriteUtc=(Get-Item $p).LastWriteTimeUtc.ToString('o')}}
 @{build=$code;authoring=$unit;pipeline=$pipeline;source=$proof;executables=$exes;note='Final authoring-only integer bounds, single-read conflict baseline and configured-asset-root support; runtime/geometry/brush/GPU semantics unchanged.'}|ConvertTo-Json -Depth 12|Set-Content "$receipts\m75-windows-parser-summary.json" -Encoding UTF8;Upload "$receipts\m75-windows-parser-summary.json"
 if($unit -or $pipeline){throw 'Final native CPU check failed'};exit 0
}catch{$_|Out-String|Set-Content "$receipts\m75-windows-parser-failure.log";Upload "$receipts\m75-windows-parser-failure.log";exit 1}
