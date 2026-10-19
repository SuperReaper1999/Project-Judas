$ErrorActionPreference='Stop';$ProgressPreference='SilentlyContinue'
$root='P:\JudasValidation\WindowsCross';$results="$root\Results";$bin="$root\Qualification";$project="$root\Source\projects\hearthworld\hearthworld.judasproj"
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
 Run 'runtime-identity' "$bin\judas.exe" '--build-info-json'
 Run 'identity' "$bin\judas_build_identity_tests.exe" ''
 Run 'readiness' "$bin\judas_windows_readiness_tests.exe" ''
 Run 'motor' "$bin\judas_character_motor_tests.exe" ''
 Run 'audio' "$bin\judas_windows_cross_audio_tests.exe" ''
 # Text fixtures intentionally need the supplied source tree, not a game package.
 Set-Location "$root\Source"
 Run 'unicode' "$bin\judas_text_localization_tests.exe" "`"$results\Text`""
 Set-Location "$root\UnrelatedCwd"
 $save=Join-Path $env:LOCALAPPDATA 'judas\games\30026fff6abc9ef36c4e0b59452b48164eeb42a268c8ddfcafe7f27fbbc53db4\Saves'
 if(Test-Path "$save\hearthworld-review.save"){throw 'Refusing to replace an existing Hearthworld review save'}
 Run 'game-write' "$bin\judas_hearthworld_application_tests.exe" "`"$project`" write `"$results\Gameplay`"" 420
 Run 'game-cold-read' "$bin\judas_hearthworld_application_tests.exe" "`"$project`" read `"$results\Gameplay`"" 420
 Run 'game-controls' "$bin\judas_hearthworld_application_tests.exe" "`"$project`" controls `"$results\Controls`"" 420
 Remove-Item Env:JUDAS_ENGINE_ROOT
 $env:JUDAS_TEST_SCRIPT="$root\standalone.steps"
 Run 'moved-standalone' "$root\Moved Package résumé\judas.exe" '' 420
 Remove-Item Env:JUDAS_TEST_SCRIPT
 [IO.File]::WriteAllText("$results\COMPLETE.txt",'Completed; inspect individual results, not only this marker')
} catch {
 $_|Out-String|Set-Content "$results\ERROR.txt"
 [IO.File]::WriteAllText("$results\COMPLETE.txt",'Runner error; inspect ERROR.txt')
}
