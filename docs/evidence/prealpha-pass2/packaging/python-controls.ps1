$ErrorActionPreference='Stop';$ProgressPreference='SilentlyContinue'
$root='C:\JudasValidation\PreAlphaPass2';$python='C:\JudasValidation\Python\python.exe';$logs="$root\Results"
$ErrorActionPreference='Continue'
& $python "$root\Source\scripts\prealpha_distribution_checks.py" --output "$logs\native-python-distribution.json" 2>&1 | Tee-Object "$logs\native-python-distribution.log"
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
& $python "$logs\inspect-system-pe.py" "$root\Source\tools" "$logs\actual-system-pe.json"
exit $LASTEXITCODE
