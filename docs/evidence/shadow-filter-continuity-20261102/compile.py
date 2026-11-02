from pathlib import Path
import subprocess,shlex,sys
build=Path('.cache/2d-stage1-build').resolve()
flags=(build/'CMakeFiles/judas_rendering_architecture_tests.dir/flags.make').read_text()
get=lambda key: shlex.split(next(l.split(' = ',1)[1] for l in flags.splitlines() if l.startswith(key+' = ')))
link=shlex.split((build/'CMakeFiles/judas_rendering_architecture_tests.dir/link.txt').read_text())
source=Path(sys.argv[1]).resolve(); dest=Path(sys.argv[2]).resolve()
cmd=['c++',*get('CXX_DEFINES'),*get('CXX_INCLUDES'),*get('CXX_FLAGS'),str(source),'-o',str(dest),*link[link.index('libjudas_engine.a'):]]
subprocess.run(cmd,cwd=build,check=True)
