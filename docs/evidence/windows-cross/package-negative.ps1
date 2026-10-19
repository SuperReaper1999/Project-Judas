$ErrorActionPreference='Stop';$ProgressPreference='SilentlyContinue';$root='P:\JudasValidation\WindowsCross';$r="$root\Results";$env:PATH='C:\Windows\System32;C:\Windows'
Add-Type -TypeDefinition 'using System.Runtime.InteropServices;public static class QuietErrors2{[DllImport("kernel32.dll")]public static extern uint SetErrorMode(uint value);}'
[void][QuietErrors2]::SetErrorMode(0x8003)
function Probe($name,$exe,$arguments="--build-info-json"){
 $i=New-Object Diagnostics.ProcessStartInfo;$i.FileName=$exe;$i.WorkingDirectory="$root\UnrelatedCwd";$i.Arguments=$arguments;$i.UseShellExecute=$false;$i.CreateNoWindow=$true;$i.RedirectStandardOutput=$true;$i.RedirectStandardError=$true
 $p=New-Object Diagnostics.Process;$p.StartInfo=$i;[void]$p.Start();$o=$p.StandardOutput.ReadToEndAsync();$e=$p.StandardError.ReadToEndAsync();if(!$p.WaitForExit(30000)){$p.Kill();$p.WaitForExit();$code=-1000}else{$code=$p.ExitCode}
 [IO.File]::WriteAllText("$r\$name.stdout.txt",$o.Result);[IO.File]::WriteAllText("$r\$name.stderr.txt",$e.Result);return @{name=$name;exit=$code}
}
$good=Probe 'pristine-native-identity' "$root\Pristine Package\judas.exe"
$graphics=Probe 'stock-QXL-graphics' "$root\Pristine Package\judas.exe" ''
$broken="$root\Missing DLL Negative";New-Item -ItemType Directory $broken|Out-Null;Copy-Item "$root\Pristine Package\judas.exe" $broken
$bad=Probe 'missing-runtime-DLL' "$broken\judas.exe"
@{positive=$good;negative=$bad;stockQXL=$graphics;pass=($good.exit -eq 0 -and $bad.exit -ne 0 -and $bad.exit -ne -1000)}|ConvertTo-Json -Depth 4|Set-Content -Encoding UTF8 "$r\NEGATIVE.json"
