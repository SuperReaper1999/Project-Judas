$ErrorActionPreference='Stop';$ProgressPreference='SilentlyContinue'
$task='P:\JudasValidation\PreAlphaPass4';$sdk="$task\Build\windows-sdk";$work="$task\GuiWork";$results="$work\Results";$software="$work\SoftwareSDK";$project="$work\Projects\projects\physics_stress_lab\physics_stress_lab.judasproj"
New-Item -ItemType Directory -Force $results,"$work\UnrelatedCwd","$work\Temp"|Out-Null
$env:TMP="$work\Temp";$env:TEMP=$env:TMP;$env:PATH='C:\Windows\System32;C:\Windows';$env:JUDAS_RESOURCE_MODE='blocking'
$rows=New-Object 'System.Collections.Generic.List[object]'
function Run-Gui($label,$exe,$arguments,$timeout=240){
 $stdout="$results\$label.stdout.log";$stderr="$results\$label.stderr.log";$start=[DateTime]::UtcNow
 $options=@{FilePath=$exe;WorkingDirectory="$work\UnrelatedCwd";PassThru=$true;RedirectStandardOutput=$stdout;RedirectStandardError=$stderr}
 if($arguments.Count){$options.ArgumentList=@($arguments|ForEach-Object {'"'+[string]$_+'"'})}
 $p=Start-Process @options;[void]$p.Handle;$done=$p.WaitForExit($timeout*1000)
 if(!$done){Stop-Process -Id $p.Id -Force;$code='timeout'}else{$p.Refresh();$code=$p.ExitCode}
 $row=[pscustomobject]@{label=$label;executable=$exe;arguments=@($arguments);cwd="$work\UnrelatedCwd";exit=$code;seconds=([DateTime]::UtcNow-$start).TotalSeconds;stdout=[IO.File]::ReadAllText($stdout);stderr=[IO.File]::ReadAllText($stderr);desktopIdentity=[Security.Principal.WindowsIdentity]::GetCurrent().Name;sessionId=[Diagnostics.Process]::GetCurrentProcess().SessionId;path=$env:PATH}
 $rows.Add($row);$row|ConvertTo-Json -Depth 5|Set-Content -Encoding UTF8 "$results\$label.json"
}
if(!(Test-Path "$sdk\SDK_MANIFEST.json")){throw 'Final complete SDK required'}
$saveRoot=Join-Path $env:LOCALAPPDATA 'judas\games\317517ccb7462d4379370eff1453c07d42388a98a9916d4a981946d4188dec65'
if(Test-Path $saveRoot){throw 'Existing user save directory detected; do not overwrite progress'}
Copy-Item "$task\Projects" "$work\Projects" -Recurse
$scene=Join-Path (Split-Path $project) 'Scenes\room128.judas';$sceneBefore=(Get-FileHash $scene).Hash.ToLowerInvariant()
@'
FRAMES 5
LOG_EVERY 0
'@|Set-Content -Encoding ASCII "$work\startup.txt"
$env:JUDAS_TEST_SCRIPT="$work\startup.txt"
Run-Gui 'stock-qxl-startup' "$sdk\judas.exe" @($project) 45
Remove-Item Env:JUDAS_TEST_SCRIPT
Copy-Item $sdk $software -Recurse
foreach($name in @('opengl32.dll','libgallium_wgl.dll')){Copy-Item "C:\JudasValidation\Mesa\x64\$name" "$software\$name"}
$env:GALLIUM_DRIVER='llvmpipe';$env:MESA_SHADER_CACHE_DIR="$work\ShaderCache"
$prefs=Join-Path $env:APPDATA 'Judas\Editor';$prefsBackup="$results\OriginalEditorPreferences";New-Item -ItemType Directory -Force $prefsBackup|Out-Null
$prefsBefore=@{}
foreach($name in @('workspace.json','windows.ini')){$path=Join-Path $prefs $name;if(Test-Path -LiteralPath $path){$prefsBefore[$name]=(Get-FileHash -LiteralPath $path).Hash.ToLowerInvariant();Copy-Item -LiteralPath $path "$prefsBackup\$name"}}
$env:JUDAS_EDITOR_AUTOTEST="$results\editor";$env:JUDAS_EDITOR_AUTOTEST_AUTHORING='1'
Run-Gui 'editor-authoring-play-stop' "$software\judas_editor.exe" @($project) 300
Remove-Item Env:JUDAS_EDITOR_AUTOTEST,Env:JUDAS_EDITOR_AUTOTEST_AUTHORING
foreach($name in @('workspace.json','windows.ini')){
 $path=Join-Path $prefs $name
 if(Test-Path -LiteralPath $path){Copy-Item -LiteralPath $path "$results\tested-$name"}
 if($prefsBefore.ContainsKey($name)){Copy-Item "$prefsBackup\$name" $path -Force;if((Get-FileHash -LiteralPath $path).Hash.ToLowerInvariant()-ne $prefsBefore[$name]){throw 'Could not byte-restore original editor preferences'}}elseif(Test-Path -LiteralPath $path){Remove-Item -LiteralPath $path}
}
@'
FRAMES 270
LOG_EVERY 0
ACTION motors 1 5 6
ACTION kick_one 1 60 61
ACTION save 1 100 101
WAIT_SERVICES save 15000 101
ACTION load 1 125 126
WAIT_SERVICES save 15000 126
EXPECT_SCENE Scenes/room128.judas 135
ACTION rotated 1 170 171
WAIT_SERVICES scene 15000 171
EXPECT_SCENE Scenes/rotated.judas 180
ACTION pause 1 200 201
EXPECT_PAUSED 1 204
ACTION pause 1 210 211
EXPECT_PAUSED 0 214
ACTION reset 1 225 226
WAIT_SERVICES scene 15000 226
EXPECT_SCENE Scenes/rotated.judas 235
SCREENSHOT 250 P:/JudasValidation/PreAlphaPass4/GuiWork/Results/runtime.png
'@|Set-Content -Encoding ASCII "$work\runtime.txt"
$env:JUDAS_TEST_SCRIPT="$work\runtime.txt"
Run-Gui 'runtime-save-load-transition-pause-reload' "$software\judas.exe" @($project) 240
@'
FRAMES 3
LOG_EVERY 0
EXPECT_SCENE deliberately-absent-scene 1
'@|Set-Content -Encoding ASCII "$work\negative.txt"
$env:JUDAS_TEST_SCRIPT="$work\negative.txt"
Run-Gui 'runtime-oracle-negative' "$software\judas.exe" @($project) 90
Remove-Item Env:JUDAS_TEST_SCRIPT
$pristine="$task\Moved Game $([char]0xe9) $([char]0x3b1)";$package="$work\MovedSoftwareGame $([char]0xe9) $([char]0x3b1)"
Copy-Item $pristine $package -Recurse
foreach($name in @('opengl32.dll','libgallium_wgl.dll')){Copy-Item "C:\JudasValidation\Mesa\x64\$name" "$package\$name"}
$env:JUDAS_TEST_SCRIPT="$work\runtime.txt"
Run-Gui 'moved-standalone-full-harness' "$package\judas.exe" @() 240
Remove-Item Env:JUDAS_TEST_SCRIPT
$pristineFiles=Get-Content -Raw "$task\PRISTINE_PACKAGE_FILES.json"|ConvertFrom-Json
foreach($f in $pristineFiles){$path=Join-Path $pristine $f.path;if(!(Test-Path -LiteralPath $path) -or (Get-FileHash -LiteralPath $path).Hash.ToLowerInvariant()-ne $f.sha256){throw 'Pristine package changed: '+$f.path}}

