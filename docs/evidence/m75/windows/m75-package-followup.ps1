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
 $stock="$build\windows-sdk";$tooling="$root\Tooling\M75";$run="C:\M75 $([char]0xe9) $([char]0x3b1)";$projectCopy="$run\Project";$project="$projectCopy\terrain_lab.judasproj"
 $env:PATH="$tooling;$env:PATH";$env:GALLIUM_DRIVER='llvmpipe';$env:JUDAS_ENGINE_ROOT=$tooling
 if((Test-Path "$stock\opengl32.dll") -or (Test-Path "$stock\libgallium_wgl.dll")){throw 'Stock SDK contains VM-only Mesa'}
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
