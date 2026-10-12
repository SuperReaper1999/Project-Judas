$ErrorActionPreference='Stop';$ProgressPreference='SilentlyContinue';$root='C:\JudasValidation';$base='http://192.168.122.1:8872';$out="$root\M78ContinuationResults";$owned="$out\combined-v2";$original="$owned\balance-cold-step";$capture="$owned\state-capture";$env:GALLIUM_DRIVER='llvmpipe';$env:JUDAS_ENGINE_ROOT="$root\Source"
function Upload($p,$n){Invoke-WebRequest -UseBasicParsing "$base/$n" -Method Post -InFile $p|Out-Null}
try{
 $compiled=Get-Content "$out\m78-combined-v2-state_observer.json" -Raw|ConvertFrom-Json;if($compiled.exitCode -ne 0){throw 'State capture original acceptance failed'}
 $before=(Get-FileHash "$original\slots\m77.save").Hash.ToLower();$fresh=(Get-FileHash "$capture\slots\m77.save").Hash.ToLower();$copies=@()
 foreach($spec in @(@{name='state-fresh-baseline';source=$capture},@{name='state-original-baseline';source=$original})){
  $dest="$owned\$($spec.name)";if(Test-Path $dest){throw 'Preserve previous diagnostic state run'};Copy-Item -LiteralPath $spec.source -Destination $dest -Recurse
  if($spec.source -eq $original){Copy-Item -LiteralPath "$capture\actual-drive-sidecar.json" -Destination "$dest\actual-drive-sidecar.json"}
  $hash=(Get-FileHash "$dest\slots\m77.save").Hash.ToLower();$expected=if($spec.source -eq $original){$before}else{$fresh};if($hash -ne $expected){throw 'Immutable diagnostic snapshot copy mismatch'};$copies+=@{name=$spec.name;source=$spec.source;saveSHA256=$hash}
 }
 Remove-Item Env:\JUDAS_DIAGNOSTIC_CONTINUE_WRITE -ErrorAction SilentlyContinue;Remove-Item Env:\JUDAS_DIAGNOSTIC_RESTORE_DRIVES -ErrorAction SilentlyContinue
 foreach($name in @('state-fresh-baseline','state-original-baseline')){
  & "$root\M78ContinuationSource\Scripts\v6\windows_direct_run.ps1" -ManifestName 'm78-continuation-direct-combined-v2-state-observer.json' -CaseId 'state_observer' -RunId $name -Arguments @("$owned\$name",'cold-read')
 }
 if((Get-FileHash "$original\slots\m77.save").Hash.ToLower() -ne $before -or (Get-FileHash "$capture\slots\m77.save").Hash.ToLower() -ne $fresh){throw 'Original/capture snapshot altered'}
 $files=@();foreach($name in @('state-capture','state-fresh-baseline','state-original-baseline')){
  foreach($f in Get-ChildItem -LiteralPath "$owned\$name" -File){$n='m78-combined-v2-'+$name+'-'+$f.Name;Upload $f.FullName $n;$files+=@{name=$n;bytes=$f.Length;sha256=(Get-FileHash $f.FullName).Hash.ToLower()}}
 }
 $r="$out\m78-combined-v2-state-isolation.json";@{scope='Read-only all-body/joint/controller state observer. Fresh exact capture and original failed native snapshot baselines; uninterrupted capture retained separately. No commands, warm impulses or contacts added. Original acceptance/time/threshold unchanged.';copies=$copies;originalSaveBeforeSHA256=$before;originalSaveAfterSHA256=(Get-FileHash "$original\slots\m77.save").Hash.ToLower();capturedSaveSHA256=$fresh;archiveSHA256=$compiled.archiveSHA256;observerSourceSHA256=$compiled.compiles[0].sourceSHA256;files=$files;freeBytes=(Get-PSDrive C).Free}|ConvertTo-Json -Depth 7|Set-Content $r -Encoding UTF8;Upload $r ([IO.Path]::GetFileName($r))
}catch{$p="$out\m78-combined-v2-state-isolation-error.txt";[string]$_.Exception.Message|Set-Content $p;Upload $p ([IO.Path]::GetFileName($p));exit 1}
