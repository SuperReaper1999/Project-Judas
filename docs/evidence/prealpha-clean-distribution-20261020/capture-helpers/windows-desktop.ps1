$ErrorActionPreference='Stop';$ProgressPreference='SilentlyContinue'
$r='P:\JudasValidation\CleanDistribution';$results="$r\Results";$sdk="$r\Moved SDK r$([char]0xe9)sum$([char]0xe9)";$python='C:\JudasValidation\Python\python.exe'
$env:PATH='C:\Windows\System32;C:\Windows';$env:TEMP="$r\Temp";$env:TMP=$env:TEMP;$env:GALLIUM_DRIVER='llvmpipe'
Get-ChildItem Env:JUDAS_*|ForEach-Object {Remove-Item "Env:$($_.Name)"}
Set-Location "$r\Unrelated CWD"
Add-Type -TypeDefinition 'using System.Runtime.InteropServices; public static class QuietErrors { [DllImport("kernel32.dll")] public static extern uint SetErrorMode(uint value); }'
[void][QuietErrors]::SetErrorMode(0x8003)
$rows=@()
function Run($name,$exe,$arguments,$expected=0,$timeout=240){
 [IO.File]::WriteAllText("$results\PROGRESS.txt",$name)
 $s=[Diagnostics.Stopwatch]::StartNew();$i=New-Object Diagnostics.ProcessStartInfo;$i.FileName=$exe;$i.Arguments=$arguments;$i.WorkingDirectory=(Get-Location).Path;$i.UseShellExecute=$false;$i.CreateNoWindow=$true;$i.RedirectStandardOutput=$true;$i.RedirectStandardError=$true
 $p=New-Object Diagnostics.Process;$p.StartInfo=$i;[void]$p.Start();$o=$p.StandardOutput.ReadToEndAsync();$e=$p.StandardError.ReadToEndAsync()
 if(!$p.WaitForExit($timeout*1000)){$p.Kill();$p.WaitForExit();$code=-1000}else{$code=$p.ExitCode}
 [IO.File]::WriteAllText("$results\$name.stdout.txt",$o.Result);[IO.File]::WriteAllText("$results\$name.stderr.txt",$e.Result)
 $script:rows+=@{name=$name;exe=$exe;arguments=$arguments;exit=$code;expectedExit=$expected;seconds=$s.Elapsed.TotalSeconds};$script:rows|ConvertTo-Json -Depth 5|Set-Content -Encoding UTF8 "$results\RUNS.json"
 if($code -ne $expected){throw "$name returned $code expected $expected"}
}
try {
 Run 'sdk-verify' $python "`"$sdk\tools\package_sdk.py`" verify `"$sdk`""
 Run 'sdk-imports' $python "`"$sdk\tools\inspect_sdk.py`" `"$sdk`" --system-directory C:\Windows\System32 --output `"$results\SDK_HOST.json`""
 Run 'native-storage' "$sdk\judas_windows_readiness_tests.exe" ''
 $project="$r\Projects\My First Game\My_First_Game.judasproj"
 Run 'script-check' "$sdk\judas_script_check.exe" "`"$project`""
 Run 'sdk-export' "$sdk\judas_export.exe" "`"$project`" `"$r\Game Package`" `"$sdk\judas.exe`""
 # The pristine distribution remains unchanged. Mesa is only a separate VM graphics harness.
 $vm="$r\Software GL SDK";Copy-Item $sdk $vm -Recurse
 Copy-Item 'C:\JudasValidation\Mesa\x64\opengl32.dll','C:\JudasValidation\Mesa\x64\libgallium_wgl.dll' $vm
 $game="$r\Relocated Game r$([char]0xe9)sum$([char]0xe9)";Move-Item "$r\Game Package" $game
 Copy-Item 'C:\JudasValidation\Mesa\x64\opengl32.dll','C:\JudasValidation\Mesa\x64\libgallium_wgl.dll' $game
 [IO.File]::WriteAllText("$r\game.steps","FRAMES 120`nFRAME_SECONDS 0.016666667`nLOG_EVERY 0`nEXPECT_SCENE Scenes/main.judas 100`nSCREENSHOT 110 $r/Results/standalone.png`n")
 $env:JUDAS_ENGINE_ROOT='Z:\not-a-judas-source';$env:JUDAS_TEST_SCRIPT="$r\game.steps"
 Run 'moved-standalone' "$game\judas.exe" '' 0 180
 Remove-Item Env:JUDAS_ENGINE_ROOT;Remove-Item Env:JUDAS_TEST_SCRIPT
 $env:JUDAS_EDITOR_STABILIZATION="$r\Editor New Project"
 Run 'sdk-editor-new-project' "$vm\judas_editor.exe" '' 0 240
 Remove-Item Env:JUDAS_EDITOR_STABILIZATION
 $env:JUDAS_EDITOR_AUTOTEST="$r\Visible Editor"
 Run 'sdk-editor-play-stop' "$vm\judas_editor.exe" "`"$project`"" 0 180
 Remove-Item Env:JUDAS_EDITOR_AUTOTEST
 Run 'sdk-final-verify' $python "`"$sdk\tools\package_sdk.py`" verify `"$sdk`""
 @{passed=$true;nativeWindows=$true;softwareGraphics='Mesa llvmpipe injected only in separate harness copies';hardwareAcceptance=$false;sdk=$sdk;cwd=(Get-Location).Path}|ConvertTo-Json|Set-Content -Encoding UTF8 "$results\COMPLETE.json"
} catch {$_|Out-String|Set-Content -Encoding UTF8 "$results\ERROR.txt";exit 1}
