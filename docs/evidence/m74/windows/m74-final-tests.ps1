# M74 native Windows candidate verification. Execute only after Linux jobs finish.
# No source hashes are generated here: the host supplies the final reviewed bytes.
$ErrorActionPreference='Stop';$ProgressPreference='SilentlyContinue'
$root='C:\JudasValidation';$base='http://192.168.122.1:8872'
$source="$root\Source";$build="$root\Build";$receipts="$root\Receipts\M74"
New-Item -Force -ItemType Directory $receipts|Out-Null
function Upload($path){Invoke-WebRequest -UseBasicParsing "$base/$([IO.Path]::GetFileName($path))" -Method Post -InFile $path|Out-Null}
function JsonReport($name,$data){$path="$receipts\$name";$data|ConvertTo-Json -Depth 12|Set-Content $path -Encoding UTF8;Upload $path}
function Run($label,$exe,$arguments,$timeout=300,$cwd=$source){
 $start=Get-Date;$stdout="$receipts\$label.stdout.log";$stderr="$receipts\$label.stderr.log"
 $options=@{FilePath=$exe;WorkingDirectory=$cwd;PassThru=$true;RedirectStandardOutput=$stdout;RedirectStandardError=$stderr}
 if($arguments.Count){$options.ArgumentList=$arguments}
 $process=Start-Process @options;[void]$process.Handle
 if(!$process.WaitForExit($timeout*1000)){Stop-Process -Id $process.Id -Force;$code='timeout'}else{$process.Refresh();$code=$process.ExitCode;if($null -eq $code){throw "No exit code: $label"}}
 Upload $stdout;Upload $stderr
 $record=@{label=$label;exit=$code;seconds=((Get-Date)-$start).TotalSeconds;exe=$exe;exeSha256=(Get-FileHash $exe).Hash.ToLower();lastWriteUtc=(Get-Item $exe).LastWriteTimeUtc.ToString('o');session=(Get-Process -Id $PID).SessionId}
 JsonReport "$label.result.json" $record
 if($code -ne 0){throw "Native check $label failed: $code"};return $record
}
function Quote($text){return '"'+$text+'"'}

try {
 Invoke-WebRequest -UseBasicParsing "$base/m74-test-warning-patch.zip" -OutFile "$root\m74-test-warning-patch.zip"
 Invoke-WebRequest -UseBasicParsing "$base/m74-test-warning-patch.sha256" -OutFile "$root\m74-test-warning-patch.sha256"
 if((Get-FileHash "$root\m74-test-warning-patch.zip").Hash.ToLower() -ne (Get-Content "$root\m74-test-warning-patch.sha256" -Raw)){throw 'Patch archive mismatch'}
 Expand-Archive -Force "$root\m74-test-warning-patch.zip" $source
 Invoke-WebRequest -UseBasicParsing "$base/m74-final-source.json" -OutFile "$root\m74-final-source.json"
 $expected=Get-Content "$root\m74-final-source.json" -Raw|ConvertFrom-Json
 $proof=@();foreach($entry in $expected.files.PSObject.Properties){$actual=(Get-FileHash (Join-Path $source $entry.Name)).Hash.ToLower();$proof+=@{path=$entry.Name;expected=$entry.Value;actual=$actual;matches=($actual -eq $entry.Value)}}
 JsonReport 'm74-windows-final-source-proof.json' @{baseRevision=$expected.baseRevision;sourceRevision=$expected.sourceRevision;files=$proof}
 if(@($proof|Where-Object {!$_.matches}).Count){throw 'Actual final candidate source mismatch'}
 foreach($p in @('tests/AnimationRetargetTests.cpp','tests/RetargetApplicationTests.cpp')){(Get-Item (Join-Path $source $p)).LastWriteTime=Get-Date}
 Import-Module "$root\VS2022\Common7\Tools\Microsoft.VisualStudio.DevShell.dll"
 Enter-VsDevShell -VsInstallPath "$root\VS2022" -SkipAutomaticLocation -DevCmdArguments '-arch=x64 -host_arch=x64'
 $env:PATH="$root\VS2022\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin;$build\windows-sdk;$env:PATH"
 $started=Get-Date;$ErrorActionPreference='Continue'
 & cmake --build $build --config Release --target judas_animation_retarget_tests judas_retarget_application_tests --parallel 2 2>&1|Tee-Object "$receipts\m74-windows-test-warning-build.log"
 $code=$LASTEXITCODE;$ErrorActionPreference='Stop';Upload "$receipts\m74-windows-test-warning-build.log"
 if($code -ne 0){throw 'Narrow test-only rebuild failed'}
 $fresh=@();foreach($n in @('judas_animation_retarget_tests.exe','judas_retarget_application_tests.exe')){$p="$build\Release\$n";$i=Get-Item $p;$fresh+=@{name=$n;sha256=(Get-FileHash $p).Hash.ToLower();lastWriteUtc=$i.LastWriteTimeUtc.ToString('o');builtAfterStart=($i.LastWriteTime -ge $started)}}
 JsonReport 'm74-windows-final-test-executable-proof.json' $fresh
 if(@($fresh|Where-Object {!$_.builtAfterStart}).Count){throw 'Narrow rebuild executable not fresh'}
 $run='C:\M74FinalCore-'+(Get-Date -Format 'yyyyMMdd-HHmmss')
 $core=Run 'm74-windows-core-final' "$build\Release\judas_animation_retarget_tests.exe" @((Quote $run)) 300
 $cook=Get-Content "$receipts\m74-windows-cook-corrected-summary.json" -Raw|ConvertFrom-Json
 $remaining=Get-Content "$receipts\m74-windows-remaining-summary.json" -Raw|ConvertFrom-Json
 JsonReport 'm74-windows-native-summary.json' @{core=$core.exit;cook=$cook.cook;lab=$remaining.lab;editorDraft=$remaining.editorDraft;profile=$remaining.profile;sourceFiles=$proof.Count;buildExit=$code;testOnlyWarningCleanup=$true;gpuHardwareValidated=$false;finished=(Get-Date).ToString('o')}
 exit 0
}catch{$ErrorActionPreference='Continue';JsonReport 'm74-windows-final-test-failure.json' @{message=$_.Exception.Message};exit 1}
