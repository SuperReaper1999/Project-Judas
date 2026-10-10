# Native MSVC incremental M75 checks. No VM/toolchain/configuration changes.
$ErrorActionPreference='Stop';$ProgressPreference='SilentlyContinue'
$root='C:\JudasValidation';$base='http://192.168.122.1:8872'
$source="$root\Source";$build="$root\Build";$receipts="$root\Receipts\M75"
# Initial transferred-source receipt precedes all candidate runtime executables.
$minimumCandidateStart=(Get-Item "$receipts\m75-windows-source-proof.json").LastWriteTime
New-Item -Force -ItemType Directory $receipts|Out-Null
function Upload($path){Invoke-WebRequest -UseBasicParsing "$base/$([IO.Path]::GetFileName($path))" -Method Post -InFile $path|Out-Null}
function Report($name,$data){$path="$receipts\$name";$data|ConvertTo-Json -Depth 12|Set-Content $path -Encoding UTF8;Upload $path}
function Quote($text){return '"'+$text+'"'}
function Run($label,$exe,$arguments,$timeout=300,$cwd=$source){
 $start=Get-Date;$stdout="$receipts\$label.stdout.log";$stderr="$receipts\$label.stderr.log"
 $options=@{FilePath=$exe;WorkingDirectory=$cwd;PassThru=$true;RedirectStandardOutput=$stdout;RedirectStandardError=$stderr};if($arguments.Count){$options.ArgumentList=$arguments}
 $p=Start-Process @options;[void]$p.Handle;if(!$p.WaitForExit($timeout*1000)){Stop-Process -Id $p.Id -Force;$code='timeout'}else{$p.Refresh();$code=$p.ExitCode;if($null -eq $code){throw "No exit code $label"}}
 Upload $stdout;Upload $stderr;Report "$label.result.json" @{exit=$code;seconds=((Get-Date)-$start).TotalSeconds;exe=$exe;sha256=(Get-FileHash $exe).Hash.ToLower()}
 if($code -ne 0){throw "$label failed: $code"}
}
try{
 Invoke-WebRequest -UseBasicParsing "$base/m75-source.zip" -OutFile "$root\m75-source.zip"
 Invoke-WebRequest -UseBasicParsing "$base/m75-source.json" -OutFile "$root\m75-source.json"
 $expected=Get-Content "$root\m75-source.json" -Raw|ConvertFrom-Json
 if((Get-FileHash "$root\m75-source.zip").Hash.ToLower() -ne $expected.archiveSha256){throw 'Transfer archive hash mismatch'}
 Expand-Archive -Force "$root\m75-source.zip" $source
 $proof=@();foreach($entry in $expected.files.PSObject.Properties){$actual=(Get-FileHash (Join-Path $source $entry.Name)).Hash.ToLower();$proof+=@{path=$entry.Name;expected=$entry.Value;actual=$actual;matches=($actual -eq $entry.Value)}}
 Report 'm75-windows-source-proof.json' @{startingHead=$expected.startingHead;files=$proof}
 if(@($proof|Where-Object {!$_.matches}).Count){throw 'Candidate source mismatch; do not regenerate expected hashes'}
 foreach($entry in $expected.changed){if($entry -match '\.(cpp|h|cmake)$' -or $entry -eq 'CMakeLists.txt'){(Get-Item (Join-Path $source $entry)).LastWriteTime=Get-Date}}
 Import-Module "$root\VS2022\Common7\Tools\Microsoft.VisualStudio.DevShell.dll"
 Enter-VsDevShell -VsInstallPath "$root\VS2022" -SkipAutomaticLocation -DevCmdArguments '-arch=x64 -host_arch=x64'
 $env:PATH="$root\Git\cmd;$root\VS2022\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin;$env:PATH"
 $started=Get-Date;$ErrorActionPreference='Continue'
 & cmake -S $source -B $build 2>&1|Tee-Object "$receipts\m75-windows-configure.log";$code=$LASTEXITCODE;Upload "$receipts\m75-windows-configure.log";if($code){throw "Configure $code"}
 & cmake --build $build --config Release --target judas_windows_sdk judas_terrain_authoring_tests judas_terrain_pipeline_tests judas_terrain_application_tests --parallel 2 2>&1|Tee-Object "$receipts\m75-windows-build.log";$code=$LASTEXITCODE;$ErrorActionPreference='Stop';Upload "$receipts\m75-windows-build.log"
 Report 'm75-windows-build-result.json' @{exit=$code;started=$started.ToString('o');finished=(Get-Date).ToString('o');jobs=2}
 if($code){throw "Native build $code"}
 $fresh=@();foreach($name in @('judas.exe','judas_editor.exe','judas_terrain_author.exe','judas_export.exe','judas_terrain_authoring_tests.exe','judas_terrain_pipeline_tests.exe','judas_terrain_application_tests.exe')){$p="$build\Release\$name";$f=Get-Item $p;$fresh+=@{name=$name;sha256=(Get-FileHash $p).Hash.ToLower();lastWriteUtc=$f.LastWriteTimeUtc.ToString('o');fresh=($f.LastWriteTime -ge $minimumCandidateStart)}}
 Report 'm75-windows-executable-proof.json' $fresh;if(@($fresh|Where-Object {!$_.fresh}).Count){throw 'Stale candidate binary'}
 $run="C:\M75 $([char]0xe9) $([char]0x3b1)";New-Item -Force -ItemType Directory $run|Out-Null
 Run 'm75-windows-authoring' "$build\Release\judas_terrain_authoring_tests.exe" @((Quote "$run\Unit"))
 Run 'm75-windows-pipeline' "$build\Release\judas_terrain_pipeline_tests.exe" @((Quote "$run\Pipeline"))
 $project="$source\projects\terrain_lab\terrain_lab.judasproj"
 Run 'm75-windows-cli-cook' "$build\windows-sdk\judas_terrain_author.exe" @('cook',(Quote $project),'Imports/landscape.judasterrain')
 Report 'm75-windows-native-summary.json' @{build=0;authoring=0;pipeline=0;cook=0;run=$run;hardwareValidated=$false}
 exit 0
}catch{$ErrorActionPreference='Continue';Report 'm75-windows-build-failure.json' @{message=$_.Exception.Message};Write-Error $_;exit 1}
