#!/usr/bin/env python3
"""Link the consumer observer against an already-built Release Judas tree."""
import argparse, pathlib, shlex, subprocess

p = argparse.ArgumentParser()
p.add_argument("--build", type=pathlib.Path, default=pathlib.Path("build"))
p.add_argument(
    "--output", type=pathlib.Path, default=pathlib.Path("/tmp/lunar-island-observer")
)
a = p.parse_args()
repo = pathlib.Path(__file__).resolve().parents[3]
build = a.build.resolve()
target = build / "CMakeFiles/judas.dir"
flags = {}
for line in (target / "flags.make").read_text().splitlines():
    if " = " in line:
        k, v = line.split(" = ", 1)
        flags[k] = shlex.split(v)
obj = a.output.with_suffix(".o").resolve()
source = pathlib.Path(__file__).with_name("ApplicationTests.cpp")
subprocess.run(
    [
        "/usr/bin/c++",
        *flags["CXX_DEFINES"],
        *flags["CXX_INCLUDES"],
        *flags["CXX_FLAGS"],
        "-I" + str(repo / "third_party/nlohmann"),
        "-c",
        str(source),
        "-o",
        str(obj),
    ],
    check=True,
)
args = shlex.split((target / "link.txt").read_text())
args = [x for x in args if not x.startswith("-Wl,--dependency-file=")]
args = [str(obj) if x.endswith("/src/main.cpp.o") else x for x in args]
args[args.index("-o") + 1] = str(a.output.resolve())
subprocess.run(args, cwd=build, check=True)
