$ErrorActionPreference='Stop'; $ProgressPreference='SilentlyContinue'
$root='C:\JudasValidation'; $out="$root\M78ContinuationResults"; $base='http://192.168.122.1:8872'; $source="$root\M78ContinuationSource\Probes"
function Upload($p,$name){Invoke-WebRequest -UseBasicParsing "$base/$name" -Method Post -InFile $p|Out-Null}
try {
  New-Item -ItemType Directory $source -Force|Out-Null
  $target="$source\event_angular_transport.cpp"
  if(Test-Path $target){throw 'Preserve preceding native event observer input'}
  $manifest=(Invoke-WebRequest -UseBasicParsing "$base/m78-continuation-event-source.json").Content|ConvertFrom-Json
  Invoke-WebRequest -UseBasicParsing "$base/m78-continuation-event-angular-transport.cpp" -OutFile $target
  $actual=(Get-FileHash -LiteralPath $target).Hash.ToLower()
  if($actual -ne $manifest.sha256){throw 'Event observer source SHA mismatch'}
  Import-Module "$root\VS2022\Common7\Tools\Microsoft.VisualStudio.DevShell.dll"
  Enter-VsDevShell -VsInstallPath "$root\VS2022" -SkipAutomaticLocation -DevCmdArguments '-arch=x64 -host_arch=x64'
  & lib.exe /nologo /list "$root\Build\Release\judas_engine.lib" | Set-Content "$out\m78-continuation-engine-members.txt" -Encoding UTF8
  if($LASTEXITCODE -ne 0){throw 'Existing archive member enumeration failed'}
  @{scope='Exact current observer source staging and read-only library member enumeration; no compilation';path=$target;sha256=$actual;bytes=(Get-Item $target).Length;libExecutable=(Get-Command lib.exe).Source;compilerExecutable=(Get-Command cl.exe).Source;linkExecutable=(Get-Command link.exe).Source;freeBytes=(Get-PSDrive C).Free}|ConvertTo-Json -Depth 5|Set-Content "$out\m78-continuation-event-observer-stage.json" -Encoding UTF8
  Upload "$out\m78-continuation-engine-members.txt" 'm78-continuation-engine-members.txt'
  Upload "$out\m78-continuation-event-observer-stage.json" 'm78-continuation-event-observer-stage.json'
}catch {
  [string]$_.Exception.Message|Set-Content "$out\m78-continuation-event-observer-stage-error.txt"
  Upload "$out\m78-continuation-event-observer-stage-error.txt" 'm78-continuation-event-observer-stage-error.txt'
  exit 1
}
