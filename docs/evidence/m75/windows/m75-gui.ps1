# Native non-elevated interactive editor/application checks; VM Mesa is test-only.
$ErrorActionPreference='Stop';$ProgressPreference='SilentlyContinue'
$root='C:\JudasValidation';$base='http://192.168.122.1:8872';$source="$root\Source";$build="$root\Build";$receipts="$root\Receipts\M75"
function Upload($p){Invoke-WebRequest -UseBasicParsing "$base/$([IO.Path]::GetFileName($p))" -Method Post -InFile $p|Out-Null}
function Report($name,$data){$p="$receipts\$name";$data|ConvertTo-Json -Depth 12|Set-Content $p -Encoding UTF8;Upload $p}
function Quote($s){return '"'+$s+'"'}
function Run($label,$exe,$arguments,$timeout=300,$cwd=$root){
 $start=Get-Date;$stdout="$receipts\$label.stdout.log";$stderr="$receipts\$label.stderr.log"
 $o=@{FilePath=$exe;WorkingDirectory=$cwd;PassThru=$true;RedirectStandardOutput=$stdout;RedirectStandardError=$stderr};if($arguments.Count){$o.ArgumentList=$arguments}
 $p=Start-Process @o;[void]$p.Handle;if(!$p.WaitForExit($timeout*1000)){Stop-Process -Id $p.Id -Force;$code='timeout'}else{$p.Refresh();$code=$p.ExitCode}
 Upload $stdout;Upload $stderr;Report "$label.result.json" @{exit=$code;seconds=((Get-Date)-$start).TotalSeconds;sha256=(Get-FileHash $exe).Hash.ToLower();session=(Get-Process -Id $PID).SessionId;admin=([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)}
 if($code -ne 0 -or ((Get-Content $stdout,$stderr -Raw) -join "`n") -match '(?m)^FAIL |\[M75 terrain editor\] FAIL:|\[editor autotest\] FAILED'){throw "$label failed: $code"}
}
function Artifact($path,$name){if(!(Test-Path $path)){throw "Missing artifact $path"};$p="$receipts\$name";Copy-Item $path $p -Force;Upload $p}
try{
 $native=Get-Content "$receipts\m75-windows-native-summary.json" -Raw|ConvertFrom-Json;if($native.build -ne 0){throw 'Native build not accepted'}
 $stock="$build\windows-sdk";if((Test-Path "$stock\opengl32.dll") -or (Test-Path "$stock\libgallium_wgl.dll")){throw 'Stock SDK contains VM Mesa'}
 $run="C:\M75 $([char]0xe9) $([char]0x3b1)";$tooling="$root\Tooling\M75";New-Item -Force -ItemType Directory $tooling|Out-Null
 Copy-Item "$stock\*" $tooling -Recurse -Force;Copy-Item "$build\Release\judas_terrain_application_tests.exe" $tooling
 Copy-Item "$root\Mesa\x64\opengl32.dll","$root\Mesa\x64\libgallium_wgl.dll" $tooling
 $env:PATH="$tooling;$env:PATH";$env:GALLIUM_DRIVER='llvmpipe';$env:JUDAS_ENGINE_ROOT=$tooling
 # Restore verified transferred inputs before native GUI; CLI uses disposable copy.
 Expand-Archive -Force "$root\m75-source.zip" "$root\M75Extract"
 Invoke-WebRequest -UseBasicParsing "$base/m75-assets.zip" -OutFile "$root\m75-assets.zip"
 Invoke-WebRequest -UseBasicParsing "$base/m75-assets.json" -OutFile "$root\m75-assets.json"
 $assetExpected=Get-Content "$root\m75-assets.json" -Raw|ConvertFrom-Json
 if((Get-FileHash "$root\m75-assets.zip").Hash.ToLower() -ne $assetExpected.archiveSha256){throw 'Final asset transfer archive mismatch'}
 Expand-Archive -Force "$root\m75-assets.zip" "$root\M75Extract"
 Expand-Archive -Force "$root\m75-assets.zip" $source
 $pristine="$root\M75Extract\projects\terrain_lab";$expected=Get-Content "$root\m75-source.json" -Raw|ConvertFrom-Json
 $proof=@();foreach($e in $assetExpected.files.PSObject.Properties){if($e.Name -like 'projects/terrain_lab/*'){$p=Join-Path "$root\M75Extract" $e.Name;$hash=(Get-FileHash $p).Hash.ToLower();$proof+=@{path=$e.Name;expected=$e.Value;actual=$hash;matches=($hash -eq $e.Value)}}}
 Report 'm75-windows-pristine-project-proof.json' $proof;if(@($proof|Where-Object {!$_.matches}).Count){throw 'Pristine project mismatch'}
 $projectCopy="$run\Project";if(Test-Path $projectCopy){Remove-Item $projectCopy -Recurse -Force};Copy-Item $pristine $projectCopy -Recurse
 $project="$projectCopy\terrain_lab.judasproj"
 Run 'm75-windows-cli-create' "$stock\judas_terrain_author.exe" @('create',(Quote "$run\created.judasterrain"),'33','33','32','32')
 Run 'm75-windows-cli-brush' "$stock\judas_terrain_author.exe" @('brush',(Quote "$run\created.judasterrain"),'raise','0','0','0','4','1')
 Run 'm75-windows-cli-validate' "$stock\judas_terrain_author.exe" @('validate',(Quote "$run\created.judasterrain"))
 Run 'm75-windows-app-probe' "$tooling\judas_terrain_application_tests.exe" @((Quote $project),'probe',(Quote "$run\Probe"))
 Artifact "$run\Probe\runtime.png" 'm75-windows-runtime.png'
 foreach($mode in @('write','read','changed','missing')){Run "m75-windows-save-$mode" "$tooling\judas_terrain_application_tests.exe" @((Quote $project),$mode,(Quote "$run\Save"))}
 Artifact "$run\Save\expected.json" 'm75-windows-save-expected.json'
 $env:JUDAS_EDITOR_AUTOTEST="$receipts\m75-windows-editor";$env:JUDAS_EDITOR_AUTOTEST_TERRAIN='Imports/landscape.judasterrain'
 Run 'm75-windows-editor' "$tooling\judas_editor.exe" @((Quote $project))
 Remove-Item Env:\JUDAS_EDITOR_AUTOTEST,Env:\JUDAS_EDITOR_AUTOTEST_TERRAIN
 $log=(Get-Content "$receipts\m75-windows-editor.stdout.log","$receipts\m75-windows-editor.stderr.log" -Raw) -join "`n"
 if($log -notmatch '\[M75 terrain editor\] failures 0' -or $log -notmatch 'authored scene after play/stop is IDENTICAL'){throw 'Editor workflow incomplete'}
 foreach($suffix in @('edit.png','play.png','saved.judas')){Upload "$receipts\m75-windows-editor.$suffix"}
 # Native sculpt/cook naturally invalidates navigation; rebake the normal input.
 foreach($scene in @('landscape','rotated')){Run "m75-windows-nav-$scene" "$stock\judas_navigation_bake.exe" @((Quote $project),"Scenes/$scene.judas")}
 $dest="$root\Packages\M75";$moved="$root\Packages\Moved M75 $([char]0xe9) $([char]0x3b1)"
 Run 'm75-windows-export' "$stock\judas_export.exe" @((Quote $project),(Quote $dest),(Quote "$stock\judas.exe"))
 $files=@(Get-ChildItem $dest -Recurse -File|ForEach-Object {$_.FullName.Substring($dest.Length+1).Replace('\','/')})
 $authoring=@($files|Where-Object {$_ -match '^(Imports|Authoring)/|\.judasterrain$'})
 if($authoring.Count -or (Test-Path "$dest\opengl32.dll") -or (Test-Path "$dest\libgallium_wgl.dll")){throw 'Runtime package contains authoring/VM files'}
 Report 'm75-windows-stock-package-proof.json' @{files=$files;authoringFiles=$authoring;vmMesaIncluded=$false;runtimeHash=(Get-FileHash "$dest\judas.exe").Hash.ToLower()}
 Move-Item $dest $moved;Copy-Item "$root\Mesa\x64\opengl32.dll","$root\Mesa\x64\libgallium_wgl.dll" $moved
 $unrelated="$root\Unrelated M75";New-Item -Force -ItemType Directory $unrelated|Out-Null
 Remove-Item Env:\JUDAS_ENGINE_ROOT
 $hidden="$run\Project unavailable";Move-Item $projectCopy $hidden
 $original="$source\projects\terrain_lab";$originalHidden="$source\projects\terrain_lab unavailable";Move-Item $original $originalHidden
 $extractHidden="$root\M75ExtractUnavailable";Move-Item "$root\M75Extract" $extractHidden
 try{Run 'm75-windows-moved-probe' "$tooling\judas_terrain_application_tests.exe" @((Quote "$moved\game.judasproj"),'probe',(Quote "$run\Moved")) 300 $unrelated
  # Plain packaged executable auto-discovers its marker from an unrelated cwd.
  Invoke-WebRequest -UseBasicParsing "$base/m75.harness" -OutFile "$root\m75.harness"
  $env:JUDAS_TEST_SCRIPT="$root\m75.harness";Run 'm75-windows-moved-runtime' "$moved\judas.exe" @() 300 $unrelated
  Remove-Item Env:\JUDAS_TEST_SCRIPT
 }finally{Move-Item $hidden $projectCopy;Move-Item $originalHidden $original;Move-Item $extractHidden "$root\M75Extract"}
 Artifact "$run\Moved\runtime.png" 'm75-windows-moved.png'
 Report 'm75-windows-gui-summary.json' @{probe=0;saveLoad=0;editor=0;export=0;movedProbe=0;movedRuntime=0;package=$moved;authoringUnavailableDuringMovedRun=$true;unrelatedCwd=$unrelated;shortSpacesUnicode=$true;VMsoftwareOpenGL=$true;hardwareHumanValidated=$false}
 exit 0
}catch{$ErrorActionPreference='Continue';Report 'm75-windows-gui-failure.json' @{message=$_.Exception.Message};Write-Error $_;exit 1}
