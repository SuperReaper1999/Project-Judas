#!/usr/bin/env python3
"""Build a bounded, test-only CCD replay without editing shipping sources."""
import argparse
import hashlib
import json
import shlex
import subprocess
from pathlib import Path

BASELINE = "934c5d3f0556c920cc7cae8b80dc4677d8cbf87b"
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("variant", nargs="?", default="corrected", choices=("baseline", "corrected"))
parser.add_argument("--build-dir", default="build")
parser.add_argument("--output", required=True, help="Fresh diagnostic output directory")
parser.add_argument("--timeout", type=float, default=35)
args = parser.parse_args()
here = Path(__file__).resolve().parent
repo = next(p for p in here.parents if (p / "src/PhysicsWorld.cpp").is_file())
build = (repo / args.build_dir).resolve()
out = (repo / args.output).resolve()
out.mkdir(parents=True, exist_ok=False)
source = (repo / "src/PhysicsWorld.cpp").read_text()
needle = "++stats.impactEvents;stats.impactSafetyFallback+=result.safetyFallback;"
if source.count(needle) != 1:
    raise RuntimeError("CCD diagnostic insertion no longer matches this recorded source")
progress = '''
            if(stats.impactEvents%256==0){std::fprintf(stderr,"WHITEBOX events=%zu queries=%zu sampling=%zu time=%.17g selected=%u,%u,%u,%u segments=%zu\\n",stats.impactEvents,stats.impactQueries,stats.impactSamplingFallbacks,now,selected[0],selected[1],selected[2],selected[3],bodies[selected[1]].motion.Segments().size());std::fflush(stderr);}'''
(out / "PhysicsWorld.cpp").write_text(source.replace(needle, needle + progress))
header = subprocess.run(["git", "show", BASELINE + ":src/RigidMotion.h"], cwd=repo,
                        stdout=subprocess.PIPE, check=True).stdout if args.variant == "baseline" else (repo / "src/RigidMotion.h").read_bytes()
(out / "RigidMotion.h").write_bytes(header)
(out / "probe.cpp").write_bytes((here / "probe.cpp").read_bytes())
flags = {}
for line in (build / "CMakeFiles/judas_character_lab_performance.dir/flags.make").read_text().splitlines():
    if line.startswith(("CXX_DEFINES =", "CXX_INCLUDES =", "CXX_FLAGS =")):
        key, value = line.split("=", 1)
        flags[key.strip()] = shlex.split(value)
link = shlex.split((build / "CMakeFiles/judas_character_lab_performance.dir/link.txt").read_text())
obj = out / "probe.o"
exe = out / "probe"
compile_args = [link[0]] + flags["CXX_DEFINES"] + flags["CXX_INCLUDES"] + flags["CXX_FLAGS"] + ["-I" + str(repo), "-c", str(out / "probe.cpp"), "-o", str(obj)]
with (out / "build.log").open("w") as log:
    subprocess.run(compile_args, cwd=repo, stdout=log, stderr=subprocess.STDOUT, check=True)
    link = [str(obj) if "CharacterLabPerformance.cpp.o" in token else token for token in link]
    link[link.index("-o") + 1] = str(exe)
    link = [token for token in link if not token.startswith("-Wl,--dependency-file")]
    subprocess.run(link, cwd=build, stdout=log, stderr=subprocess.STDOUT, check=True)
receipt = {"variant": args.variant, "baselineCheckpoint": BASELINE,
           "headerSha256": hashlib.sha256(header).hexdigest(),
           "shippingPhysicsSourceSha256": hashlib.sha256(source.encode()).hexdigest(),
           "fixtureSha256": hashlib.sha256((here / "fixture.json").read_bytes()).hexdigest(),
           "timeoutSeconds": args.timeout, "engineSourceModified": False}
with (out / "run.log").open("w") as log:
    try:
        receipt["exitCode"] = subprocess.run([str(exe), str(here / "fixture.json")], cwd=repo,
                                             stdout=log, stderr=subprocess.STDOUT,
                                             timeout=args.timeout).returncode
        receipt["timedOut"] = False
    except subprocess.TimeoutExpired:
        receipt["exitCode"] = 124
        receipt["timedOut"] = True
(out / "receipt.json").write_text(json.dumps(receipt, indent=2) + "\n")
print(json.dumps(receipt))
raise SystemExit(receipt["exitCode"])
