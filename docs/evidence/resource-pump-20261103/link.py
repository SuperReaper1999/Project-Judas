import sys,shlex,subprocess
from pathlib import Path
p=Path(__file__).parent
b=Path(sys.argv[1])
a=shlex.split((b/'CMakeFiles/judas_asset_residency_tests.dir/link.txt').read_text())
a=[str(p/'bench.o') if x.endswith('AssetResidencyTests.cpp.o') else str(p/sys.argv[2]) if x=='judas_asset_residency_tests' else x for x in a if not x.startswith('-Wl,--dependency-file')]
subprocess.run(a,cwd=b,check=True)
