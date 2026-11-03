"""Extend only scratch observations, preserving the original 4 s measurements."""
import os
import pathlib
import shlex
import subprocess

work = pathlib.Path(__file__).resolve().parent
root = work.parents[1]
build = root / ".cache/2d-stage1-build"
target = build / "CMakeFiles/judas_production_fluid_tests.dir"
source = (root/"tests/ProductionFluidTests.cpp").read_text()
old = "const int count=settling?static_cast<int>(settling->late.end):240;"
assert source.count(old) == 1
(sourcepath := work/"small-body-extension.cpp").write_text(
    source.replace(old, "const int count=observationalScale?720:(settling?static_cast<int>(settling->late.end):240);"))
flags = {}
for line in (target/"flags.make").read_text().splitlines():
    if " = " in line:
        k,v=line.split(" = ", 1)
        flags[k]=shlex.split(v)
obj=work/"small-body-extension.o"
exe=work/"small-body-extension"
subprocess.run(["/usr/bin/c++",*flags["CXX_DEFINES"],*flags["CXX_INCLUDES"],*flags["CXX_FLAGS"],
                "-iquote",str(root/"tests"),"-c",str(sourcepath),"-o",str(obj)],cwd=build,check=True)
link=[arg for arg in shlex.split((target/"link.txt").read_text()) if not arg.startswith("-Wl,--dependency-file=")]
link[link.index("-o")+1]=str(exe)
link[link.index(next(arg for arg in link if arg.endswith("ProductionFluidTests.cpp.o")))]=str(obj)
subprocess.run(link,cwd=build,check=True)
out=work/"small-body-12s"
out.mkdir(exist_ok=True)
with (work/"small-body-12s.log").open("w") as log:
    status=subprocess.run([str(exe),"--case","small_mass_125x","--strict-diagnostics","--output",str(out)],stdout=log,stderr=subprocess.STDOUT).returncode
print(f"unchanged 4 s comparison plus 8 s observation, process exit {status}")
