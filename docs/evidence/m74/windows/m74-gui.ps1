# Actual-GL M74 checks. Run as interactive Conner, LeastPrivilege, only after
# m74-build.ps1 succeeded. QXL requires VM-only software GL; hardware is untested.
$ErrorActionPreference='Stop';$ProgressPreference='SilentlyContinue'
$root='C:\JudasValidation';$base='http://192.168.122.1:8872'
$source="$root\Source";$build="$root\Build";$receipts="$root\Receipts\M74"
function Upload($path){Invoke-WebRequest -UseBasicParsing "$base/$([IO.Path]::GetFileName($path))" -Method Post -InFile $path|Out-Null}
function JsonReport($name,$data){$path="$receipts\$name";$data|ConvertTo-Json -Depth 12|Set-Content $path -Encoding UTF8;Upload $path}
function Quote($text){return '"'+$text+'"'}
function Run($label,$exe,$arguments,$timeout=300,$cwd=$source){
 $start=Get-Date;$stdout="$receipts\$label.stdout.log";$stderr="$receipts\$label.stderr.log"
 $options=@{FilePath=$exe;WorkingDirectory=$cwd;PassThru=$true;RedirectStandardOutput=$stdout;RedirectStandardError=$stderr}
 if($arguments.Count){$options.ArgumentList=$arguments}
 $process=Start-Process @options;[void]$process.Handle
 if(!$process.WaitForExit($timeout*1000)){Stop-Process -Id $process.Id -Force;$code='timeout'}else{$process.Refresh();$code=$process.ExitCode;if($null -eq $code){throw "No exit code: $label"}}
 Upload $stdout;Upload $stderr
 $record=@{label=$label;exit=$code;seconds=((Get-Date)-$start).TotalSeconds;exe=$exe;exeSha256=(Get-FileHash $exe).Hash.ToLower();session=(Get-Process -Id $PID).SessionId;admin=([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)}
 JsonReport "$label.result.json" $record
 if($code -ne 0){throw "Native GUI check $label failed: $code"}
 if((Get-Content $stdout,$stderr -Raw) -match '(?m)^FAULT |(?:M74 editor retarget\] FAIL|TestHarness\] (?:script callback failed|service wait timed out))'){throw "Native GUI check $label reported a runtime failure"}
 return $record
}
function UploadArtifact($path,$name){if(!(Test-Path $path)){throw "Missing verification artifact: $path"};$copy="$receipts\$name";Copy-Item $path $copy -Force;Upload $copy}
try {
 $summary=Get-Content "$receipts\m74-windows-native-summary.json" -Raw|ConvertFrom-Json
 foreach($key in @('core','cook','lab','editorDraft','profile')){if($summary.$key -ne 0){throw "Native preparation did not pass: $key"}}
 $expected=Get-Content "$root\m74-final-source.json" -Raw|ConvertFrom-Json
 $before=@{};foreach($entry in $expected.files.PSObject.Properties){
  $hash=(Get-FileHash (Join-Path $source $entry.Name)).Hash.ToLower()
  if($hash -ne $entry.Value){throw "Candidate changed after native build: $($entry.Name)"}
  if($entry.Name -like 'projects/retarget_lab/*'){$before[$entry.Name]=$hash}
 }
 $stamp=Get-Date -Format 'yyyyMMdd-HHmmss';$run="$receipts\GUI-$stamp";New-Item -ItemType Directory $run|Out-Null
 $stock="$build\windows-sdk";$tooling="$root\Tooling\M74-$stamp";New-Item -Force -ItemType Directory $tooling|Out-Null
 if((Test-Path "$stock\opengl32.dll") -or (Test-Path "$stock\libgallium_wgl.dll")){throw 'Stock SDK contains VM-only Mesa; rebuild normal SDK before verification'}
 Copy-Item "$stock\*" $tooling -Recurse -Force
 Copy-Item "$build\Release\judas_retarget_application_tests.exe" $tooling
 Copy-Item "$root\Mesa\x64\opengl32.dll","$root\Mesa\x64\libgallium_wgl.dll" $tooling
 $env:PATH="$tooling;$env:PATH";$env:GALLIUM_DRIVER='llvmpipe';$env:JUDAS_ENGINE_ROOT=$tooling
 $project="$source\projects\retarget_lab\retarget_lab.judasproj"
 $probe=Run 'm74-windows-app-probe' "$tooling\judas_retarget_application_tests.exe" @((Quote $project),'probe',(Quote "$run\Probe")) 300
 UploadArtifact "$run\Probe\runtime.png" 'm74-windows-runtime.png'
 UploadArtifact "$run\Probe\profile.json" 'm74-windows-application-profile.json'
 $save=Run 'm74-windows-app-save' "$tooling\judas_retarget_application_tests.exe" @((Quote $project),'write',(Quote "$run\Save")) 300
 $load=Run 'm74-windows-app-load' "$tooling\judas_retarget_application_tests.exe" @((Quote $project),'read',(Quote "$run\Save")) 300
 UploadArtifact "$run\Save\expected.json" 'm74-windows-save-expected.json'
 $env:JUDAS_EDITOR_AUTOTEST="$receipts\m74-windows-editor"
 $env:JUDAS_EDITOR_AUTOTEST_AUTHORING='1';$env:JUDAS_EDITOR_AUTOTEST_RETARGET='Imports/broad.judasretarget'
 $editor=Run 'm74-windows-editor-preview-play-stop' "$tooling\judas_editor.exe" @((Quote $project)) 300
 Remove-Item Env:\JUDAS_EDITOR_AUTOTEST,Env:\JUDAS_EDITOR_AUTOTEST_AUTHORING,Env:\JUDAS_EDITOR_AUTOTEST_RETARGET
 $editorLog=(Get-Content "$receipts\m74-windows-editor-preview-play-stop.stdout.log","$receipts\m74-windows-editor-preview-play-stop.stderr.log" -Raw) -join "`n"
 if($editorLog -notmatch '\[M74 editor retarget\] READY source_joints=' -or $editorLog -notmatch 'authored scene after play/stop is IDENTICAL' -or $editorLog -match '\bFAIL\b'){throw 'Editor did not complete actual retarget preview and ordinary Play/Stop proof'}
 foreach($name in @('m74-windows-editor.retarget.png','m74-windows-editor.edit.png','m74-windows-editor.play.png','m74-windows-editor.saved.judas')){Upload "$receipts\$name"}
 $unchanged=@();foreach($entry in $before.GetEnumerator()){$actual=(Get-FileHash (Join-Path $source $entry.Key)).Hash.ToLower();$unchanged+=@{path=$entry.Key;before=$entry.Value;after=$actual;matches=($actual -eq $entry.Value)}}
 JsonReport 'm74-windows-editor-authored-proof.json' $unchanged
 if(@($unchanged|Where-Object {!$_.matches}).Count){throw 'Editor/application checks altered authored retarget lab bytes'}
 $dest="$root\Packages\M74-$stamp";$moved="$root\Packages\Moved M74 Retarget Lab $([char]0x00e9) $([char]0x03b1) $stamp"
 # Export from the pristine native SDK, never from the Mesa tooling copy.
 $export=Run 'm74-windows-export' "$stock\judas_export.exe" @((Quote $project),(Quote $dest),(Quote "$stock\judas.exe")) 300
 $files=@(Get-ChildItem $dest -Recurse -File|ForEach-Object {$_.FullName.Substring($dest.Length+1).Replace('\','/')})
 $authoring=@($files|Where-Object {$_ -match '^(Sources|Imports|Authoring)/|\.(gltf|glb|fbx|judasretarget|judasimport)$'})
 if($authoring.Count){throw "Runtime export contains authoring sources/profiles: $($authoring -join ', ')"}
 if((Test-Path "$dest\opengl32.dll") -or (Test-Path "$dest\libgallium_wgl.dll")){throw 'Normal exported package unexpectedly contains Mesa'}
 JsonReport 'm74-windows-stock-package-proof.json' @{path=$dest;files=$files;authoringFiles=$authoring;vmMesaIncluded=$false;runtimeSha256=(Get-FileHash "$dest\judas.exe").Hash.ToLower()}
 Move-Item $dest $moved
 # App-local software GL is added only to this VM test package after stock proof.
 Copy-Item "$root\Mesa\x64\opengl32.dll","$root\Mesa\x64\libgallium_wgl.dll" $moved
 Invoke-WebRequest -UseBasicParsing "$base/m74.harness" -OutFile "$root\m74.harness"
 $unrelated="$root\Unrelated M74 working directory";New-Item -Force -ItemType Directory $unrelated|Out-Null
 $env:JUDAS_TEST_SCRIPT="$root\m74.harness";$env:JUDAS_PROFILE='1';$env:JUDAS_PROFILE_OUTPUT="$receipts\m74-windows-moved-profile.json"
 Remove-Item Env:\JUDAS_ENGINE_ROOT
 # Hide the entire project during launch: the moved package cannot fall back to
 # the original sources, profiles, recipes or cooked source-project assets.
 $projectDirectory="$source\projects\retarget_lab";$hidden="$source\projects\retarget_lab unavailable $stamp"
 Move-Item $projectDirectory $hidden
 try{$runtime=Run 'm74-windows-moved-runtime' "$moved\judas.exe" @() 300 $unrelated}finally{Move-Item $hidden $projectDirectory}
 Remove-Item Env:\JUDAS_TEST_SCRIPT,Env:\JUDAS_PROFILE,Env:\JUDAS_PROFILE_OUTPUT
 UploadArtifact "$unrelated\m74-moved.png" 'm74-windows-moved.png';Upload "$receipts\m74-windows-moved-profile.json"
 JsonReport 'm74-windows-gui-summary.json' @{probe=$probe.exit;save=$save.exit;load=$load.exit;editor=$editor.exit;export=$export.exit;moved=$runtime.exit;package=$moved;sourceProjectUnavailableDuringMovedRun=$true;unrelatedWorkingDirectory=$unrelated;spacesAndNonAsciiPath=$true;stockPackageMesa=$false;vmOnlyExtraDlls=@('opengl32.dll','libgallium_wgl.dll');gpuHardwareValidated=$false;finished=(Get-Date).ToString('o')}
 exit 0
}catch {
 $ErrorActionPreference='Continue';JsonReport 'm74-windows-gui-failure.json' @{message=$_.Exception.Message;finished=(Get-Date).ToString('o');gpuHardwareValidated=$false};Write-Error $_;exit 1
}
