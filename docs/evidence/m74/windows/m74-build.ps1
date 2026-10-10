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
 Invoke-WebRequest -UseBasicParsing "$base/m74-final-delta.zip" -OutFile "$root\m74-final-delta.zip"
 Invoke-WebRequest -UseBasicParsing "$base/m74-final-source.json" -OutFile "$root\m74-final-source.json"
 $expected=Get-Content "$root\m74-final-source.json" -Raw|ConvertFrom-Json
 if($expected.archiveSha256 -and (Get-FileHash "$root\m74-final-delta.zip").Hash.ToLower() -ne $expected.archiveSha256){throw 'M74 transfer archive fingerprint mismatch'}
 Expand-Archive -Force "$root\m74-final-delta.zip" $source
 $proof=@();foreach($entry in $expected.files.PSObject.Properties){$path=Join-Path $source $entry.Name;$actual=(Get-FileHash $path -Algorithm SHA256).Hash.ToLower();$proof+=@{path=$entry.Name;expected=$entry.Value;actual=$actual;matches=($actual -eq $entry.Value)}}
 JsonReport 'm74-windows-source-proof.json' @{baseRevision=$expected.baseRevision;sourceRevision=$expected.sourceRevision;files=$proof}
 if(@($proof|Where-Object {!$_.matches}).Count){throw 'M74 final candidate source fingerprint mismatch; do not rebuild or regenerate expected hashes'}
 # A ZIP may preserve source mtimes older than an existing native object. Only
 # transferred C++/CMake inputs are touched, after content has matched above.
 $transferred=if($expected.transferred){@($expected.transferred)}else{@($expected.files.PSObject.Properties.Name)}
 foreach($path in $transferred){if($path -match '\.(cpp|h|hpp|inc|cmake)$' -or $path -eq 'CMakeLists.txt'){(Get-Item (Join-Path $source $path)).LastWriteTime=Get-Date}}
 Import-Module "$root\VS2022\Common7\Tools\Microsoft.VisualStudio.DevShell.dll"
 Enter-VsDevShell -VsInstallPath "$root\VS2022" -SkipAutomaticLocation -DevCmdArguments '-arch=x64 -host_arch=x64'
 $env:PATH="$root\Git\cmd;$root\VS2022\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin;$env:PATH"
 $started=Get-Date;$ErrorActionPreference='Continue'
 & cmake -S $source -B $build 2>&1|Tee-Object "$receipts\m74-windows-configure.log";$configure=$LASTEXITCODE;Upload "$receipts\m74-windows-configure.log"
 if($configure -ne 0){throw "M74 native configure failed: $configure"}
 & cmake --build $build --config Release --target judas_windows_sdk judas_model_import_cli judas_animation_retarget_tests judas_animation_retarget_cook_tests judas_retarget_lab_tests judas_retarget_editor_tests judas_retarget_application_tests --parallel 2 2>&1|Tee-Object "$receipts\m74-windows-build.log"
 $code=$LASTEXITCODE;$ErrorActionPreference='Stop';Upload "$receipts\m74-windows-build.log"
 JsonReport 'm74-windows-build-result.json' @{exit=$code;started=$started.ToString('o');finished=(Get-Date).ToString('o');jobs=2;sourceFiles=$proof.Count}
 if($code -ne 0){throw "M74 native build failed: $code"}
 $sdk="$build\windows-sdk";$bin="$build\Release";$env:PATH="$sdk;$env:PATH"
 $fresh=@();foreach($name in @('judas_editor.exe','judas_model_import_cli.exe','judas_animation_retarget_tests.exe','judas_animation_retarget_cook_tests.exe','judas_retarget_lab_tests.exe','judas_retarget_editor_tests.exe','judas_retarget_application_tests.exe')){$path="$bin\$name";$item=Get-Item $path;$fresh+=@{name=$name;sha256=(Get-FileHash $path).Hash.ToLower();lastWriteUtc=$item.LastWriteTimeUtc.ToString('o');builtAfterStart=($item.LastWriteTime -ge $started)}}
 JsonReport 'm74-windows-executable-proof.json' $fresh
 if(@($fresh|Where-Object {!$_.builtAfterStart}).Count){throw 'A required M74 executable is older than this candidate build'}
 $stamp=Get-Date -Format 'yyyyMMdd-HHmmss';$run="$receipts\Run-$stamp";New-Item -Force -ItemType Directory $run|Out-Null
 $project="$source\projects\retarget_lab\retarget_lab.judasproj"
 $core=Run 'm74-windows-core' "$bin\judas_animation_retarget_tests.exe" @((Quote "$run\Core")) 300
 $cook=Run 'm74-windows-cook' "$bin\judas_animation_retarget_cook_tests.exe" @((Quote "$run\Cook")) 300
 Copy-Item "$run\Cook\results.json" "$receipts\m74-windows-cook-checks.json" -Force;Upload "$receipts\m74-windows-cook-checks.json"
 $lab=Run 'm74-windows-lab' "$bin\judas_retarget_lab_tests.exe" @((Quote "$source\projects\retarget_lab"),(Quote "$run\lab-checks.json")) 300
 Copy-Item "$run\lab-checks.json" "$receipts\m74-windows-lab-checks.json" -Force;Upload "$receipts\m74-windows-lab-checks.json"
 $editor=Run 'm74-windows-editor-state' "$bin\judas_retarget_editor_tests.exe" @((Quote "$run\Editor state")) 120
 $validate=Run 'm74-windows-cli-profile' "$bin\judas_model_import_cli.exe" @('--retarget-validate',(Quote "$source\projects\retarget_lab\Sources\source.gltf"),(Quote "$source\projects\retarget_lab\Imports\broad.judasimport"),(Quote "$source\projects\retarget_lab\Imports\broad.judasretarget")) 120
 JsonReport 'm74-windows-native-summary.json' @{core=$core.exit;cook=$cook.exit;lab=$lab.exit;editorDraft=$editor.exit;profile=$validate.exit;project=$project;finished=(Get-Date).ToString('o');gpuHardwareValidated=$false}
 exit 0
}catch {
 $ErrorActionPreference='Continue';JsonReport 'm74-windows-build-failure.json' @{message=$_.Exception.Message;finished=(Get-Date).ToString('o')};Write-Error $_;exit 1
}
