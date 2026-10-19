#!/usr/bin/env python3
from pathlib import Path
import hashlib
import json
import shlex
import shutil
import subprocess
import time

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]
BUILD = ROOT / "build"
BASELINE = HERE.parent / "baseline-e30"


def run(args):
    print(shlex.join(map(str, args)), flush=True)
    start = time.time()
    subprocess.run(list(map(str, args)), cwd=ROOT, check=True)
    return time.time() - start


flags = {}
for line in (BUILD / "CMakeFiles/judas_engine.dir/flags.make").read_text().splitlines():
    if " = " in line:
        key, value = line.split(" = ", 1)
        flags[key] = shlex.split(value)
command = ["/usr/bin/c++", *flags["CXX_DEFINES"], *flags["CXX_INCLUDES"],
           "-I" + str(ROOT / "src/physics"), *flags["CXX_FLAGS"], "-c",
           HERE / "WorldImpactEvents.cpp", "-o", HERE / "WorldImpactEvents.cpp.o"]
compile_seconds = run(command)
archive = HERE / "libjudas_engine.a"
shutil.copy2(BASELINE / "libjudas_engine.a", archive)
run(["ar", "r", archive, HERE / "WorldImpactEvents.cpp.o"])
links = {}
for target in ("judas_physics_stress_live_benchmark", "judas_impact_timing_tests",
               "judas_physics_scaling_diagnostics"):
    original = shlex.split((BUILD / f"CMakeFiles/{target}.dir/link.txt").read_text())
    args = []
    for arg in original:
        if arg.startswith("-Wl,--dependency-file="):
            continue
        if arg == "libjudas_engine.a":
            args.append(str(archive))
        elif arg == target:
            args.append(str(HERE / target))
        elif not arg.startswith("-") and not arg.startswith("/") and (BUILD / arg).exists():
            args.append(str(BUILD / arg))
        else:
            args.append(arg)
    links[target] = run(args)
result = {"status": "compiled and linked; no checks executed by this script",
          "compile_seconds": compile_seconds, "link_seconds": links,
          "sha256": {p.name: hashlib.sha256(p.read_bytes()).hexdigest()
                     for p in [archive, *[HERE / target for target in links]]}}
(HERE / "BUILD.json").write_text(json.dumps(result, indent=2) + "\n")
print(json.dumps(result, indent=2))
