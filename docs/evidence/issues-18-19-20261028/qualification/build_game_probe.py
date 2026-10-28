from pathlib import Path
import shlex,subprocess
root=Path.cwd();out=root/'.cache/issues-18-19-investigation';build=root/'build';target=build/'CMakeFiles/judas_runtime_ui_application_tests.dir'
flags={}
for line in (target/'flags.make').read_text().splitlines():
 if ' = ' in line:
  k,v=line.split(' = ',1);flags[k]=shlex.split(v)
obj=out/'BlockGameProbe.o';subprocess.run(['/usr/bin/c++',*flags['CXX_DEFINES'],*flags['CXX_INCLUDES'],*flags['CXX_FLAGS'],'-c',str(out/'BlockGameProbe.cpp'),'-o',str(obj)],check=True)
link=shlex.split((target/'link.txt').read_text());link=[x for x in link if not x.startswith('-Wl,--dependency-file=')];link=[str(obj) if x.endswith('/tests/RuntimeUIApplicationTests.cpp.o') else x for x in link];link[link.index('-o')+1]=str(out/'block-game-probe');subprocess.run(link,cwd=build,check=True)
