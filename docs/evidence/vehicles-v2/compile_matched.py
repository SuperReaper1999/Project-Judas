from pathlib import Path
import hashlib,json,shlex,subprocess,sys
build,source,output=map(lambda x:Path(x).resolve(),sys.argv[1:4])
opts=[]
for line in (build/'CMakeFiles/judas_wheel_tests.dir/flags.make').read_text().splitlines():
 if line.startswith(('CXX_DEFINES =','CXX_INCLUDES =','CXX_FLAGS =')):opts+=shlex.split(line.split('=',1)[1])
link=shlex.split((build/'CMakeFiles/judas_wheel_tests.dir/link.txt').read_text())
cmd=['/usr/bin/c++',*opts,str(source),'-o',str(output),*link[link.index('libjudas_engine.a'):]]
subprocess.run(cmd,cwd=build,check=True)
output.with_suffix('.provenance.json').write_text(json.dumps({'command':cmd,'cwd':str(build),'source_sha256':hashlib.sha256(source.read_bytes()).hexdigest(),'fixture_sha256':hashlib.sha256(Path('/home/conner/Documents/GitHub/Project-Judas/tests/support/VehicleFixture.h').read_bytes()).hexdigest(),'binary_sha256':hashlib.sha256(output.read_bytes()).hexdigest()},indent=2)+'\n')
