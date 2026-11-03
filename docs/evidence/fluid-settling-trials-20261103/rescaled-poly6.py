"""Change pressure kernel support to its first unbiased larger nominal lattice root."""
from pathlib import Path
import shlex
import subprocess

work=Path(__file__).resolve().parent
root=work.parents[1]
build=root/".cache/2d-stage1-build"
source=(root/"src/FluidWorld.cpp").read_text()
start=source.index("        for (int iteration = 0; iteration < iterations; ++iteration) {")
end=source.index("        // This passive geometry state",start)
phase=source[start:end].replace("BuildNeighbors(positions, radius,","BuildNeighbors(positions, pressureKernel.radius,")
phase=phase.replace(", kernel)",", pressureKernel)")
source=source[:start]+phase+source[end:]
source=source.replace("    const FluidKernel kernel(radius);","""    const FluidKernel kernel(radius);
    // Nominal lattice has h/s=2.1. The next larger unity Poly6 sum is at
    // h/s=2.4125056685749637. Both pressure density and its existing Spiky
    // surrogate now use this support. Smoothing keeps its original support.
    const FluidKernel pressureKernel(radius * (2.4125056685749637f / 2.1f));""")
(src:=work/"rescaled-poly6.cpp").write_text(source)
flags={}
for line in (build/"CMakeFiles/judas_engine.dir/flags.make").read_text().splitlines():
    if " = " in line:
        k,v=line.split(" = ",1)
        flags[k]=shlex.split(v)
obj=work/"rescaled-poly6.o"
subprocess.run(["/usr/bin/c++",*flags["CXX_DEFINES"],*flags["CXX_INCLUDES"],*flags["CXX_FLAGS"],
                "-c",str(src),"-o",str(obj)],cwd=build,check=True)
for target, cases in (("judas_production_fluid_tests",("zero_gravity","half_025")),):
    exe=work/f"rescaled-poly6-{target}"
    link=[a for a in shlex.split((build/f"CMakeFiles/{target}.dir/link.txt").read_text()) if not a.startswith("-Wl,--dependency-file=")]
    link[link.index("-o")+1]=str(exe)
    link.insert(link.index("libjudas_engine.a"),str(obj))
    subprocess.run(link,cwd=build,check=True)
    for case in cases:
        out=work/f"rescaled-poly6-{case}"
        out.mkdir(exist_ok=True)
        with (out/"result.log").open("w") as log:
            result=subprocess.run([str(exe),"--case",case,"--strict-diagnostics","--output",str(out)],
                                 stdout=log,stderr=subprocess.STDOUT)
        print(case,result.returncode,flush=True)
