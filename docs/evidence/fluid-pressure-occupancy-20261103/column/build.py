from pathlib import Path
import shlex
import subprocess
import sys

work=Path(__file__).resolve().parent
root=work.parents[1]
build=root/".cache/2d-stage1-build"
target=build/"CMakeFiles/judas_production_fluid_tests.dir"
flags={}
for line in (target/"flags.make").read_text().splitlines():
    if " = " in line:
        k,v=line.split(" = ",1)
        flags[k]=shlex.split(v)
obj=work/"column.o"
subprocess.run(["/usr/bin/c++",*flags["CXX_DEFINES"],*flags["CXX_INCLUDES"],*flags["CXX_FLAGS"],
                "-c",str(work/"column.cpp"),"-o",str(obj)],cwd=build,check=True)
name=sys.argv[1] if len(sys.argv)>1 else "baseline"
exe=work/("column-"+name)
link=[a for a in shlex.split((target/"link.txt").read_text()) if not a.startswith("-Wl,--dependency-file=")]
link[link.index("-o")+1]=str(exe)
link[link.index(next(a for a in link if a.endswith("ProductionFluidTests.cpp.o")))]=str(obj)
if len(sys.argv)>2:
    link.insert(link.index("libjudas_engine.a"),str(Path(sys.argv[2]).resolve()))
subprocess.run(link,cwd=build,check=True)
for n, orientation, support in ((2,"aligned","floor"),(5,"aligned","floor"),(8,"aligned","floor"),(8,"rotated","floor"),(8,"aligned","free"),(8,"rotated","free")):
    with (work/f"{name}-{n}-{orientation}-{support}.csv").open("w") as log:
        subprocess.run([str(exe),str(n),orientation,support],stdout=log,stderr=subprocess.STDOUT,check=True)
