$ErrorActionPreference='Stop'
$r='P:\JudasValidation\WindowsSeven'
try {
 $deadline=(Get-Date).AddMinutes(20)
 while (!(Test-Path "$r\build-after-forced.exit")) { if((Get-Date)-gt $deadline){throw 'forced build wait exceeded'};Start-Sleep 3 }
 if([int](Get-Content "$r\build-after-forced.exit")-ne 0){throw 'forced build failed'}
 Add-Type 'using System.Runtime.InteropServices; public class JudasErrorMode { [DllImport("kernel32.dll")] public static extern uint SetErrorMode(uint mode); }'
 [JudasErrorMode]::SetErrorMode(3)|Out-Null
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
 'final-tests'|Set-Content "$r\phase.txt"
 foreach($t in @('liquid_rotation','liquid_surface','editor_preferences','mk2_rotation','mk2_physics','impact_mesh_cost')) { Test-One $t 'final' }
 Test-One 'mk2_rotation_cold' 'final' @('P:\JudasValidation\WindowsSeven\cold-final-forced')
 'final-complete'|Set-Content "$r\phase.txt"
} catch {$_|Out-String|Set-Content "$r\final-error.txt";'final-blocked'|Set-Content "$r\phase.txt";throw}
