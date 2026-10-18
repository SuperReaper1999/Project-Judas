$ErrorActionPreference='Stop';$ProgressPreference='SilentlyContinue'
$task='P:\JudasValidation\PreAlphaPass4';$sdk="$task\Build\windows-sdk";$python='C:\JudasValidation\Python\python.exe';$results="$task\InspectorFollowup"
New-Item -ItemType Directory -Force $results|Out-Null
$env:TMP="$task\Temp";$env:TEMP=$env:TMP
Remove-Item Env:PYTHONDONTWRITEBYTECODE,Env:PYTHONPYCACHEPREFIX -ErrorAction SilentlyContinue
$rows=New-Object 'System.Collections.Generic.List[object]'
function Run-Native($label,$exe,$arguments,$expected=0,$timeout=60,$cwd=$task){
 $stdout="$results\$label.stdout.log";$stderr="$results\$label.stderr.log";$options=@{FilePath=$exe;WorkingDirectory=$cwd;PassThru=$true;RedirectStandardOutput=$stdout;RedirectStandardError=$stderr}
 if($arguments.Count){$options.ArgumentList=@($arguments|ForEach-Object {'"'+([string]$_)+'"'})}
 $started=[DateTime]::UtcNow;$p=Start-Process @options;[void]$p.Handle;$finished=$p.WaitForExit($timeout*1000)
 if(!$finished){Stop-Process -Id $p.Id -Force;$exit='timeout'}else{$p.Refresh();$exit=$p.ExitCode;if($null-eq $exit){throw "No native exit code: $label"}}
 $row=[pscustomobject]@{label=$label;executable=$exe;arguments=@($arguments);cwd=$cwd;exit=$exit;expected=$expected;passed=($exit-eq $expected);seconds=([DateTime]::UtcNow-$started).TotalSeconds;stdout=[IO.File]::ReadAllText($stdout);stderr=[IO.File]::ReadAllText($stderr)}
 $rows.Add($row);$row|ConvertTo-Json -Depth 5|Set-Content -Encoding UTF8 "$results\$label.json"
}

$manifestBefore=(Get-FileHash "$sdk\SDK_MANIFEST.json").Hash.ToLowerInvariant()
Run-Native 'verify-before' $python @("$sdk\tools\package_sdk.py",'verify',$sdk)
Run-Native 'plain-native-inspector' $python @("$sdk\tools\inspect_sdk.py",$sdk,'--host-check','--system-directory','C:\Windows\System32','--output',"$results\native-pe.detail.json")
Run-Native 'verify-after-inspector' $python @("$sdk\tools\package_sdk.py",'verify',$sdk)
Run-Native 'plain-apk-verifier-missing-negative' $python @("$sdk\tools\verify_android.py","$task\absent.apk",'--output',"$results\absent-apk.detail.json") 1
Run-Native 'verify-after-apk-entry' $python @("$sdk\tools\package_sdk.py",'verify',$sdk)
$negative="$sdk\unlisted-task-negative.txt";[IO.File]::WriteAllText($negative,'Strict immutable SDK negative control')
$negativeHash=(Get-FileHash $negative).Hash.ToLowerInvariant()
Run-Native 'strict-unexpected-file-negative' $python @("$sdk\tools\package_sdk.py",'verify',$sdk) 1
if((Get-FileHash $negative).Hash.ToLowerInvariant()-ne $negativeHash){throw 'Negative-control bytes unexpectedly changed'}
Remove-Item -LiteralPath $negative
Run-Native 'verify-after-removing-negative' $python @("$sdk\tools\package_sdk.py",'verify',$sdk)
$noCache=!(Test-Path "$sdk\tools\__pycache__");$manifestAfter=(Get-FileHash "$sdk\SDK_MANIFEST.json").Hash.ToLowerInvariant()
[pscustomobject]@{schema=1;utc=[DateTime]::UtcNow.ToString('o');nativeSourceRevision='9deb47d1667e5b6bd1abe708ed343c3fbf6fe847';toolingSourceRevision='df8929f73505d407697b5d35b115fc71d9100471';rows=$rows.ToArray();noBytecodeCacheCreated=$noCache;manifestBefore=$manifestBefore;manifestAfter=$manifestAfter;nativeManifestUnchanged=($manifestBefore-eq $manifestAfter);noPythonEnvironmentMask=$true;noBFlag=$true;passed=(@($rows|Where-Object {!$_.passed}).Count-eq 0-and $noCache-and $manifestBefore-eq $manifestAfter)}|ConvertTo-Json -Depth 7|Set-Content -Encoding UTF8 "$task\INSPECTOR_FOLLOWUP_RESULTS.json"
