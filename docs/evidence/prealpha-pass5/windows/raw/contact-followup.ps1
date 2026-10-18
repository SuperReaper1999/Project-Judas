$ErrorActionPreference='Stop';$ProgressPreference='SilentlyContinue';$task='P:\JudasValidation\PreAlphaPass5';$work="$task\GuiWork";$results="$work\ContactFollowupResults";$fixture="$work\Projects\docs\evidence\ftft4\geometry\fixtures.txt"
if(Test-Path -LiteralPath $results){throw 'Preserve existing follow-up attempt'}
if(Test-Path -LiteralPath $fixture){throw 'Do not replace existing fixture'}
if((Get-FileHash -LiteralPath "$task\fixtures.txt").Hash.ToLowerInvariant()-ne 'dcbcb9bf038f11662a2e6c388cc21cc4bdcbfe6807c54a282bb6dea87d7b57ce'){throw 'Transferred original fixture hash mismatch'}
New-Item -ItemType Directory -Force $results,(Split-Path $fixture)|Out-Null
Copy-Item -LiteralPath "$task\fixtures.txt" $fixture
$env:PATH='C:\Windows\System32;C:\Windows';$env:TEMP="$work\Temp";$env:TMP=$env:TEMP
$exe="$work\NativeTests\judas_contact_cache_tests.exe";$stdout="$results\stdout.log";$stderr="$results\stderr.log";$start=[DateTime]::UtcNow
$process=Start-Process -FilePath $exe -WorkingDirectory "$work\Projects" -PassThru -RedirectStandardOutput $stdout -RedirectStandardError $stderr
[void]$process.Handle;$done=$process.WaitForExit(60000)
if(!$done){Stop-Process -Id $process.Id -Force;$code='timeout'}else{$process.Refresh();$code=$process.ExitCode}
$text=[IO.File]::ReadAllText($stdout);$err=[IO.File]::ReadAllText($stderr)
[pscustomobject]@{schema=1;utc=[DateTime]::UtcNow.ToString('o');label='contact-cache-original-fixture-followup';executable=$exe;cwd="$work\Projects";exit=$code;seconds=([DateTime]::UtcNow-$start).TotalSeconds;stdout=$text;stderr=$err;passed=($code-eq 0 -and $text.Contains('fixture_rows=823 primitive_pairs=887'));fixtureSHA256=(Get-FileHash -LiteralPath $fixture).Hash.ToLowerInvariant();setupCorrection='Only exact original FTFT fixture supplied at original relative location in task-owned cwd; no source, test, threshold or protected research edit';originalSetupFailurePreserved=$true;desktopIdentity=[Security.Principal.WindowsIdentity]::GetCurrent().Name;sessionId=[Diagnostics.Process]::GetCurrentProcess().SessionId}|ConvertTo-Json -Depth 5|Set-Content -Encoding UTF8 "$task\CONTACT_FOLLOWUP_RESULTS.json"
exit $code
