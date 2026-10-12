param([string]$ManifestName='',[string]$CaseId='',[string]$RunId='',[string[]]$Arguments=@(),[int]$TimeoutMilliseconds=600000)
$ErrorActionPreference='Stop';$ProgressPreference='SilentlyContinue';$root='C:\JudasValidation';$base='http://192.168.122.1:8872';$out="$root\M78ContinuationResults"
function Upload($p,$name){Invoke-WebRequest -UseBasicParsing "$base/$name" -Method Post -InFile $p|Out-Null}
try {
 if($ManifestName -notmatch '^m78-continuation-direct-[a-z0-9_-]+\.json$' -or $CaseId -notmatch '^[a-z0-9_-]+$' -or $RunId -notmatch '^[a-z0-9_-]+$'){throw 'Explicit manifest, compiled case and unique run required'}
 $m=(Invoke-WebRequest -UseBasicParsing "$base/$ManifestName").Content|ConvertFrom-Json;$name="m78-$($m.label)-run-$RunId";$receipt="$out\$name.json"
 if(Test-Path $receipt){throw 'Preserve preceding native run receipt'}
 $compiled=Get-Content "$out\m78-$($m.label)-$CaseId.json" -Raw|ConvertFrom-Json;$library=Get-Content "$out\m78-$($m.label)-library.json" -Raw|ConvertFrom-Json
 if($compiled.archiveSHA256 -ne $library.archiveSHA256 -or (Get-FileHash $compiled.executable).Hash.ToLower() -ne $compiled.executableSHA256){throw 'Final executable/library identity mismatch'}
 $archive="$root\M78ContinuationBuild\$($m.label)\judas_engine.lib";if((Get-FileHash $archive).Hash.ToLower() -ne $compiled.archiveSHA256){throw 'Candidate archive changed'}
 foreach($sourceRow in $compiled.compiles){if((Get-FileHash -LiteralPath $sourceRow.source).Hash.ToLower() -ne $sourceRow.sourceSHA256){throw 'Compiled source changed after freeze'}}
 $env:TEMP="$out\temp";$env:TMP=$env:TEMP;New-Item -ItemType Directory $env:TEMP -Force|Out-Null
 $options=@{FilePath=$compiled.executable;WorkingDirectory="$root\Source";PassThru=$true;RedirectStandardOutput="$out\$name.stdout.log";RedirectStandardError="$out\$name.stderr.log"};if($Arguments.Count){$options.ArgumentList=$Arguments}
 $watch=[Diagnostics.Stopwatch]::StartNew();$process=Start-Process @options;[void]$process.Handle;if(!$process.WaitForExit($TimeoutMilliseconds)){Stop-Process -Id $process.Id -Force;$exit='owned case timed out'}else{$process.Refresh();$exit=$process.ExitCode}
 @{scope='Fresh native execution of a verified final candidate executable; no recompile';case=$CaseId;run=$RunId;label=$m.label;exitCode=$exit;seconds=$watch.Elapsed.TotalSeconds;archiveSHA256=$compiled.archiveSHA256;executable=$compiled.executable;executableSHA256=$compiled.executableSHA256;arguments=$Arguments;freeBytes=(Get-PSDrive C).Free;environment=@{GALLIUM_DRIVER=$env:GALLIUM_DRIVER;JUDAS_ENGINE_ROOT=$env:JUDAS_ENGINE_ROOT;JUDAS_TEST_SCRIPT=$env:JUDAS_TEST_SCRIPT;TEMP=$env:TEMP}}|ConvertTo-Json -Depth 5|Set-Content $receipt -Encoding UTF8
 Upload "$out\$name.stdout.log" "$name.stdout.log";Upload "$out\$name.stderr.log" "$name.stderr.log";Upload $receipt "$name.json"
}catch{$name="m78-direct-run-$RunId-error.txt";[string]$_.Exception.Message|Set-Content "$out\$name";Upload "$out\$name" $name;exit 1}
