from pathlib import Path
import shlex
import subprocess
import sys
import tempfile

root = Path(__file__).resolve().parent
repo = next(parent for parent in root.parents if (parent / "src/PhysicsWorld.cpp").is_file())
build = repo / ".cache/2d-stage1-build"
target = build / "CMakeFiles/judas_production_fluid_tests.dir"
flags = []
for line in (target / "flags.make").read_text().splitlines():
    key, _, value = line.partition(" = ")
    if key in {"CXX_DEFINES", "CXX_INCLUDES", "CXX_FLAGS"}:
        flags.extend(shlex.split(value))
link = [arg for arg in shlex.split((target / "link.txt").read_text())
        if not arg.startswith("-Wl,--dependency-file=")]
contact = "--contact" in sys.argv
sources = [root / ("SupportedContact.cpp" if contact else "Probe.cpp")]
exact = "--exact" in sys.argv
if exact:
    sources += [repo / "src" / file for file in ("PhysicsWorld.cpp", "physics/WorldConstraintBatch.cpp",
                                                "physics/WorldMk2Solver.cpp", "physics/WorldBodies.cpp")]
with tempfile.TemporaryDirectory(prefix="judas-native-path-probe-") as output:
    objects = []
    for source in sources:
        obj = Path(output) / (source.stem + ".o")
        subprocess.run([link[0], *flags, "-Werror", "-c", source, "-o", obj], cwd=build, check=True)
        objects.append(obj)
    index = next(i for i, arg in enumerate(link) if arg.endswith("ProductionFluidTests.cpp.o"))
    link[index:index + 1] = objects
    exe = Path(output) / "probe"
    link[link.index("-o") + 1] = exe
    subprocess.run(link, cwd=build, check=True)
    subprocess.run([exe], cwd=build, check=True)
