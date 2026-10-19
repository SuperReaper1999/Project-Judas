from pathlib import Path
import shlex
import subprocess

h = Path(__file__).resolve().parent
r = h.parents[2]
b = r / "build"
f = {}
for line in (b / "CMakeFiles/judas_engine.dir/flags.make").read_text().splitlines():
    if " = " in line:
        k, v = line.split(" = ", 1)
        f[k] = shlex.split(v)
a = ["/usr/bin/c++", *f["CXX_DEFINES"], *f["CXX_INCLUDES"], "-I" + str(r / "src/physics"),
     *f["CXX_FLAGS"], "-c", str(h / "QueryParity.cpp"), "-o", str(h / "QueryParity.o")]
print(shlex.join(a), flush=True)
subprocess.run(a, check=True)
a = shlex.split((b / "CMakeFiles/judas_impact_timing_tests.dir/link.txt").read_text())
o = []
for x in a:
    if x.startswith("-Wl,--dependency-file=") or x.endswith("tests/ValidationMain.cpp.o"):
        continue
    if x.endswith("tests/ImpactTimingTests.cpp.o"):
        x = str(h / "QueryParity.o")
    elif x == "judas_impact_timing_tests":
        x = str(h / "query-parity")
    elif x == "libjudas_engine.a":
        x = str(h.parent / "baseline-e30/libjudas_engine.a")
    elif not x.startswith("-") and not x.startswith("/") and (b / x).exists():
        x = str(b / x)
    o.append(x)
print(shlex.join(o), flush=True)
subprocess.run(o, check=True)
