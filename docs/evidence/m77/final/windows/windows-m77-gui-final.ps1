$ErrorActionPreference='Stop';$ProgressPreference='SilentlyContinue';$root='C:\JudasValidation';$base='http://192.168.122.1:8872';$receipt="$root\Receipts\M77";$bin="$root\Build\Release";$staged="$root\M77TestBin"
function Upload($p){Invoke-WebRequest -UseBasicParsing "$base/$([IO.Path]::GetFileName($p))" -Method Post -InFile $p|Out-Null}
function Quote($s){return '"'+$s+'"'}
$rows=@()
function Run($label,$exe,$arguments,$timeout=240,$cwd="$root\Source"){
 $watch=[Diagnostics.Stopwatch]::StartNew();$stdout="$receipt\m77-$label.stdout.log";$stderr="$receipt\m77-$label.stderr.log";$opts=@{FilePath=$exe;WorkingDirectory=$cwd;PassThru=$true;RedirectStandardOutput=$stdout;RedirectStandardError=$stderr};if($arguments.Count){$opts.ArgumentList=$arguments};$p=Start-Process @opts;[void]$p.Handle
 if(!$p.WaitForExit($timeout*1000)){Stop-Process -Id $p.Id -Force;$code='timeout'}else{$p.Refresh();$code=$p.ExitCode;if($null -eq $code){throw 'missing exit status'}}
 Upload $stdout;Upload $stderr;$r=@{label=$label;exitcode=$code;seconds=$watch.Elapsed.TotalSeconds;command=$exe;arguments=$arguments;cwd=$cwd};$script:rows+=$r;$r|ConvertTo-Json -Depth 4|Set-Content "$receipt\m77-$label.result.json" -Encoding UTF8;Upload "$receipt\m77-$label.result.json";return $code
}
try{
 New-Item -ItemType Directory $staged -Force|Out-Null
 $names=@('judas_profiler_tests.exe','judas.exe','judas_editor.exe','judas_export.exe','judas_physical_balance_foundation_tests.exe','judas_physical_balance_controller_tests.exe','judas_balance_lab_tests.exe','judas_physical_animation_tests.exe','judas_character_motor_tests.exe')
 foreach($n in $names){Copy-Item "$bin\$n" "$staged\$n" -Force}
 $dlls=Get-Content "$bin\windows-runtime\required-dlls.txt";foreach($n in $dlls){Copy-Item "$bin\windows-runtime\$n" "$staged\$n" -Force}
 # Mesa is ONLY in disposable test copy; never copy it into build/sdk/pristine export.
 foreach($n in @('opengl32.dll','libgallium_wgl.dll')){New-Item -ItemType HardLink -Path "$staged\$n" -Target "$root\Mesa\x64\$n" -Force|Out-Null}
 New-Item -ItemType Directory "$staged\assets" -Force|Out-Null;Copy-Item "$root\Source\assets\fonts" "$staged\assets\fonts" -Recurse -Force
 $env:GALLIUM_DRIVER='llvmpipe';Set-Location "$root\Source"
 foreach($target in @('profiler','physical_balance_foundation','physical_balance_controller','physical_animation','character_motor')){Run "core-$target" "$staged\judas_${target}_tests.exe" @()|Out-Null}
 $outputs="$root\M77Results";New-Item -ItemType Directory $outputs -Force|Out-Null
 foreach($mode in @('standing','denied','supports','variants','recovery','blocked-recovery','low-grip','radial','lifecycle','regions','fault','cookbook')){Run "lab-$mode" "$staged\judas_balance_lab_tests.exe" @("$outputs\$mode",$mode)|Out-Null}
 foreach($task in @('','stand-','recovery-')){foreach($phase in @('write','read')){Run "cold-$task$phase" "$staged\judas_balance_lab_tests.exe" @("$outputs\cold-$task","cold-$task$phase")|Out-Null}}
 Invoke-WebRequest -UseBasicParsing "$base/m77-services.harness" -OutFile "$root\m77-services.harness"
 $env:JUDAS_TEST_SCRIPT="$root\m77-services.harness"
 Run 'application-services' "$staged\judas.exe" @("$root\Source\projects\balance_lab\balance_lab.judasproj")|Out-Null
 Remove-Item Env:\JUDAS_TEST_SCRIPT
 $editorProject="$root\M77EditorProject";Copy-Item "$root\Source\projects\balance_lab" $editorProject -Recurse
 $env:JUDAS_EDITOR_AUTOTEST="$receipt\m77-editor";$env:JUDAS_EDITOR_AUTOTEST_AUTHORING='1'
 Run 'editor-play-stop' "$staged\judas_editor.exe" @("$editorProject\balance_lab.judasproj")|Out-Null
 Remove-Item Env:\JUDAS_EDITOR_AUTOTEST,Env:\JUDAS_EDITOR_AUTOTEST_AUTHORING
 foreach($p in Get-ChildItem "$receipt\m77-editor*.png"){Upload $p.FullName}
 $pristine="$root\Packages\M77Pristine";Run 'export' "$staged\judas_export.exe" @("$root\Source\projects\balance_lab\balance_lab.judasproj",$pristine,"$bin\judas.exe")|Out-Null
 if($script:rows[-1].exitcode -ne 0){throw 'export failed'}
 if((Test-Path "$pristine\opengl32.dll") -or (Test-Path "$pristine\libgallium_wgl.dll")){throw 'Mesa leaked into pristine export'}
 $packageHashes=Get-ChildItem $pristine -File -Recurse|ForEach-Object{@{path=$_.FullName.Substring($pristine.Length+1);sha256=(Get-FileHash $_.FullName).Hash.ToLower();bytes=$_.Length}};$packageHashes|ConvertTo-Json -Depth 4|Set-Content "$receipt\m77-pristine-package-files.json" -Encoding UTF8;Upload "$receipt\m77-pristine-package-files.json"
 $moved="$root\Packages\Moved M77 Balance $([char]0x00e9) $([char]0x03b1)";Move-Item $pristine $moved
 foreach($n in @('opengl32.dll','libgallium_wgl.dll')){New-Item -ItemType HardLink -Path "$moved\$n" -Target "$root\Mesa\x64\$n" -Force|Out-Null}
 $unrelated="$root\M77UnrelatedCwd";New-Item -ItemType Directory $unrelated -Force|Out-Null;Set-Location $unrelated
 # Only owned validation source copy temporarily hidden, restored in finally; no build can be running.
 Rename-Item "$root\Source" 'Source-M77-hidden'
 try{$env:JUDAS_TEST_SCRIPT="$root\m77-services.harness";Run 'moved-source-hidden' "$moved\judas.exe" @() 240 $unrelated|Out-Null}finally{Remove-Item Env:\JUDAS_TEST_SCRIPT -ErrorAction SilentlyContinue;Rename-Item "$root\Source-M77-hidden" 'Source'}
 $hashes=$names|ForEach-Object{@{path=$_;sha256=(Get-FileHash "$staged\$_").Hash.ToLower()}};$hashes|ConvertTo-Json -Depth 4|Set-Content "$receipt\m77-windows-binaries.json" -Encoding UTF8;Upload "$receipt\m77-windows-binaries.json"
 @{rows=$script:rows;failed=@($script:rows|Where-Object {$_.exitcode -ne 0}).Count;session=(Get-Process -Id $PID).SessionId;admin=([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator);package=$moved;vmOnlyMesa=@('opengl32.dll','libgallium_wgl.dll');freeBytes=(Get-PSDrive C).Free}|ConvertTo-Json -Depth 8|Set-Content "$receipt\m77-windows-final-summary.json" -Encoding UTF8;Upload "$receipt\m77-windows-final-summary.json"
}catch{$_|Out-String|Set-Content "$receipt\m77-windows-gui-failure.log";Upload "$receipt\m77-windows-gui-failure.log";throw}
