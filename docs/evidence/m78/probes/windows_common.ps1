$ErrorActionPreference='Stop';$ProgressPreference='SilentlyContinue'
$root='C:\JudasValidation';$out="$root\M78Results";New-Item -ItemType Directory $out -Force | Out-Null
$source="$root\Source";$bin="$root\Build\Release";$rows=@()
foreach($name in @('profiler','physical_balance_foundation','physical_balance_controller','physical_animation','character_motor')) {
 $exe="$bin\judas_${name}_tests.exe";$stdout="$out\$name.stdout.log";$stderr="$out\$name.stderr.log"
 if(Test-Path $stdout){throw "Retain existing M78 receipt: $stdout"}
 $watch=[Diagnostics.Stopwatch]::StartNew();$p=Start-Process -FilePath $exe -WorkingDirectory $source -PassThru -RedirectStandardOutput $stdout -RedirectStandardError $stderr;[void]$p.Handle
 if(!$p.WaitForExit(180000)){Stop-Process -Id $p.Id -Force;$code='timeout'}else{$p.Refresh();$code=$p.ExitCode}
 $rows+=@{case=$name;exe=$exe;sha256=(Get-FileHash $exe -Algorithm SHA256).Hash;exitCode=$code;seconds=$watch.Elapsed.TotalSeconds;stdout=(Get-Content $stdout -Raw);stderr=(Get-Content $stderr -Raw)}
}
$result=@{scope='Fresh common native tests only; no physical GPU/input acceptance';checkpoint='9e77b2226edcf217ab789fe4f0fea42a3415a72d';cwd=$source;rows=$rows;free=(Get-PSDrive C).Free}
$result|ConvertTo-Json -Depth 7 | Set-Content "$out\common.json" -Encoding UTF8
$result|ConvertTo-Json -Depth 7
