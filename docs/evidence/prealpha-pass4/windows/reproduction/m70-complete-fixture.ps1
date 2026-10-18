$ErrorActionPreference='Stop';$ProgressPreference='SilentlyContinue'
$task='P:\JudasValidation\PreAlphaPass4';$sdk="$task\Build\windows-sdk";$work="$task\GuiWork";$results="$work\M70CompleteFixtureResults";$testRoot="$work\NativeTests"
New-Item -ItemType Directory -Force $results|Out-Null
$env:PATH='C:\Windows\System32;C:\Windows';$env:TMP="$work\Temp";$env:TEMP=$env:TMP;$env:GALLIUM_DRIVER='llvmpipe';$env:MESA_SHADER_CACHE_DIR="$work\ShaderCache";$env:JUDAS_RESOURCE_MODE='blocking'
$rows=New-Object 'System.Collections.Generic.List[object]';$fixtureCwd="$task\Projects";$out="$work\CompleteFixtures"
function Run-Fixture($label,$exe,$fixtureArguments,$timeout=300){
 $stdout="$results\$label.stdout.log";$stderr="$results\$label.stderr.log";$start=[DateTime]::UtcNow
 $opts=@{FilePath=$exe;WorkingDirectory=$fixtureCwd;PassThru=$true;RedirectStandardOutput=$stdout;RedirectStandardError=$stderr}
 if($fixtureArguments.Count){$opts.ArgumentList=@($fixtureArguments|ForEach-Object {'"'+[string]$_+'"'})}
 $p=Start-Process @opts;[void]$p.Handle;$done=$p.WaitForExit($timeout*1000)
 if(!$done){Stop-Process -Id $p.Id -Force;$code='timeout'}else{$p.Refresh();$code=$p.ExitCode}
 $r=[pscustomobject]@{label=$label;executable=$exe;arguments=@($fixtureArguments);cwd=$fixtureCwd;exit=$code;seconds=([DateTime]::UtcNow-$start).TotalSeconds;stdout=[IO.File]::ReadAllText($stdout);stderr=[IO.File]::ReadAllText($stderr);desktopIdentity=[Security.Principal.WindowsIdentity]::GetCurrent().Name;sessionId=[Diagnostics.Process]::GetCurrentProcess().SessionId}
 $rows.Add($r);$r|ConvertTo-Json -Depth 5|Set-Content -Encoding UTF8 "$results\$label.json"
}

Run-Fixture 'm70-actual-physics-complete-fixtures' "$testRoot\judas_character_lab_tests.exe" @("$out\m70-app") 300
[pscustomobject]@{schema=1;utc=[DateTime]::UtcNow.ToString('o');rows=$rows.ToArray();allPassed=(@($rows|Where-Object {$_.exit-ne 0}).Count-eq 0);setupCorrection='Original engine fonts and two exact repository cookbook files supplied at their original relative locations; no test/controller/physics change';originalSetupFailurePreserved=$true}|ConvertTo-Json -Depth 6|Set-Content -Encoding UTF8 "$work\M70_COMPLETE_FIXTURE_RESULTS.json"
