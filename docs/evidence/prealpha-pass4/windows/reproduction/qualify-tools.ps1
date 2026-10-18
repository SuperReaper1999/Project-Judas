$ErrorActionPreference='Stop';$ProgressPreference='SilentlyContinue'
$task='P:\JudasValidation\PreAlphaPass4';$sdk="$task\Build\windows-sdk";$python='C:\JudasValidation\Python\python.exe'
$env:TEMP="$task\Temp";$env:TMP=$env:TEMP
$results="$task\ToolResults";New-Item -ItemType Directory -Force $results|Out-Null
$rows=New-Object 'System.Collections.Generic.List[object]'
function Run-Native($label,$exe,$arguments,$expected=0,$timeout=60,$cwd=$task){
 $stdout="$results\$label.stdout.log";$stderr="$results\$label.stderr.log";$options=@{FilePath=$exe;WorkingDirectory=$cwd;PassThru=$true;RedirectStandardOutput=$stdout;RedirectStandardError=$stderr}
 if($arguments.Count){$options.ArgumentList=@($arguments|ForEach-Object {'"'+([string]$_)+'"'})}
 $started=[DateTime]::UtcNow;$p=Start-Process @options;[void]$p.Handle;$finished=$p.WaitForExit($timeout*1000)
 if(!$finished){Stop-Process -Id $p.Id -Force;$exit='timeout'}else{$p.Refresh();$exit=$p.ExitCode;if($null-eq $exit){throw "No native exit code: $label"}}
 $row=[pscustomobject]@{label=$label;executable=$exe;arguments=@($arguments);cwd=$cwd;exit=$exit;expected=$expected;passed=($exit-eq $expected);seconds=([DateTime]::UtcNow-$started).TotalSeconds;stdout=[IO.File]::ReadAllText($stdout);stderr=[IO.File]::ReadAllText($stderr)}
 $rows.Add($row);$row|ConvertTo-Json -Depth 5|Set-Content -Encoding UTF8 "$results\$label.json"
}
if(!(Test-Path "$sdk\SDK_MANIFEST.json")){throw 'Complete new native SDK is required'}
# No mutation of this SDK payload is permitted; all tests write outside it.
Run-Native 'sdk-verify' $python @("$sdk\tools\package_sdk.py",'verify',$sdk)
Run-Native 'sdk-native-pe-imports' $python @("$sdk\tools\inspect_sdk.py",$sdk,'--host-check','--system-directory','C:\Windows\System32','--output',"$results\sdk-native-pe-imports.detail.json")
$spec=Get-Content -Raw "$sdk\packaging\sdk-inventory.json"|ConvertFrom-Json
$expectedIdentity=Get-Content -Raw "$sdk\BUILD_INFO.json"|ConvertFrom-Json
foreach($name in @($spec.tools)+@($spec.windows_tools)){
 Run-Native "identity-$name" "$sdk\$name.exe" @('--build-info-json')
 $row=$rows[$rows.Count-1]
 try{$identity=$row.stdout|ConvertFrom-Json;foreach($field in $expectedIdentity.PSObject.Properties.Name){if($identity.$field-cne $expectedIdentity.$field){throw "Identity mismatch: $name $field"}}}catch{$row.passed=$false;$row.stderr+="Identity validation: $_"}
}
$projects="$task\Projects";if(!(Test-Path $projects)){Expand-Archive -LiteralPath "$task\qualification-projects.zip" -DestinationPath $projects}
$project="$projects\projects\physics_stress_lab\physics_stress_lab.judasproj"
Run-Native 'script-check-consumer' "$sdk\judas_script_check.exe" @($project)
$work="$task\ToolWork";New-Item -ItemType Directory -Force $work|Out-Null
Run-Native 'scene-create' "$sdk\judas_scene_author.exe" @('--create','scene',"$work\empty.judas")
Run-Native 'scene-validate' "$sdk\judas_scene_author.exe" @('--validate','scene',"$work\empty.judas")
Run-Native 'scene-invalid-negative' "$sdk\judas_scene_author.exe" @('--validate','scene',"$work\absent.judas") 1
Run-Native 'terrain-create' "$sdk\judas_terrain_author.exe" @('create',"$work\terrain.judasterrain",9,9,8,8)
Run-Native 'terrain-validate' "$sdk\judas_terrain_author.exe" @('validate',"$work\terrain.judasterrain")
Run-Native 'terrain-edit' "$sdk\judas_terrain_author.exe" @('brush',"$work\terrain.judasterrain",'raise',0,0,0,2,0.2)
Run-Native 'volume-create' "$sdk\judas_volume_author.exe" @('create',"$work\volume.judasvolume",9,9,9,8,8,8)
Run-Native 'volume-edit' "$sdk\judas_volume_author.exe" @('brush',"$work\volume.judasvolume",'remove',0,0,0,2,0.5)
Run-Native 'volume-validate' "$sdk\judas_volume_author.exe" @('validate',"$work\volume.judasvolume")
Run-Native 'deformable-bake' "$sdk\judas_deformable_bake.exe" @('block',"$work\block.judasdeform",2,2,2,1,1,1)
Run-Native 'cook-texture' "$sdk\judas_cook.exe" @("$task\Source\assets\branding\judas-icon.png","$work\icon.judastex",128,4,0.02,0,0)
Run-Native 'cook-invalid-negative' "$sdk\judas_cook.exe" @("$work\absent.png","$work\must-not-exist.judastex") 1
Run-Native 'model-import' "$sdk\judas_model_import_cli.exe" @("$task\Source\assets\models\beacon.obj","$work\beacon.judasmodel")
Run-Native 'collision-hull-cook' "$sdk\judas_collision_cook.exe" @('hull',"$task\Source\assets\models\beacon.obj","$work\beacon.judascollision",'00000000000000000000000000000001',0)
$hdr=[Text.Encoding]::ASCII.GetBytes("#?RADIANCE`nFORMAT=32-bit_rle_rgbe`n`n-Y 2 +X 4`n")+[byte[]](128,160,200,129,128,160,200,129,128,160,200,129,128,160,200,129,128,160,200,129,128,160,200,129,128,160,200,129,128,160,200,129)
[IO.File]::WriteAllBytes("$work\constant.hdr",$hdr)
Run-Native 'environment-real-hdr-bake' "$sdk\judas_environment_bake.exe" @("$work\constant.hdr","$work\constant.judasenv",16,32)
Run-Native 'navigation-dependencies-empty-project' "$sdk\judas_navigation_bake.exe" @($project,'--check-dependents')
Run-Native 'liquid-invalid-project-negative' "$sdk\judas_liquid_bake.exe" @("$work\absent.judasproj",'Scenes/absent.judas') 1
Run-Native 'mod-invalid-package-negative' "$sdk\judas_mod.exe" @('inspect',"$work\absent.judasmod") 1
Run-Native 'server-invalid-port-negative' "$sdk\judas_server.exe" @($project,'70000',1) 1
Run-Native 'save-content-identity'  "$sdk\judas_save.exe" @('identity',$project)
Run-Native 'windows-readiness' "$sdk\judas_windows_readiness_tests.exe" @() 0 120
$allPassed=@($rows|Where-Object {!$_.passed}).Count-eq 0
[pscustomobject]@{utc=[DateTime]::UtcNow.ToString('o');kind='native-windows-sdk-and-tools';rows=$rows.ToArray();passed=$allPassed;scope='Exact final source archive; all SDK tools compile/stage and emit identity, selected tools exercised with real positive/negative operations. Not every subsystem feature is certified.'}|ConvertTo-Json -Depth 7|Set-Content -Encoding UTF8 "$task\TOOLS_RESULTS.json"
if(!$allPassed){exit 1}
