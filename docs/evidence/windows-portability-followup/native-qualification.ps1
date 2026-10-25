$ErrorActionPreference='Stop';$ProgressPreference='SilentlyContinue'
$r='P:\JudasValidation\WindowsSeven'
$cm='C:\JudasValidation\VS2022\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
$env:PATH='C:\JudasValidation\Python;C:\Windows;'+$env:PATH
try {
 'waiting-for-baseline-build' | Set-Content "$r\phase.txt"
 $deadline=(Get-Date).AddMinutes(40)
 while (!(Test-Path "$r\build-before.exit")) { if((Get-Date)-gt $deadline){throw 'baseline build exceeded bounded wait'};Start-Sleep 5 }
 if([int](Get-Content "$r\build-before.exit") -ne 0){throw 'baseline build failed; inspect original log'}
 Add-Type 'using System.Runtime.InteropServices; public class JudasErrorMode { [DllImport("kernel32.dll")] public static extern uint SetErrorMode(uint mode); }'
 [JudasErrorMode]::SetErrorMode(3) | Out-Null
 New-Item -ItemType Directory -Force "$r\Build\Release" | Out-Null
 Copy-Item "$r\Build\unicode\windows-icu\bin64\*.dll" "$r\Build\Release" -Force
 function Test-One($name,$phase,$arguments=@()) {
  $exe="$r\Build\Release\judas_$name`_tests.exe"
  $watch=[Diagnostics.Stopwatch]::StartNew()
  $args=@{FilePath=$exe;WorkingDirectory="$r\Source";PassThru=$true;NoNewWindow=$true;RedirectStandardOutput="$r\$phase-$name.log";RedirectStandardError="$r\$phase-$name.stderr"}
  if($arguments.Count){$args.ArgumentList=$arguments}
  $p=Start-Process @args
  $handle=$p.Handle # retain the native handle through process exit on PowerShell 5
  if(!$p.WaitForExit(120000)){ $p.Kill();throw "$name exceeded 120s" }
  $p.Refresh()
  $exit=$p.ExitCode
  # Start-Process may not populate ExitCode after a separate WaitForExit on PS5;
  # the native process handle remains authoritative via GetExitCodeProcess.
  if($null -eq $exit){throw "No exit status for $name"}
  $result=[ordered]@{name=$name;exit=$exit;seconds=$watch.Elapsed.TotalSeconds;sha256=(Get-FileHash $exe -Algorithm SHA256).Hash}
  $result | ConvertTo-Json | Set-Content "$r\$phase-$name.result.json"
 }
 'baseline-tests' | Set-Content "$r\phase.txt"
 foreach($t in @('liquid_rotation','liquid_surface','editor_preferences','mk2_rotation')) { Test-One $t 'before' }
 Get-ChildItem "$r\Source\src\LiquidGeometry.cpp","$r\Source\src\physics\WorldCCDSearch.cpp" | Get-FileHash | ConvertTo-Json | Set-Content "$r\source-before.json"
 Invoke-WebRequest http://192.168.122.1:8765/candidate.zip -OutFile "$r\candidate-final.zip"
 Expand-Archive "$r\candidate-final.zip" "$r\Source" -Force
 'candidate-build' | Set-Content "$r\phase.txt"
 $ErrorActionPreference='Continue'
 & $cm --build "$r\Build" --config Release --target judas_mk2_rotation_tests judas_liquid_rotation_tests judas_liquid_surface_tests judas_editor_preferences_tests judas_mk2_rotation_cold_tests judas_mk2_physics_tests judas_impact_mesh_cost_tests --parallel 2 *> "$r\build-after.log"
 $code=$LASTEXITCODE;$code | Set-Content "$r\build-after.exit"
 $ErrorActionPreference='Stop'
 if($code-ne 0){throw 'candidate build failed'}
 Copy-Item "$r\Build\unicode\windows-icu\bin64\*.dll" "$r\Build\Release" -Force
 'candidate-tests' | Set-Content "$r\phase.txt"
 foreach($t in @('liquid_rotation','liquid_surface','editor_preferences','mk2_rotation','mk2_physics','impact_mesh_cost')) { Test-One $t 'after' }
 Test-One 'mk2_rotation_cold' 'after' @('P:\JudasValidation\WindowsSeven\cold-final')
 Get-ChildItem "$r\Source\src\LiquidGeometry.cpp","$r\Source\src\physics\WorldCCDSearch.cpp" | Get-FileHash | ConvertTo-Json | Set-Content "$r\source-after.json"
 'complete' | Set-Content "$r\phase.txt"
} catch { $_ | Out-String | Set-Content "$r\qualification-error.txt"; 'blocked' | Set-Content "$r\phase.txt";throw }
