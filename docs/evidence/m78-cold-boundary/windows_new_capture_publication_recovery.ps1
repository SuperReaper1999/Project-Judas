$ErrorActionPreference='Stop';$ProgressPreference='SilentlyContinue';$root='C:\JudasValidation';$base='http://192.168.122.1:8872';$m=$null
function Upload($p,$n){Invoke-WebRequest -UseBasicParsing "$base/$n" -Method Post -InFile $p|Out-Null}
try{
 $m=(Invoke-WebRequest -UseBasicParsing "$base/m78-native-new-capture-publication.json").Content|ConvertFrom-Json;$scratch=$m.scratch;$out=$m.output
 if((Get-FileHash $m.library.path).Hash.ToLower() -ne $m.library.sha256){throw 'Frozen archive changed'}
 $restored=@()
 foreach($case in $m.cases){foreach($u in $case.objects){
  if(!(Test-Path $u.object)){Invoke-WebRequest -UseBasicParsing "$base/m78-new-publication-original-$([IO.Path]::GetFileName($u.object))" -OutFile $u.object}
  if((Get-FileHash $u.object).Hash.ToLower() -ne $u.objectSHA256){throw 'Original restored object mismatch'}
  $restored+=@{path=$u.object;sha256=(Get-FileHash $u.object).Hash.ToLower();bytes=(Get-Item $u.object).Length}
 }}
 Import-Module "$root\VS2022\Common7\Tools\Microsoft.VisualStudio.DevShell.dll";Enter-VsDevShell -VsInstallPath "$root\VS2022" -SkipAutomaticLocation -DevCmdArguments '-arch=x64 -host_arch=x64';Set-Location "$root\Build";$compiles=@()
 foreach($name in @('PhysicsWorld','WorldPersistence','PhysicalBalance')){
  $source="$scratch\source\src\$name.cpp";$obj="$scratch\objects\$name.obj";$response=Get-Content "$scratch\objects\$name-compile.rsp" -Raw
  $expected=@($m.files|Where-Object {$_.path -eq "src/$name.cpp"})[0].sha256
  if((Get-FileHash $source).Hash.ToLower() -ne $expected){throw 'Compiled source changed'}
  $compiles+=@{source=$source;sha256=$expected;object=$obj;objectSHA256=(Get-FileHash $obj).Hash.ToLower();response=$response;reusedSuccessfulCompile=$true}
 }
 $recovery=@{scope='Recovery only: exact original fixture objects restored from independently hash-verified prior archive; successful copied prototype TUs reused, no recompile';objects=$restored;compiles=$compiles;library=$m.library;freeBytes=(Get-PSDrive C).Free}
 $rp="$out\m78-native-new-publication-recovery.json";$recovery|ConvertTo-Json -Depth 6|Set-Content $rp -Encoding UTF8;Upload $rp ([IO.Path]::GetFileName($rp))
 $ErrorActionPreference='Continue'
 $env:GALLIUM_DRIVER='llvmpipe';$env:JUDAS_ENGINE_ROOT="$root\Source";$env:TEMP="$out\temp";$env:TMP=$env:TEMP;New-Item -ItemType Directory $env:TEMP -Force|Out-Null;$env:M78_DIAGNOSTIC_PERSIST_DRIVE_IMPULSES='1'
 foreach($n in @('JUDAS_DIAGNOSTIC_RESTORE_DRIVES','JUDAS_BOUNDARY_REPLAY_CAPTURED_NATIVE','JUDAS_BOUNDARY_REPLAY_MODE','JUDAS_BOUNDARY_DISCARD_FIRST_JOINT_SEEDS','JUDAS_BOUNDARY_ORACLE_FIRST_SOLVE','JUDAS_BOUNDARY_ORACLE_ALL_SOLVES','JUDAS_DIAGNOSTIC_CONTINUE_WRITE')){[Environment]::SetEnvironmentVariable($n,$null,'Process')};$runs=@();$links=@()
 foreach($case in $m.cases){
  foreach($u in $case.objects){if((Get-FileHash $u.object).Hash.ToLower() -ne $u.objectSHA256 -or (Get-FileHash $u.source).Hash.ToLower() -ne $u.sourceSHA256){throw 'Original fixture/object changed'}}
  $name="m78-native-new-publication-$($case.id)";$exe="$root\Build\Release\$name.exe";if(Test-Path $exe){throw 'Preserve preceding prototype exe'};$map="$out\$name.map";$link=[string]$case.linkFlags;$link=[regex]::Replace($link,'/OUT:"[^"]*"',('/OUT:"'+$exe+'"'),'IgnoreCase');$link=[regex]::Replace($link,'RELEASE\\JUDAS_ENGINE\.LIB',('"'+$m.library.path+'"'),'IgnoreCase');$link=[regex]::Replace($link,'/PDB:"[^"]*"',('/PDB:"'+$scratch+'\objects\'+$case.id+'.pdb"'),'IgnoreCase');$link=[regex]::Replace($link,'/IMPLIB:"[^"]*"',('/IMPLIB:"'+$scratch+'\objects\'+$case.id+'.lib"'),'IgnoreCase');$prefix='';foreach($u in $case.objects){$prefix+='"'+$u.object+'" '};foreach($u in $compiles){$prefix+='"'+$u.object+'" '};$link=$prefix+$link+' /VERBOSE:LIB /MAP:"'+$map+'"';$rsp="$scratch\objects\$($case.id)-link.rsp";[IO.File]::WriteAllText($rsp,$link,[Text.UTF8Encoding]::new($false));$log="$out\$name-link.log";[IO.File]::WriteAllText($log,'');& link.exe "@$rsp" 2>&1|Tee-Object -FilePath $log;$code=$LASTEXITCODE;Upload $log ([IO.Path]::GetFileName($log));Upload $map ([IO.Path]::GetFileName($map));if($code -ne 0){throw 'Copied publication prototype link failed'}
  $bad=@(Get-Content $log|Where-Object {$_ -match 'Loaded.*judas_engine\.lib\([^)]*(PhysicsWorld|WorldPersistence|PhysicalBalance)\.obj\)'});if($bad.Count){throw 'Original replaced archive member extracted'};$links+=@{case=$case.id;exe=$exe;sha256=(Get-FileHash $exe).Hash.ToLower();linkerResponse=$link;mapSHA256=(Get-FileHash $map).Hash.ToLower();compiles=$compiles;fixtures=$case.objects}
  $modes=$(if($case.id -eq 'balance_lab'){@('cold-write','cold-read','legacy-a106-read')}else{@('controller')})
  foreach($mode in $modes){
   $rn="$name-$mode";$dest="$out\new-capture";$args=@();if($mode -eq 'legacy-a106-read'){$dest="$out\legacy-a106";New-Item -ItemType Directory "$dest\slots"|Out-Null;Copy-Item $m.project "$dest\project" -Recurse;Copy-Item $m.exactSave.path "$dest\slots\m77.save";if((Get-FileHash "$dest\slots\m77.save").Hash.ToLower() -ne $m.exactSave.sha256){throw 'Original a106 fixture changed'};$args=@($dest,'cold-read')}elseif($case.id -eq 'balance_lab'){$args=@($dest,$mode)}
   $options=@{FilePath=$exe;WorkingDirectory="$root\Source";PassThru=$true;RedirectStandardOutput="$out\$rn.stdout.log";RedirectStandardError="$out\$rn.stderr.log"};if($args.Count){$options.ArgumentList=$args};$watch=[Diagnostics.Stopwatch]::StartNew();$p=Start-Process @options;[void]$p.Handle;if(!$p.WaitForExit(600000)){Stop-Process -Id $p.Id -Force;$exit='owned prototype timeout'}else{$p.Refresh();$exit=$p.ExitCode};$watch.Stop();$files=@()
   if($case.id -eq 'balance_lab'){
    foreach($f in @(Get-ChildItem $dest -File|Where-Object {$_.Extension -in '.json','.jsonl'})+@(Get-ChildItem "$dest\slots" -File)){$report="$rn-$($f.Name)";Upload $f.FullName $report;$files+=@{path=$f.FullName;reportName=$report;sha256=(Get-FileHash $f.FullName).Hash.ToLower();bytes=$f.Length}}
   }
   $row=@{scope=$m.scope;case=$case.id;mode=$mode;exitCode=$exit;seconds=$watch.Elapsed.TotalSeconds;executableSHA256=(Get-FileHash $exe).Hash.ToLower();library=$m.library;arguments=$args;files=$files;sourceBundleSHA256=$m.sourceBundleSHA256;freeBytes=(Get-PSDrive C).Free};$runs+=$row;$rp="$out\$rn.json";$row|ConvertTo-Json -Depth 8|Set-Content $rp -Encoding UTF8;Upload $rp "$rn.json";Upload "$out\$rn.stdout.log" "$rn.stdout.log";Upload "$out\$rn.stderr.log" "$rn.stderr.log"
  }
 }
 $rp="$out\m78-native-new-publication-proof.json";@{scope=$m.scope;links=$links;runs=$runs;freeBytes=(Get-PSDrive C).Free}|ConvertTo-Json -Depth 9|Set-Content $rp -Encoding UTF8;Upload $rp ([IO.Path]::GetFileName($rp))
}catch{$out=$(if($m){$m.output}else{'C:\JudasValidation\M78NativeFunctionalResults'});New-Item -ItemType Directory $out -Force|Out-Null;$name='m78-native-new-publication-recovery-error.txt';[string]$_.Exception.Message|Set-Content "$out\$name";Upload "$out\$name" $name;exit 1}
