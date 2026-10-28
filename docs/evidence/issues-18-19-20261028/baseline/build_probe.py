from pathlib import Path
import shlex,subprocess
root=Path('/home/conner/Documents/GitHub/Project-Judas');out=Path('/tmp/judas-issues18-19-flight-review');build=root/'build'
flags={}
for line in (build/'CMakeFiles/judas_script_tests.dir/flags.make').read_text().splitlines():
 if ' = ' in line:
  k,v=line.split(' = ',1);flags[k]=shlex.split(v)
obj=out/'IssueProbe.o';subprocess.run(['/usr/bin/c++',*flags['CXX_DEFINES'],*flags['CXX_INCLUDES'],*flags['CXX_FLAGS'],'-c',str(out/'IssueProbe.cpp'),'-o',str(obj)],check=True)
link=shlex.split((build/'CMakeFiles/judas_script_tests.dir/link.txt').read_text());link=[x for x in link if not x.startswith('-Wl,--dependency-file=')];link=[str(obj) if x.endswith('/tests/ScriptTests.cpp.o') else x for x in link];link[link.index('-o')+1]=str(out/'issue-probe');subprocess.run(link,cwd=build,check=True)
