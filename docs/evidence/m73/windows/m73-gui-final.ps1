$root='C:\JudasValidation';$base='http://192.168.122.1:8872';$ProgressPreference='SilentlyContinue';$ErrorActionPreference='Stop'
$receipts="$root\Receipts";New-Item -Force -ItemType Directory $receipts|Out-Null
function Upload($path){Invoke-WebRequest -UseBasicParsing "$base/$([IO.Path]::GetFileName($path))" -Method Post -InFile $path|Out-Null}
function Run($label,$exe,$arguments,$timeout=180,$cwd="$root\Source"){
 $start=Get-Date;$stdout="$receipts\$label.stdout.log";$stderr="$receipts\$label.stderr.log"
 $options=@{FilePath=$exe;WorkingDirectory=$cwd;PassThru=$true;RedirectStandardOutput=$stdout;RedirectStandardError=$stderr}
 if($arguments.Count){$options.ArgumentList=$arguments}
 $p=Start-Process @options
 [void]$p.Handle
 $finished=$p.WaitForExit($timeout*1000)
 if(!$finished){Stop-Process -Id $p.Id -Force; $exit='timeout'}else{$p.Refresh();$exit=$p.ExitCode;if($null -eq $exit){throw "No exit code for $label"}}
 Upload $stdout;Upload $stderr
 $record=@{label=$label;exit=$exit;seconds=((Get-Date)-$start).TotalSeconds;stdout=[IO.File]::ReadAllText($stdout);stderr=[IO.File]::ReadAllText($stderr);session=(Get-Process -Id $PID).SessionId;admin=([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)}
 $path="$receipts\$label.result.json";$record|ConvertTo-Json -Depth 4|Set-Content -Encoding UTF8 $path;Upload $path
 return $record
}


$sdk="$root\Build\windows-sdk";$bin="$root\Build\Release";$env:PATH="$sdk;$env:PATH";$env:GALLIUM_DRIVER='llvmpipe'
# Software OpenGL is VM-only; it is not added to normal Judas packages.
foreach($dest in @($sdk,$bin)){Copy-Item "$root\Mesa\x64\opengl32.dll","$root\Mesa\x64\libgallium_wgl.dll" $dest -Force}
function Quote($s){return '"'+$s+'"'}
$project="$root\Source\projects\signals_lab\signals_lab.judasproj"
$r1=Run 'm73-app-save' "$bin\judas_signal_application_tests.exe" @($project) 180
$r2=Run 'm73-app-load' "$bin\judas_signal_application_tests.exe" @($project,'load') 180
$dest="$root\Packages\Signals Lab M73";$moved="$root\Packages\Moved Signals Lab M73 $([char]0x00e9) $([char]0x03b1)"
$ex=Run 'm73-export' "$sdk\judas_export.exe" @((Quote $project),(Quote $dest)) 180
if($ex.exit -ne 0){throw 'M73 export failed'}
if(Test-Path $moved){Move-Item $moved ($moved+' before final rebuild')}
Move-Item $dest $moved
Copy-Item "$root\Mesa\x64\opengl32.dll","$root\Mesa\x64\libgallium_wgl.dll" $moved -Force
$harness="$root\m73.harness";Invoke-WebRequest -UseBasicParsing "$base/m73.harness" -OutFile $harness
$unrelated="$root\Unrelated working directory";New-Item -Force -ItemType Directory $unrelated|Out-Null
$env:JUDAS_TEST_SCRIPT=$harness
$run=Run 'm73-moved-runtime' "$moved\judas.exe" @() 180 $unrelated
Remove-Item Env:\JUDAS_TEST_SCRIPT
$env:JUDAS_EDITOR_AUTOTEST="$receipts\m73-editor";$env:JUDAS_EDITOR_AUTOTEST_AUTHORING='1'
$editor=Run 'm73-editor-play-stop' "$sdk\judas_editor.exe" @($project) 180
Remove-Item Env:\JUDAS_EDITOR_AUTOTEST,Env:\JUDAS_EDITOR_AUTOTEST_AUTHORING
@{save=$r1.exit;load=$r2.exit;export=$ex.exit;moved=$run.exit;editor=$editor.exit;package=$moved;vmOnlyExtraDlls=@('opengl32.dll','libgallium_wgl.dll');finished=(Get-Date).ToString('o')}|ConvertTo-Json -Depth 4|Set-Content "$receipts\m73-windows-summary.json" -Encoding UTF8;Upload "$receipts\m73-windows-summary.json"
