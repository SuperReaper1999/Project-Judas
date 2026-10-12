$ErrorActionPreference='Stop';$ProgressPreference='SilentlyContinue'
$root='C:\JudasValidation';$out="$root\M78ContinuationResults";$base='http://192.168.122.1:8872'
function Upload($p,$name){Invoke-WebRequest -UseBasicParsing "$base/$name" -Method Post -InFile $p|Out-Null}
try {
 $recipes=@();foreach($target in @('judas_navigation_dependency_tests','judas_character_lab_tests')){
  $dir="$root\Build\$target.dir\Release"
  if(Test-Path $dir){foreach($p in @(Get-ChildItem -LiteralPath $dir -Recurse -File|Where-Object {$_.Name -eq 'CL.command.1.tlog' -or $_.Name -eq 'link.command.1.tlog'})){
   Upload $p.FullName ('m78-continuation-'+$target+'-'+$p.Name);$recipes+=@{target=$target;path=$p.FullName;sha256=(Get-FileHash $p.FullName).Hash.ToLower();bytes=$p.Length}
  }}
 }
 $inputs=@();foreach($relative in @('tests\PhysicalBalanceControllerTests.cpp','tests\RigidMotionTests.cpp','tests\ContactLifecycleTests.cpp','tests\CollisionGeometryTests.cpp','tests\PrefabTests.cpp','tests\WorldStreamingTests.cpp','tests\ScalabilityTests.cpp','tests\BalanceLabTests.cpp','tests\CharacterLabTests.cpp','tests\NavigationDependencyTests.cpp','projects\balance_lab','projects\character_lab','projects\ragdoll_demo','projects\post_m65_consumers\post_m65_consumers.judasproj','projects\post_m65_consumers\Scenes\skate\park.judas','projects\post_m65_consumers\Assets\skate\models\rider.glb','projects\post_m65_consumers\Assets\skate\models\rider.glb.judasmeta','projects\animation_demo\Assets\bend_bar.glb','docs\evidence\ftft3\fixtures')){
  $p="$root\Source\$relative";$row=@{relative=$relative;exists=(Test-Path -LiteralPath $p)}
  if($row.exists){if((Get-Item -LiteralPath $p).PSIsContainer){$files=@(Get-ChildItem -LiteralPath $p -Recurse -File);$row.files=$files.Count;$row.bytes=($files|Measure-Object -Property Length -Sum).Sum}else{$row.bytes=(Get-Item -LiteralPath $p).Length;$row.sha256=(Get-FileHash -LiteralPath $p).Hash.ToLower()}};$inputs+=$row
 }
 $libraries=@();foreach($name in @('judas_engine.lib','judas_world_authoring.lib','judas_navigation_baker.lib')){$p="$root\Build\Release\$name";$row=@{name=$name;exists=(Test-Path $p)};if($row.exists){$row.bytes=(Get-Item $p).Length;$row.sha256=(Get-FileHash $p).Hash.ToLower()};$libraries+=$row}
 $dlls=@(Get-ChildItem "$root\Build\Release" -Filter '*.dll' -File|ForEach-Object {@{name=$_.Name;bytes=$_.Length}})
 $receipt="$out\m78-continuation-final-matrix-inputs.json"
 if(Test-Path $receipt){throw 'Preserve prior matrix inventory'}
 @{scope='Read-only final-native recipe and fixture presence inventory; no compilation, cleanup or source replacement';recipes=$recipes;inputs=$inputs;libraries=$libraries;dlls=$dlls;freeBytes=(Get-PSDrive C).Free}|ConvertTo-Json -Depth 6|Set-Content $receipt -Encoding UTF8;Upload $receipt ([IO.Path]::GetFileName($receipt))
}catch{[string]$_.Exception.Message|Set-Content "$out\m78-continuation-final-matrix-inputs-error.txt";Upload "$out\m78-continuation-final-matrix-inputs-error.txt" 'm78-continuation-final-matrix-inputs-error.txt';exit 1}