$testRoot="$work\NativeTests";New-Item -ItemType Directory -Force $testRoot|Out-Null
foreach($dll in Get-ChildItem -LiteralPath $sdk -Filter '*.dll'){Copy-Item -LiteralPath $dll.FullName "$testRoot\$($dll.Name)"}
foreach($name in @('opengl32.dll','libgallium_wgl.dll')){Copy-Item "C:\JudasValidation\Mesa\x64\$name" "$testRoot\$name"}
$testNames=@('judas_impact_mesh_cost_tests','judas_impact_robustness_tests','judas_origin_admission_tests','judas_character_motor_tests','judas_world_persistence_tests','judas_character_lab_tests','judas_balance_lab_tests')
foreach($name in $testNames){Copy-Item "$task\Build\Release\$name.exe" "$testRoot\$name.exe"}
# These original fixtures locate their authored project by relative path.
# Preserve the ordinary game's unrelated cwd tests above; use authored fixture cwd here.
$fixtureCwd="$task\Projects";$out="$work\ColdCases"
function Run-Fixture($label,$exe,$fixtureArguments,$timeout=300){
 $stdout="$results\$label.stdout.log";$stderr="$results\$label.stderr.log";$start=[DateTime]::UtcNow
 $opts=@{FilePath=$exe;WorkingDirectory=$fixtureCwd;PassThru=$true;RedirectStandardOutput=$stdout;RedirectStandardError=$stderr}
 if($fixtureArguments.Count){$opts.ArgumentList=@($fixtureArguments|ForEach-Object {'"'+[string]$_+'"'})}
 $p=Start-Process @opts;[void]$p.Handle;$done=$p.WaitForExit($timeout*1000)
 if(!$done){Stop-Process -Id $p.Id -Force;$code='timeout'}else{$p.Refresh();$code=$p.ExitCode}
 $r=[pscustomobject]@{label=$label;executable=$exe;arguments=@($fixtureArguments);cwd=$fixtureCwd;exit=$code;seconds=([DateTime]::UtcNow-$start).TotalSeconds;stdout=[IO.File]::ReadAllText($stdout);stderr=[IO.File]::ReadAllText($stderr);desktopIdentity=[Security.Principal.WindowsIdentity]::GetCurrent().Name;sessionId=[Diagnostics.Process]::GetCurrentProcess().SessionId}
 $rows.Add($r);$r|ConvertTo-Json -Depth 5|Set-Content -Encoding UTF8 "$results\$label.json"
}
foreach($name in @('judas_impact_mesh_cost_tests','judas_impact_robustness_tests','judas_origin_admission_tests','judas_character_motor_tests','judas_world_persistence_tests')){if($name-eq 'judas_impact_mesh_cost_tests'){$arguments=@(1)}else{$arguments=@()};Run-Fixture $name "$testRoot\$name.exe" $arguments 300}
Run-Fixture 'm70-actual-physics' "$testRoot\judas_character_lab_tests.exe" @("$out\m70-app") 300
Run-Fixture 'm70-cold-write' "$testRoot\judas_character_lab_tests.exe" @("$out\m70-cold",'cold-write') 300
Run-Fixture 'm70-cold-read' "$testRoot\judas_character_lab_tests.exe" @("$out\m70-cold",'cold-read') 300
foreach($case in @(@('step','cold-write','cold-read'),@('stand','cold-stand-write','cold-stand-read'),@('recovery','cold-recovery-write','cold-recovery-read'))){
 Run-Fixture "m77-$($case[0])-write" "$testRoot\judas_balance_lab_tests.exe" @("$out\m77-$($case[0])",$case[1]) 300
 Run-Fixture "m77-$($case[0])-read" "$testRoot\judas_balance_lab_tests.exe" @("$out\m77-$($case[0])",$case[2]) 300
}

