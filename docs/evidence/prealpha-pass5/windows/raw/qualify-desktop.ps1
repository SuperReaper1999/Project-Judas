$ErrorActionPreference='Stop';$ProgressPreference='SilentlyContinue'
$task='P:\JudasValidation\PreAlphaPass5';$previous='P:\JudasValidation\PreAlphaPass4';$source="$previous\Source";$release="$previous\Build\Release";$work="$task\GuiWork";$results="$work\Results";$apps="$work\QualifiedApps";$native="$work\NativeTests"
foreach($p in @($results,$apps,$native,"$work\Projects")){if(Test-Path -LiteralPath $p){throw "Preserve prior attempt: $p"}}
New-Item -ItemType Directory -Force $results,$apps,$native,"$work\Temp","$work\UnrelatedCwd"|Out-Null
$env:TEMP="$work\Temp";$env:TMP=$env:TEMP;$env:PATH='C:\Windows\System32;C:\Windows';$env:JUDAS_RESOURCE_MODE='blocking';$env:PYTHONDONTWRITEBYTECODE='1'
$rows=New-Object 'System.Collections.Generic.List[object]'
function Run-Native($label,$exe,$arguments,$expected=0,$timeout=180,$cwd="$work\UnrelatedCwd",$required=''){
 $stdout="$results\$label.stdout.log";$stderr="$results\$label.stderr.log";$start=[DateTime]::UtcNow
 $options=@{FilePath=$exe;WorkingDirectory=$cwd;PassThru=$true;RedirectStandardOutput=$stdout;RedirectStandardError=$stderr}
 if($arguments.Count){$options.ArgumentList=@($arguments|ForEach-Object {'"'+[string]$_+'"'})}
 $process=Start-Process @options;[void]$process.Handle;$done=$process.WaitForExit($timeout*1000)
 if(!$done){Stop-Process -Id $process.Id -Force;$code='timeout'}else{$process.Refresh();$code=$process.ExitCode;if($null-eq $code){throw "Missing native exit code: $label"}}
 $outText=[IO.File]::ReadAllText($stdout);$errText=[IO.File]::ReadAllText($stderr)
 $row=[pscustomobject]@{label=$label;executable=$exe;arguments=@($arguments);cwd=$cwd;exit=$code;expected=$expected;passed=($code-eq $expected -and (!$required -or (($outText+$errText)-match $required)));requiredOutput=$required;seconds=([DateTime]::UtcNow-$start).TotalSeconds;stdout=$outText;stderr=$errText;desktopIdentity=[Security.Principal.WindowsIdentity]::GetCurrent().Name;sessionId=[Diagnostics.Process]::GetCurrentProcess().SessionId}
 $rows.Add($row);$row|ConvertTo-Json -Depth 5|Set-Content -Encoding UTF8 "$results\$label.json"
}
# This is a deliberately small qualification staging directory, not a republished SDK.
foreach($destination in @($apps,$native)){
 foreach($file in Get-ChildItem -LiteralPath $release -Filter '*.dll'){Copy-Item -LiteralPath $file.FullName "$destination\$($file.Name)"}
 foreach($directory in @('windows-runtime','windows-licenses')){Copy-Item -LiteralPath "$release\$directory" "$destination\$directory" -Recurse}
 Copy-Item -LiteralPath "$source\assets" "$destination\assets" -Recurse
 New-Item -ItemType Directory -Force "$destination\third_party"|Out-Null
 Copy-Item -LiteralPath "$source\third_party\RUNTIME_NOTICES.txt" "$destination\third_party\RUNTIME_NOTICES.txt"
 Copy-Item -LiteralPath "$source\LICENSE" "$destination\LICENSE"
}
foreach($name in @('judas','judas_editor','judas_export')){Copy-Item -LiteralPath "$release\$name.exe" "$apps\$name.exe";Run-Native "$name-identity" "$apps\$name.exe" @('--build-info-json')}
$identity=Get-Content -Raw "$results\judas-identity.stdout.log"|ConvertFrom-Json
$expected=(Get-Content -Raw "$task\SOURCE_VERIFIED.json"|ConvertFrom-Json).afterInfo
if($identity.source_revision-ne $expected.revision -or $identity.source_fingerprint-ne $expected.source_fingerprint -or $identity.source_dirty -or $identity.source_origin-ne 'archive'){throw 'Native application identity does not match verified archived inputs'}
$identity|ConvertTo-Json -Depth 5|Set-Content -Encoding UTF8 "$apps\BUILD_INFO.json"
$testNames=@('judas_impact_candidate_cache_tests','judas_impact_timing_tests','judas_contact_cache_tests','judas_physics_continuation_tests','judas_contact_lifecycle_tests','judas_character_motor_tests','judas_rigid_motion_tests','judas_impact_robustness_tests','judas_world_persistence_tests','judas_origin_admission_tests','judas_impact_mesh_cost_tests','judas_character_lab_tests','judas_balance_lab_tests')
foreach($name in $testNames){Copy-Item -LiteralPath "$release\$name.exe" "$native\$name.exe"}
Copy-Item -LiteralPath "$previous\Projects" "$work\Projects" -Recurse
$fixtureCwd="$work\Projects";$project="$fixtureCwd\projects\physics_stress_lab\physics_stress_lab.judasproj"
$projectOriginal=[IO.File]::ReadAllText($project)
$oldSaveId='317517ccb7462d4379370eff1453c07d42388a98a9916d4a981946d4188dec65'
$saveId='b403b5b551b1d8729bb1dc819a8dc9a9f11b32865c66dc67e2017f8924934d92'
if(!$projectOriginal.Contains($oldSaveId)){throw 'Unexpected existing physics-lab save identity'}
[IO.File]::WriteAllText($project,$projectOriginal.Replace($oldSaveId,$saveId),[Text.UTF8Encoding]::new($false))
$saveRoot=Join-Path $env:LOCALAPPDATA ('judas\games\'+$saveId)
if(Test-Path -LiteralPath $saveRoot){throw 'Task-isolated save root already exists; preserve prior attempt'}
$scene=Join-Path (Split-Path $project) 'Scenes\room128.judas';$sceneBefore=(Get-FileHash -LiteralPath $scene).Hash.ToLowerInvariant()
@'
FRAMES 5
LOG_EVERY 0
'@|Set-Content -Encoding ASCII "$work\startup.txt"
$env:JUDAS_TEST_SCRIPT="$work\startup.txt"
Run-Native 'stock-qxl-startup' "$apps\judas.exe" @($project) 1 45 "$work\UnrelatedCwd" 'OpenGL 3.3'
Remove-Item Env:JUDAS_TEST_SCRIPT
foreach($destination in @($apps,$native)){foreach($name in @('opengl32.dll','libgallium_wgl.dll')){Copy-Item -LiteralPath "C:\JudasValidation\Mesa\x64\$name" "$destination\$name"}}
$env:GALLIUM_DRIVER='llvmpipe';$env:MESA_SHADER_CACHE_DIR="$work\ShaderCache"
foreach($name in @('judas_impact_candidate_cache_tests','judas_impact_timing_tests','judas_contact_cache_tests','judas_physics_continuation_tests','judas_contact_lifecycle_tests','judas_character_motor_tests','judas_rigid_motion_tests','judas_impact_robustness_tests','judas_world_persistence_tests','judas_origin_admission_tests')){Run-Native $name "$native\$name.exe" @() 0 300 $fixtureCwd}
Run-Native 'candidate-cache-revision-negative' "$native\judas_impact_candidate_cache_tests.exe" @('--omit-revision') 1 60 $fixtureCwd
Run-Native 'curved-original-continuation-known-failure' "$native\judas_impact_mesh_cost_tests.exe" @(60,'',1.2) 1 300 $fixtureCwd 'checks 372 failures 4'
Run-Native 'curved-first-arrival-positive' "$native\judas_impact_mesh_cost_tests.exe" @(60,'',1.2,'entry') 0 300 $fixtureCwd 'checks 372 failures 0'
$out="$work\ColdCases"
Run-Native 'm70-actual-physics' "$native\judas_character_lab_tests.exe" @("$out\m70-app") 0 300 $fixtureCwd 'SUMMARY 98 checks 0 failures'
Run-Native 'm70-cold-write' "$native\judas_character_lab_tests.exe" @("$out\m70-cold",'cold-write') 0 300 $fixtureCwd
Run-Native 'm70-cold-read' "$native\judas_character_lab_tests.exe" @("$out\m70-cold",'cold-read') 0 300 $fixtureCwd
foreach($case in @(@('step','cold-write','cold-read'),@('stand','cold-stand-write','cold-stand-read'),@('recovery','cold-recovery-write','cold-recovery-read'))){
 Run-Native "m77-$($case[0])-write" "$native\judas_balance_lab_tests.exe" @("$out\m77-$($case[0])",$case[1]) 0 300 $fixtureCwd
 Run-Native "m77-$($case[0])-read" "$native\judas_balance_lab_tests.exe" @("$out\m77-$($case[0])",$case[2]) 0 300 $fixtureCwd 'restored native task reaches measured physical standing'
}
$prefs=Join-Path $env:APPDATA 'Judas\Editor';$prefsBackup="$results\OriginalEditorPreferences";New-Item -ItemType Directory -Force $prefsBackup|Out-Null;$prefsBefore=@{}
foreach($name in @('workspace.json','windows.ini')){$path=Join-Path $prefs $name;if(Test-Path -LiteralPath $path){$prefsBefore[$name]=(Get-FileHash -LiteralPath $path).Hash.ToLowerInvariant();Copy-Item -LiteralPath $path "$prefsBackup\$name"}}
try{
 $env:JUDAS_EDITOR_AUTOTEST="$results\editor";$env:JUDAS_EDITOR_AUTOTEST_AUTHORING='1'
 Run-Native 'editor-authoring-play-stop' "$apps\judas_editor.exe" @($project) 0 300 "$work\UnrelatedCwd" 'authored scene after play/stop is IDENTICAL'
}finally{
 Remove-Item Env:JUDAS_EDITOR_AUTOTEST,Env:JUDAS_EDITOR_AUTOTEST_AUTHORING -ErrorAction SilentlyContinue
 foreach($name in @('workspace.json','windows.ini')){$path=Join-Path $prefs $name;if(Test-Path -LiteralPath $path){Copy-Item -LiteralPath $path "$results\tested-$name"};if($prefsBefore.ContainsKey($name)){Copy-Item -LiteralPath "$prefsBackup\$name" $path -Force;if((Get-FileHash -LiteralPath $path).Hash.ToLowerInvariant()-ne $prefsBefore[$name]){throw 'Could not byte-restore original editor preferences'}}elseif(Test-Path -LiteralPath $path){Remove-Item -LiteralPath $path}}
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
SCREENSHOT 250 P:/JudasValidation/PreAlphaPass5/GuiWork/Results/runtime.png
'@|Set-Content -Encoding ASCII "$work\runtime.txt"
$env:JUDAS_TEST_SCRIPT="$work\runtime.txt"
Run-Native 'runtime-save-load-transition-pause-reload' "$apps\judas.exe" @($project) 0 240
@'
FRAMES 3
LOG_EVERY 0
EXPECT_SCENE deliberately-absent-scene 1
'@|Set-Content -Encoding ASCII "$work\negative.txt"
$env:JUDAS_TEST_SCRIPT="$work\negative.txt"
Run-Native 'runtime-oracle-negative' "$apps\judas.exe" @($project) 1 90
Remove-Item Env:JUDAS_TEST_SCRIPT
Run-Native 'ordinary-consumer-export' "$apps\judas_export.exe" @($project,"$work\Export Original") 0 180
$moved="$work\Moved Game $([char]0xe9) $([char]0x3b1)"
Move-Item -LiteralPath "$work\Export Original" $moved
$pristineFiles=@(Get-ChildItem -LiteralPath $moved -Recurse -File|ForEach-Object {[pscustomobject]@{path=$_.FullName.Substring($moved.Length+1).Replace('\','/');bytes=$_.Length;sha256=(Get-FileHash -LiteralPath $_.FullName).Hash.ToLowerInvariant()}})
$pristineFiles|ConvertTo-Json -Depth 4|Set-Content -Encoding UTF8 "$task\PRISTINE_PACKAGE_FILES.json"
Run-Native 'moved-standalone-identity' "$moved\judas.exe" @('--build-info-json')
$software="$work\Moved Software Game $([char]0xe9) $([char]0x3b1)";Copy-Item -LiteralPath $moved $software -Recurse
foreach($name in @('opengl32.dll','libgallium_wgl.dll')){Copy-Item -LiteralPath "C:\JudasValidation\Mesa\x64\$name" "$software\$name"}
$env:JUDAS_TEST_SCRIPT="$work\runtime.txt"
Run-Native 'moved-standalone-full-harness' "$software\judas.exe" @() 0 240
Remove-Item Env:JUDAS_TEST_SCRIPT
foreach($file in $pristineFiles){if((Get-FileHash -LiteralPath (Join-Path $moved $file.path)).Hash.ToLowerInvariant()-ne $file.sha256){throw 'Pristine moved package changed: '+$file.path}}
$sceneAfter=(Get-FileHash -LiteralPath $scene).Hash.ToLowerInvariant()
$binaryFiles=@(Get-ChildItem -LiteralPath $apps,$native -Filter '*.exe'|ForEach-Object {[pscustomobject]@{path=$_.FullName;bytes=$_.Length;sha256=(Get-FileHash -LiteralPath $_.FullName).Hash.ToLowerInvariant()}})
$record=[pscustomobject]@{schema=1;utc=[DateTime]::UtcNow.ToString('o');sourceIdentity=$identity;rows=$rows.ToArray();allExpectedResultsObserved=(@($rows|Where-Object {!$_.passed}).Count-eq 0);knownUnfixedContinuationFailurePreserved=$true;binaryFiles=$binaryFiles;authoredSceneUnchanged=($sceneBefore-eq $sceneAfter);sceneBefore=$sceneBefore;sceneAfter=$sceneAfter;projectOnlyIsolationChange='Task copy saveIdentity replaces the original identity; all other project bytes unchanged';taskSaveIdentity=$saveId;saveRoot=$saveRoot;saveFiles=@(Get-ChildItem -LiteralPath $saveRoot -Recurse -File|ForEach-Object {[pscustomobject]@{path=$_.FullName;bytes=$_.Length;sha256=(Get-FileHash -LiteralPath $_.FullName).Hash.ToLowerInvariant()}});pristinePackageUnchanged=$true;originalEditorPreferencesRestored=$true;originalEditorPreferenceHashes=$prefsBefore;graphicsScope='Stock QXL failure separately retained; llvmpipe overlay only in disposable task app copies, not SDK/distribution; no physical GPU/audio/controller validation';noWindowsPerformanceGate=$true;desktopIdentity=[Security.Principal.WindowsIdentity]::GetCurrent().Name;sessionId=[Diagnostics.Process]::GetCurrentProcess().SessionId}
$record|ConvertTo-Json -Depth 8|Set-Content -Encoding UTF8 "$task\FUNCTIONAL_RESULTS.json"
if(!$record.allExpectedResultsObserved -or !$record.authoredSceneUnchanged){exit 1}
