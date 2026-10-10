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
 $summary=Get-Content "$receipts\m75-windows-ui-build-summary.json" -Raw|ConvertFrom-Json;if($summary.build -ne 0){throw 'UI build not passed'}
 $tooling="$root\Tooling\M75";$stock="$build\windows-sdk";Copy-Item "$build\Release\judas_editor.exe" $tooling -Force;Copy-Item "$build\Release\judas_editor.exe" $stock -Force
 $env:PATH="$tooling;$env:PATH";$env:GALLIUM_DRIVER='llvmpipe';$env:JUDAS_ENGINE_ROOT=$tooling
 $copy="$root\M75UiProject";if(Test-Path $copy){Remove-Item $copy -Recurse -Force};Copy-Item "$source\projects\terrain_lab" $copy -Recurse
 $env:JUDAS_EDITOR_AUTOTEST="$receipts\m75-windows-ui-editor";$env:JUDAS_EDITOR_AUTOTEST_TERRAIN='Imports/landscape.judasterrain'
 Run 'm75-windows-ui-editor' "$tooling\judas_editor.exe" @((Quote "$copy\terrain_lab.judasproj"))
 Remove-Item Env:\JUDAS_EDITOR_AUTOTEST,Env:\JUDAS_EDITOR_AUTOTEST_TERRAIN
 $log=(Get-Content "$receipts\m75-windows-ui-editor.stdout.log","$receipts\m75-windows-ui-editor.stderr.log" -Raw) -join "`n"
 if($log -notmatch 'PASS: real panel click opens selected source and activates brush' -or $log -notmatch '\[M75 terrain editor\] failures 0' -or $log -notmatch 'authored scene after play/stop is IDENTICAL'){throw 'Native UI activation workflow incomplete'}
 foreach($suffix in @('activation.png','edit.png','play.png')){Upload "$receipts\m75-windows-ui-editor.$suffix"}
 Report 'm75-windows-ui-summary.json' @{editor=0;terrainAssertions=13;actualUiActivation=$true;sceneRestored=$true;hardwareHumanValidated=$false;source=$summary.source;exe=(Get-FileHash "$tooling\judas_editor.exe").Hash.ToLower()};exit 0
}catch{Report 'm75-windows-ui-failure.json' @{error=($_|Out-String)};exit 1}
