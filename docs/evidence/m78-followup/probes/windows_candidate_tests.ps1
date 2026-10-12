$ErrorActionPreference='Stop';$ProgressPreference='SilentlyContinue';$root='C:\JudasValidation';$base='http://192.168.122.1:8872';$out="$root\M78FollowupResults";$source="$root\Source";$bin="$root\Build\Release";$rows=@()
function Upload($p){Invoke-WebRequest -UseBasicParsing "$base/$([IO.Path]::GetFileName($p))" -Method Post -InFile $p|Out-Null}
if((Get-PSDrive C).Free -lt 100MB){throw 'Retained reserve insufficient for focused tests'}
$fixture="$root\M78DependencyFixture";if(Test-Path $fixture){throw 'Use fresh owned fixture; preserve preceding runs'}
Invoke-WebRequest -UseBasicParsing "$base/m78-dependent-fixture.zip" -OutFile "$out\dependent-fixture.zip";Expand-Archive -LiteralPath "$out\dependent-fixture.zip" -DestinationPath $fixture
$env:TEMP="$out\temp";$env:TMP=$env:TEMP;New-Item -ItemType Directory $env:TEMP -Force|Out-Null
foreach($name in @('rigid_motion','joint','collision_geometry','prefab','physical_balance_foundation','physical_balance_controller','physical_animation','character_motor','streaming','scalability','navigation_dependency')){
 $exe="$bin\judas_${name}_tests.exe";$stdout="$out\native-$name.stdout.log";$stderr="$out\native-$name.stderr.log";if(Test-Path $stdout){throw "Preserve previous receipt $stdout"}
 $args=@();if($name -eq 'scalability'){$args=@("$out\scalability")}elseif($name -eq 'navigation_dependency'){$args=@("$fixture\landscape_fork.judasproj",'Imports/landscape.judasterrain')}
 $watch=[Diagnostics.Stopwatch]::StartNew();$options=@{FilePath=$exe;WorkingDirectory=$source;PassThru=$true;RedirectStandardOutput=$stdout;RedirectStandardError=$stderr};if($args.Count){$options.ArgumentList=$args}
 $p=Start-Process @options;[void]$p.Handle;$timeout=$name -eq 'physical_balance_controller' -or $name -eq 'scalability';$limit=if($timeout){600000}else{180000}
 if(!$p.WaitForExit($limit)){Stop-Process -Id $p.Id -Force;$code='owned test timed out'}else{$p.Refresh();$code=$p.ExitCode}
 $rows+=@{case=$name;arguments=$args;exe=$exe;sha256=(Get-FileHash $exe).Hash.ToLower();exitCode=$code;seconds=$watch.Elapsed.TotalSeconds;stdoutBytes=(Get-Item $stdout).Length;stderrBytes=(Get-Item $stderr).Length}
 Upload $stdout;Upload $stderr;@{scope='Native VM focused checks, not hardware acceptance';cwd=$source;rows=$rows;freeBytes=(Get-PSDrive C).Free}|ConvertTo-Json -Depth 6|Set-Content "$out\m78-candidate-native-tests.json" -Encoding UTF8;Upload "$out\m78-candidate-native-tests.json"
}
