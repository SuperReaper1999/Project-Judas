$ErrorActionPreference='Stop';$ProgressPreference='SilentlyContinue'
$root='C:\JudasValidation\PreAlphaPass2';$python='C:\JudasValidation\Python\python.exe';$logs="$root\Results"
if((Get-FileHash "$root\windows-python-followup.zip" -Algorithm SHA256).Hash.ToLowerInvariant() -ne "188a66511c5b6a1813d032c14df54bf2ade1b62b8dae9de96c48a4bf53872a21") {throw "Fixture hash mismatch"}
Expand-Archive -LiteralPath "$root\windows-python-followup.zip" -DestinationPath "$root\PythonFollowup"
$ErrorActionPreference='Continue'
& $python "$root\PythonFollowup\scripts\prealpha_distribution_checks.py" --output "$logs\followup-native-python-distribution.json" 2>&1 | Tee-Object "$logs\followup-native-python-distribution.log"
$exit=$LASTEXITCODE;$ErrorActionPreference='Stop';if($exit -ne 0){exit $exit}
$code=@'
import json, pathlib, sys
sys.path.insert(0, sys.argv[1])
from binary_dependencies import inspect
from hashlib import sha256
exe=pathlib.Path(r'C:\Windows\System32\cmd.exe')
result={'binary':str(exe),'sha256':sha256(exe.read_bytes()).hexdigest(),'metadata':inspect(exe.read_bytes()),'scope':'actual Windows system PE parsing only; not current Judas engine execution'}
assert result['metadata']['format']=='PE32+' and result['metadata']['machine']==0x8664
pathlib.Path(sys.argv[2]).write_text(json.dumps(result,indent=2)+'\n')
'@
[IO.File]::WriteAllText("$logs\inspect-system-pe.py",$code)
& $python "$logs\inspect-system-pe.py" "$root\PythonFollowup\tools" "$logs\actual-system-pe.json"
exit $LASTEXITCODE
