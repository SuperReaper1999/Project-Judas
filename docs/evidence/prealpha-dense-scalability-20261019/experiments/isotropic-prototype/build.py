from pathlib import Path
import hashlib
import json
import shlex
import shutil
import subprocess
import time

h = Path(__file__).resolve().parent
r = h.parents[2]
b = r / "build"
f = {}
for line in (b / "CMakeFiles/judas_engine.dir/flags.make").read_text().splitlines():
    if " = " in line:
        key, value = line.split(" = ", 1)
        f[key] = shlex.split(value)
commands = []


def run(args):
    args = list(map(str, args))
    print(shlex.join(args), flush=True)
    commands.append(args)
    subprocess.run(args, cwd=r, check=True)


start = time.time()
for name in ["RigidBody.cpp", "RigidBodyReference.cpp", "IsotropicTests.cpp"]:
    run(["/usr/bin/c++", *f["CXX_DEFINES"], *f["CXX_INCLUDES"], *f["CXX_FLAGS"],
         "-c", h / name, "-o", h / (name + ".o")])
archive = h / "libjudas_engine.a"
shutil.copy2(h.parent / "baseline-e30/libjudas_engine.a", archive)
run(["ar", "r", archive, h / "RigidBody.cpp.o"])
for target in ["judas_physics_stress_live_benchmark", "judas_physics_scaling_diagnostics",
               "judas_rigid_motion_tests", "judas_impact_timing_tests", "isotropic-tests"]:
    template = "judas_impact_timing_tests" if target == "isotropic-tests" else target
    args = shlex.split((b / f"CMakeFiles/{template}.dir/link.txt").read_text())
    actual = []
    for arg in args:
        if arg.startswith("-Wl,--dependency-file="):
            continue
        if target == "isotropic-tests" and arg.endswith("tests/ValidationMain.cpp.o"):
            continue
        if target == "isotropic-tests" and arg.endswith("tests/ImpactTimingTests.cpp.o"):
            actual.extend([str(h / "IsotropicTests.cpp.o"), str(h / "RigidBodyReference.cpp.o")])
            continue
        if arg == "libjudas_engine.a":
            arg = str(archive)
        elif arg == template:
            arg = str(h / target)
        elif not arg.startswith("-") and not arg.startswith("/") and (b / arg).exists():
            arg = str(b / arg)
        actual.append(arg)
    run(actual)
result = {"status": "compiled and linked; no tests run", "seconds": time.time() - start,
          "commands": commands,
          "sha256": {p.name: hashlib.sha256(p.read_bytes()).hexdigest()
                     for p in h.iterdir() if p.is_file() and
                     (p.name.endswith((".cpp", ".a")) or p.name in
                      ["isotropic-tests", "judas_physics_stress_live_benchmark"])}}
(h / "BUILD.json").write_text(json.dumps(result, indent=2) + "\n")
print(json.dumps({"seconds": result["seconds"], "status": result["status"]}))
