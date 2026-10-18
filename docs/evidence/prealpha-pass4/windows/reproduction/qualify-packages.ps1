$ErrorActionPreference='Stop';$ProgressPreference='SilentlyContinue'
$task='P:\JudasValidation\PreAlphaPass4';$sdk="$task\Build\windows-sdk";$python='C:\JudasValidation\Python\python.exe'
$env:TEMP="$task\Temp";$env:TMP=$env:TEMP;$env:PATH='C:\Windows\System32;C:\Windows'
$results="$task\PackageResults";New-Item -ItemType Directory -Force $results|Out-Null
$rows=New-Object 'System.Collections.Generic.List[object]'
function Run-Native($label,$exe,$arguments,$expected=0,$timeout=60,$cwd=$task){
 $stdout="$results\$label.stdout.log";$stderr="$results\$label.stderr.log";$options=@{FilePath=$exe;WorkingDirectory=$cwd;PassThru=$true;RedirectStandardOutput=$stdout;RedirectStandardError=$stderr}
 if($arguments.Count){$options.ArgumentList=@($arguments|ForEach-Object {'"'+([string]$_)+'"'})}
 $started=[DateTime]::UtcNow;$p=Start-Process @options;[void]$p.Handle;$finished=$p.WaitForExit($timeout*1000)
 if(!$finished){Stop-Process -Id $p.Id -Force;$exit='timeout'}else{$p.Refresh();$exit=$p.ExitCode;if($null-eq $exit){throw "No native exit code: $label"}}
 $row=[pscustomobject]@{label=$label;executable=$exe;arguments=@($arguments);cwd=$cwd;exit=$exit;expected=$expected;passed=($exit-eq $expected);seconds=([DateTime]::UtcNow-$started).TotalSeconds;stdout=[IO.File]::ReadAllText($stdout);stderr=[IO.File]::ReadAllText($stderr)}
 $rows.Add($row);$row|ConvertTo-Json -Depth 5|Set-Content -Encoding UTF8 "$results\$label.json"
}

$relocated="$task\Relocated SDK $([char]0xe9) $([char]0x3b1)"
Copy-Item $sdk $relocated -Recurse
Run-Native 'moved-sdk-verify' $python @("$relocated\tools\package_sdk.py",'verify',$relocated)
Run-Native 'moved-editor-identity' "$relocated\judas_editor.exe" @('--build-info-json')
$project="$task\Projects\projects\physics_stress_lab\physics_stress_lab.judasproj"
Run-Native 'ordinary-consumer-export' "$relocated\judas_export.exe" @($project,"$task\Export Original") 0 180
$moved="$task\Moved Game $([char]0xe9) $([char]0x3b1)"
Move-Item "$task\Export Original" $moved
Run-Native 'moved-standalone-identity' "$moved\judas.exe" @('--build-info-json')
$negative="$task\NegativeMissingRuntimeSDK";Copy-Item $sdk $negative -Recurse
$dll=Get-ChildItem "$negative\windows-runtime" -Filter 'icu*.dll'|Select-Object -First 1
if(!$dll){throw 'Native SDK ICU manifest fixture missing'}
$removed=[pscustomobject]@{path=$dll.FullName;sha256=(Get-FileHash $dll.FullName).Hash.ToLowerInvariant()};Remove-Item -LiteralPath $dll.FullName
Run-Native 'missing-runtime-export-negative' "$negative\judas_export.exe" @($project,"$task\MustNotPublish") 1 90
if(Test-Path "$task\MustNotPublish"){throw 'Failed export published a package'}
$tamper="$task\TamperedSDK";Copy-Item $sdk $tamper -Recurse
Add-Content -LiteralPath "$tamper\README.md" -Encoding UTF8 -Value 'Task-only corruption negative control'
Run-Native 'sdk-tamper-negative' $python @("$tamper\tools\package_sdk.py",'verify',$tamper) 1 90
$files=Get-ChildItem -LiteralPath $moved -Recurse -File|ForEach-Object {[pscustomobject]@{path=$_.FullName.Substring($moved.Length+1).Replace('\','/');bytes=$_.Length;sha256=(Get-FileHash -LiteralPath $_.FullName).Hash.ToLowerInvariant()}}
$files|ConvertTo-Json -Depth 3|Set-Content -Encoding UTF8 "$task\PRISTINE_PACKAGE_FILES.json"
$allPassed=@($rows|Where-Object {!$_.passed}).Count-eq 0
[pscustomobject]@{schema=1;utc=[DateTime]::UtcNow.ToString('o');rows=$rows.ToArray();passed=$allPassed;missingDll=$removed;failedExportNotPublished=!(Test-Path "$task\MustNotPublish");movedPackage=$moved;packageFiles=$files.Count;stockPackageContainsMesa=@($files|Where-Object {$_.path-match '(?i)opengl32.dll|libgallium_wgl.dll'}).Count-gt 0;scope='Genuine native all-tool SDK, relocated Unicode/spaces SDK and ordinary export, checksums and missing-payload negative controls. Application graphical execution separately qualified using explicit isolated software overlay.'}|ConvertTo-Json -Depth 7|Set-Content -Encoding UTF8 "$task\PACKAGING_RESULTS.json"
if(!$allPassed){exit 1}
