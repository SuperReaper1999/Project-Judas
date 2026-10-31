from pathlib import Path
import shlex,subprocess,sys
r=Path(__file__).resolve().parents[2];b=r/'.cache/2d-stage1-build';o=Path(__file__).resolve().parent;f={}
for line in (b/'CMakeFiles/judas.dir/flags.make').read_text().splitlines():
 if ' = ' in line:k,v=line.split(' = ',1);f[k]=shlex.split(v)
source=Path(sys.argv[1]).resolve();target=Path(sys.argv[2]).resolve();obj=target.with_suffix('.o')
subprocess.run(['c++','-std=c++17','-O3','-DNDEBUG','-Wall','-Wextra',*f['CXX_DEFINES'],*f['CXX_INCLUDES'],'-c',str(source),'-o',str(obj)],check=True)
if '--object-only' in sys.argv: sys.exit(0)
l=shlex.split((b/'CMakeFiles/judas.dir/link.txt').read_text());subprocess.run([l[0],str(obj),*[str(Path(x).resolve()) for x in sys.argv[3:]],'-o',str(target),*l[l.index('-o')+2:]],cwd=b,check=True)
