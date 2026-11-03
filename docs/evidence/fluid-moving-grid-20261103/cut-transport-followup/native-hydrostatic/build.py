#!/usr/bin/env python3
from pathlib import Path
import argparse
import os
import shlex
import subprocess

parser = argparse.ArgumentParser()
parser.add_argument("--repo", type=Path, default=Path.cwd())
parser.add_argument("--exact", action="store_true")
parser.add_argument("--build", type=Path, default=Path(os.environ.get(
    "JUDAS_BASIN_CMAKE_BUILD", ".cache/2d-stage1-build")))
parser.add_argument("--output", type=Path, default=Path("/tmp/judas-cut-basin/native-controls"))
parser.add_argument("--source", type=Path, action="append")
args = parser.parse_args()
repo = args.repo.resolve()
scratch = Path(__file__).resolve().parent
build = (repo / args.build).resolve()
target = build / "CMakeFiles/judas_production_fluid_tests.dir"
flags = []
for line in (target / "flags.make").read_text().splitlines():
    key, _, value = line.partition(" = ")
    if key in {"CXX_DEFINES", "CXX_INCLUDES", "CXX_FLAGS"}:
        flags.extend(shlex.split(value))
flags.extend(["-Werror", "-I" + str(repo / "tools/prototypes/fluid_volume_grid"),
              "-I" + str(scratch)])
link = [arg for arg in shlex.split((target / "link.txt").read_text())
        if not arg.startswith("-Wl,--dependency-file=")]
sources = [scratch / "BasinMk2.cpp", *(args.source or [scratch / "NativeControls.cpp"])]
if args.exact:
    sources += [repo / "src" / f for f in ("PhysicsWorld.cpp", "physics/WorldConstraintBatch.cpp",
                                          "physics/WorldMk2Solver.cpp", "physics/WorldBodies.cpp")]
objects = []
for source in sources:
    obj = args.output.with_name(args.output.name + "-" + source.stem + ".o").resolve()
    subprocess.run([link[0], *flags, "-c", source.resolve(), "-o", obj], cwd=build, check=True)
    objects.append(obj)
index = next(i for i, arg in enumerate(link) if arg.endswith("ProductionFluidTests.cpp.o"))
link[index:index + 1] = objects
link[link.index("-o") + 1] = args.output.resolve()
subprocess.run(link, cwd=build, check=True)
