from pathlib import Path
import shlex,subprocess,json
r=Path.cwd();w=Path(__file__).resolve().parent;b=r/'build';flags={}
for l in(b/'CMakeFiles/judas_engine.dir/flags.make').read_text().splitlines():
 if' = 'in l:
  k,v=l.split(' = ',1);flags[k]=shlex.split(v)
cmd=['c++',*flags['CXX_DEFINES'],'-I'+str(r),'-I'+str(w),*flags['CXX_INCLUDES'],*flags['CXX_FLAGS'],'-c',str(w/'Replay.cpp'),'-o',str(w/'Replay.o')];subprocess.run(cmd,check=True)
link=shlex.split((b/'CMakeFiles/judas_impact_solver_tests.dir/link.txt').read_text());link=[x for x in link if not x.startswith('CMakeFiles/')and not x.startswith('-Wl,--dependency-file=')];link.insert(1,str(w/'Replay.o'));link[link.index('-o')+1]=str(w/'Replay');subprocess.run(link,cwd=b,check=True)
