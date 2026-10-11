$ErrorActionPreference='Stop';$ProgressPreference='SilentlyContinue';$root='C:\JudasValidation';$base='http://192.168.122.1:8872';$receipt="$root\Receipts\M77";$bin="$root\Build\Release";$staged="$root\M77TestBin"
function Upload($p){Invoke-WebRequest -UseBasicParsing "$base/$([IO.Path]::GetFileName($p))" -Method Post -InFile $p|Out-Null}
function Quote($s){return '"'+$s+'"'}
$rows=@()
function Run($label,$exe,$arguments,$timeout=240,$cwd="$root\Source"){
 $watch=[Diagnostics.Stopwatch]::StartNew();$stdout="$receipt\m77-$label.stdout.log";$stderr="$receipt\m77-$label.stderr.log";$opts=@{FilePath=$exe;WorkingDirectory=$cwd;PassThru=$true;RedirectStandardOutput=$stdout;RedirectStandardError=$stderr};if($arguments.Count){$opts.ArgumentList=$arguments};$p=Start-Process @opts;[void]$p.Handle
 if(!$p.WaitForExit($timeout*1000)){Stop-Process -Id $p.Id -Force;$code='timeout'}else{$p.Refresh();$code=$p.ExitCode;if($null -eq $code){throw 'missing exit status'}}
 Upload $stdout;Upload $stderr;$r=@{label=$label;exitcode=$code;seconds=$watch.Elapsed.TotalSeconds;command=$exe;arguments=$arguments;cwd=$cwd};$script:rows+=$r;$r|ConvertTo-Json -Depth 4|Set-Content "$receipt\m77-$label.result.json" -Encoding UTF8;Upload "$receipt\m77-$label.result.json";return $code
}
try{
 Copy-Item "$bin\judas_balance_lab_tests.exe" "$staged\judas_balance_lab_tests.exe" -Force
 Copy-Item "$bin\judas_profiler_tests.exe" "$staged\judas_profiler_tests.exe" -Force
 $env:GALLIUM_DRIVER='llvmpipe'
 
 Run 'step-trace-verified' "$staged\judas_balance_lab_tests.exe" @("$root\M77Results\step-trace-verified",'step-trace')|Out-Null
 Copy-Item "$root\M77Results\step-trace-verified\step-trace.jsonl" "$receipt\m77-step-trace-verified.jsonl";Upload "$receipt\m77-step-trace-verified.jsonl"
}catch{$_|Out-String|Set-Content "$receipt\m77-windows-trace-failure.log";Upload "$receipt\m77-windows-trace-failure.log";throw}