$sceneAfter=(Get-FileHash $scene).Hash.ToLowerInvariant()
$manifest=Get-Content -Raw "$sdk\SDK_MANIFEST.json"|ConvertFrom-Json
$identity=Get-Content -Raw "$sdk\BUILD_INFO.json"|ConvertFrom-Json
[pscustomobject]@{schema=1;utc=[DateTime]::UtcNow.ToString('o');sourceIdentity=$identity;rows=$rows.ToArray();sceneBefore=$sceneBefore;sceneAfter=$sceneAfter;authoredSceneUnchanged=($sceneBefore-eq $sceneAfter);saveRoot=$saveRoot;saveRootWasAbsentBeforeTest=$true;saveFiles=@(Get-ChildItem -LiteralPath $saveRoot -Recurse -File|ForEach-Object {[pscustomobject]@{path=$_.FullName;bytes=$_.Length;sha256=(Get-FileHash -LiteralPath $_.FullName).Hash.ToLowerInvariant()}});pristinePackageUnchanged=$true;originalEditorPreferenceHashes=$prefsBefore;originalEditorPreferencesRestored=$true;graphicsScope='Stock QXL startup separately observed; software overlay uses existing llvmpipe only in disposable task copy, not SDK or exported distribution; no physical GPU/audio/controller acceptance';localDesktopUser=[Security.Principal.WindowsIdentity]::GetCurrent().Name;sessionId=[Diagnostics.Process]::GetCurrentProcess().SessionId}|ConvertTo-Json -Depth 7|Set-Content -Encoding UTF8 "$work\GUI_RESULTS.json"
