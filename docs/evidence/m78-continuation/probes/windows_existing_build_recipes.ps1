$ErrorActionPreference='Stop'; $ProgressPreference='SilentlyContinue'
$root='C:\JudasValidation'; $build="$root\Build"; $out="$root\M78ContinuationResults"; $base='http://192.168.122.1:8872'
function Upload($p,$name){Invoke-WebRequest -UseBasicParsing "$base/$name" -Method Post -InFile $p|Out-Null}
try {
  $rows=@(); $targets=@('judas_engine','judas_rigid_motion_tests','judas_physical_balance_controller_tests','judas_physical_balance_foundation_tests','judas_physical_animation_tests','judas_balance_lab_tests','judas','judas_export')
  foreach($target in $targets){
    $project="$build\$target.vcxproj"
    if(Test-Path $project){Upload $project ('m78-continuation-'+$target+'.vcxproj');$rows+=@{target=$target;kind='project';path=$project;sha256=(Get-FileHash $project).Hash.ToLower();bytes=(Get-Item $project).Length}}
    $dir="$build\$target.dir\Release"
    if(Test-Path $dir){
      foreach($p in @(Get-ChildItem -LiteralPath $dir -Recurse -File | Where-Object {$_.Name -eq 'CL.command.1.tlog' -or $_.Name -eq 'link.command.1.tlog'})){
        Upload $p.FullName ('m78-continuation-'+$target+'-'+$p.Name)
        $rows+=@{target=$target;kind='command-recipe';path=$p.FullName;sha256=(Get-FileHash $p.FullName).Hash.ToLower();bytes=$p.Length}
      }
    }
  }
  $archive="$build\Release\judas_engine.lib"; $archiveRow=@{path=$archive;sha256=(Get-FileHash $archive).Hash.ToLower();bytes=(Get-Item $archive).Length}
  @{scope='Read-only existing compiler/link recipes and engine archive identity; no compilation, extraction, VM cleanup or source changes';recipes=$rows;engineArchive=$archiveRow;freeBytes=(Get-PSDrive C).Free}|ConvertTo-Json -Depth 5|Set-Content "$out\m78-continuation-existing-recipes.json" -Encoding UTF8
  Upload "$out\m78-continuation-existing-recipes.json" 'm78-continuation-existing-recipes.json'
}catch {
  [string]$_.Exception.Message|Set-Content "$out\m78-continuation-existing-recipes-error.txt"
  Upload "$out\m78-continuation-existing-recipes-error.txt" 'm78-continuation-existing-recipes-error.txt'
  exit 1
}
