#!/usr/bin/env python3
"""Build one private overlay and its candidate oracle; execution is opt-in."""
from pathlib import Path
import argparse
import datetime
import hashlib
import json
import shlex
import shutil
import subprocess
import time

parser = argparse.ArgumentParser()
parser.add_argument("--core", type=Path, required=True)
parser.add_argument("--core-sha256", required=True)
parser.add_argument("--variant", choices=["original", "prediction-reuse", "combined",
                                        "empty-manifold", "diagnostic", "broken-always-reuse"],
                    required=True)
parser.add_argument("--output", type=Path, required=True)
parser.add_argument("--run", action="store_true")
args = parser.parse_args()
root = Path("/home/conner/Documents/GitHub/Project-Judas")
work = Path(__file__).resolve().parent
build = root / "build"
out = args.output.resolve()
out.mkdir(parents=True, exist_ok=False)
core_input = args.core.resolve()

def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()

assert sha(core_input) == args.core_sha256, "input core differs from requested frozen core"
flags = {}
for line in (build / "CMakeFiles/judas_engine.dir/flags.make").read_text().splitlines():
    if " = " in line:
        key, value = line.split(" = ", 1)
        flags[key] = shlex.split(value)
records = []

def command(argv, name, cwd=root, expect=0):
    argv = list(map(str, argv))
    started = time.monotonic()
    stamp = datetime.datetime.now(datetime.timezone.utc).isoformat()
    with (out / (name + ".log")).open("w") as log:
        result = subprocess.run(argv, cwd=cwd, stdout=log, stderr=subprocess.STDOUT,
                                timeout=180)
    records.append({"name": name, "argv": argv, "cwd": str(cwd),
                    "exit": result.returncode, "expected_exit": expect,
                    "seconds": time.monotonic() - started, "utc": stamp})
    (out / "COMMANDS.json").write_text(json.dumps(records, indent=2) + "\n")
    if expect == "nonzero":
        assert result.returncode != 0, name + " negative control unexpectedly passed"
    else:
        assert result.returncode == expect, name + " failed"

common = ["/usr/bin/c++", *flags["CXX_DEFINES"], "-I" + str(work / "diagnostic"),
          "-I" + str(root / "src/physics"), *flags["CXX_INCLUDES"], *flags["CXX_FLAGS"]]
source = work / args.variant / "PhysicsWorld.cpp"
obj = out / "PhysicsWorld.cpp.o"
command([*common, "-c", source, "-o", obj], "overlay-compile")
core = out / "libjudas_engine.a"
shutil.copy2(core_input, core)
members = subprocess.check_output(["ar", "t", core], text=True).splitlines()
assert members.count("PhysicsWorld.cpp.o") == 1
command(["ar", "r", core, obj], "overlay-archive")
fixture = work / "OrdinaryCandidateGuardTests.cpp"
fixture_obj = out / "fixture.o"
command([*common, "-c", fixture, "-o", fixture_obj], "fixture-compile")
link = shlex.split((build / "CMakeFiles/judas_support_guess_ownership_tests.dir/link.txt").read_text())
closure = [str(core) if value == "libjudas_engine.a" else value for value in link
           if value.endswith((".a", ".so")) or value.startswith("-l")]
command(["/usr/bin/c++", "-O3", "-DNDEBUG", fixture_obj, "-o", out / "guard", *closure],
        "fixture-link", build)
if args.run:
    command([out / "guard"], "fixture-run",
            expect="nonzero" if args.variant in {"original", "empty-manifold", "diagnostic", "broken-always-reuse"} else 0)
report = {"classification": "private source control; not a production change",
          "variant": args.variant, "input_core": str(core_input),
          "input_core_sha256": args.core_sha256,
          "private_core_sha256": sha(core), "fixture_sha256": sha(fixture),
          "source_sha256": sha(source), "binary_sha256": sha(out / "guard"),
          "executed": args.run, "commands": records}
(out / "RESULT.json").write_text(json.dumps(report, indent=2) + "\n")
assert sha(core_input) == args.core_sha256
print(json.dumps(report, indent=2))
