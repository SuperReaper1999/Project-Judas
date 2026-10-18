$ErrorActionPreference='Stop';$ProgressPreference='SilentlyContinue'
$task='P:\JudasValidation\PreAlphaPass4';$sdk="$task\Build\windows-sdk";$work="$task\GuiWork";$results="$work\ColdFollowupResults";$testRoot="$work\NativeTests"
New-Item -ItemType Directory -Force $results|Out-Null
$env:PATH='C:\Windows\System32;C:\Windows';$env:TMP="$work\Temp";$env:TEMP=$env:TMP;$env:GALLIUM_DRIVER='llvmpipe';$env:MESA_SHADER_CACHE_DIR="$work\ShaderCache";$env:JUDAS_RESOURCE_MODE='blocking'
Copy-Item "$sdk\assets" "$testRoot\assets" -Recurse
if(!(Test-Path "$testRoot\assets\fonts\DejaVuSans.ttf")){throw 'Required unchanged engine fonts missing'}
$rows=New-Object 'System.Collections.Generic.List[object]';$fixtureCwd="$task\Projects";$out="$work\ColdCases-fontfollowup"
function Run-Fixture($label,$exe,$fixtureArguments,$timeout=300){
 $stdout="$results\$label.stdout.log";$stderr="$results\$label.stderr.log";$start=[DateTime]::UtcNow
 $opts=@{FilePath=$exe;WorkingDirectory=$fixtureCwd;PassThru=$true;RedirectStandardOutput=$stdout;RedirectStandardError=$stderr}
 if($fixtureArguments.Count){$opts.ArgumentList=@($fixtureArguments|ForEach-Object {'"'+[string]$_+'"'})}
 $p=Start-Process @opts;[void]$p.Handle;$done=$p.WaitForExit($timeout*1000)
 if(!$done){Stop-Process -Id $p.Id -Force;$code='timeout'}else{$p.Refresh();$code=$p.ExitCode}
 $r=[pscustomobject]@{label=$label;executable=$exe;arguments=@($fixtureArguments);cwd=$fixtureCwd;exit=$code;seconds=([DateTime]::UtcNow-$start).TotalSeconds;stdout=[IO.File]::ReadAllText($stdout);stderr=[IO.File]::ReadAllText($stderr);desktopIdentity=[Security.Principal.WindowsIdentity]::GetCurrent().Name;sessionId=[Diagnostics.Process]::GetCurrentProcess().SessionId}
 $rows.Add($r);$r|ConvertTo-Json -Depth 5|Set-Content -Encoding UTF8 "$results\$label.json"
}
Run-Fixture 'm70-actual-physics' "$testRoot\judas_character_lab_tests.exe" @("$out\m70-app") 300
Run-Fixture 'm70-cold-write' "$testRoot\judas_character_lab_tests.exe" @("$out\m70-cold",'cold-write') 300
Run-Fixture 'm70-cold-read' "$testRoot\judas_character_lab_tests.exe" @("$out\m70-cold",'cold-read') 300
foreach($case in @(@('step','cold-write','cold-read'),@('stand','cold-stand-write','cold-stand-read'),@('recovery','cold-recovery-write','cold-recovery-read'))){
 Run-Fixture "m77-$($case[0])-write" "$testRoot\judas_balance_lab_tests.exe" @("$out\m77-$($case[0])",$case[1]) 300
 Run-Fixture "m77-$($case[0])-read" "$testRoot\judas_balance_lab_tests.exe" @("$out\m77-$($case[0])",$case[2]) 300
}

$manifest=Get-Content -Raw "$sdk\BUILD_INFO.json"|ConvertFrom-Json
[pscustomobject]@{schema=1;utc=[DateTime]::UtcNow.ToString('o');engineIdentity=$manifest;rows=$rows.ToArray();allPassed=(@($rows|Where-Object {$_.exit-ne 0}).Count-eq 0);fontSHA256=(Get-FileHash "$testRoot\assets\fonts\DejaVuSans.ttf").Hash.ToLowerInvariant();setupCorrection='Only unchanged SDK engine-font assets supplied beside isolated test executables; original tests, inputs, controller settings, timestep, thresholds and source preserved';originalSetupFailuresPreserved=$true;noWindowsPerformanceGate=$true}|ConvertTo-Json -Depth 7|Set-Content -Encoding UTF8 "$work\COLD_FOLLOWUP_RESULTS.json"
