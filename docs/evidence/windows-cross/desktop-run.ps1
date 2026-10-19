$ErrorActionPreference='Stop';$ProgressPreference='SilentlyContinue'
$root='P:\JudasValidation\WindowsCross';$results="$root\Results-Moved";$bin="$root\Qualification";$project="$root\Source\projects\hearthworld\hearthworld.judasproj"
$env:PATH='C:\Windows\System32;C:\Windows';$env:TEMP="$root\Temp";$env:TMP=$env:TEMP
$env:GALLIUM_DRIVER='llvmpipe';$env:JUDAS_ENGINE_ROOT="$root\Source"
New-Item -ItemType Directory -Force $env:TEMP,$results|Out-Null
Add-Type -TypeDefinition 'using System.Runtime.InteropServices; public static class QuietErrors { [DllImport("kernel32.dll")] public static extern uint SetErrorMode(uint value); }'
[void][QuietErrors]::SetErrorMode(0x8003)
Set-Location "$root\UnrelatedCwd"
$rows=@()
function Run([string]$name,[string]$exe,[string]$arguments,[int]$timeout=240){
 [IO.File]::WriteAllText("$results\PROGRESS.txt",$name)
 $watch=[Diagnostics.Stopwatch]::StartNew()
 $info=New-Object Diagnostics.ProcessStartInfo
 $info.FileName=$exe;$info.Arguments=$arguments;$info.WorkingDirectory=(Get-Location).Path
 $info.UseShellExecute=$false;$info.CreateNoWindow=$true;$info.RedirectStandardOutput=$true;$info.RedirectStandardError=$true
 $p=New-Object Diagnostics.Process;$p.StartInfo=$info;[void]$p.Start()
 $stdout=$p.StandardOutput.ReadToEndAsync();$stderr=$p.StandardError.ReadToEndAsync()
 if(!$p.WaitForExit($timeout*1000)){$p.Kill();$p.WaitForExit();$code=-1000}else{$code=$p.ExitCode}
 [IO.File]::WriteAllText("$results\$name.stdout.txt",$stdout.Result)
 [IO.File]::WriteAllText("$results\$name.stderr.txt",$stderr.Result)
 $script:rows+=@{name=$name;exit=$code;seconds=$watch.Elapsed.TotalSeconds}
 $script:rows|ConvertTo-Json -Depth 5|Set-Content -Encoding UTF8 "$results\RUNS.json"
}
try {
 Remove-Item Env:JUDAS_ENGINE_ROOT
 $env:JUDAS_TEST_SCRIPT="$root\standalone.steps"
 Run 'moved-standalone' "$root\Moved Package résumé\judas.exe" '' 420
 Remove-Item Env:JUDAS_TEST_SCRIPT
 [IO.File]::WriteAllText("$results\COMPLETE.txt",'Completed; inspect individual exit code')
} catch {$_|Out-String|Set-Content "$results\ERROR.txt"}
