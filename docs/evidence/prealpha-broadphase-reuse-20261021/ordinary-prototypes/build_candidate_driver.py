#!/usr/bin/env python3
"""Compile/link the private full-two-pass candidate observer; never run workload.

Plan only unless --execute. Uses the frozen sampled-build-v2 driver recipe and
already-built guard-diagnostic core. No production source/header/layout changes.
"""
from pathlib import Path
import argparse
import datetime
import hashlib
import json
import subprocess

PRIVATE = Path(__file__).resolve().parent
HERE = PRIVATE.parent
ROOT = HERE.parents[1]

def sha(path):
    h = hashlib.sha256()
    with Path(path).open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            h.update(chunk)
    return h.hexdigest()

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--output", type=Path, required=True)
parser.add_argument("--execute", action="store_true")
args = parser.parse_args()
out = args.output.resolve()
if out.exists() or not out.is_relative_to(HERE):
    raise SystemExit("fresh private output directory required")
reference = json.loads((HERE / "sampled-build-v2/BUILD.json").read_text())
guard = json.loads((HERE / "guard-diagnostic/RESULT.json").read_text())
core = HERE / "guard-diagnostic/libjudas_engine.a"
assert sha(core) == guard["private_core_sha256"], "diagnostic core differs from tested guard build"
source = PRIVATE / "candidate-driver-source/CandidateLive.cpp"
header = PRIVATE / "candidate-driver-source/OrdinaryCandidateAudit.h"
assert sha(header) == sha(PRIVATE / "diagnostic/OrdinaryCandidateAudit.h"), "audit header differs between TUs"
assert sha(PRIVATE / "candidate-driver-source/MainThreadSampler.h") == sha(HERE / "sampled-build-v2/MainThreadSampler.h")
obj = out / "CandidateLive.cpp.o"
binary = out / "candidate-live"
compile_argv = [str(source) if token.endswith("/sampled-build-v2/SampledLive.cpp")
                else str(obj) if token.endswith("/sampled-build-v2/SampledLive.cpp.o")
                else token for token in reference["compile"]]
compile_argv.insert(1, "-I" + str(source.parent))
link = [str(core) if token == reference["core"]
        else str(obj) if token.endswith("/sampled-build-v2/SampledLive.cpp.o")
        else token for token in reference["link"]]
link[link.index("-o") + 1] = str(binary)
modules = {str(core): sha(core)}
for path, expected in reference["modules"].items():
    if path == reference["core"]:
        continue
    assert sha(path) == expected, "linked module changed: " + path
    modules[path] = expected
report = {"classification": "private exact candidate-equality/hit-rate diagnostic; not timing evidence",
          "execute": args.execute, "core": str(core), "core_sha256": sha(core),
          "source": str(source), "source_sha256": sha(source), "audit_header_sha256": sha(header),
          "reference_build_manifest_sha256": sha(HERE / "sampled-build-v2/BUILD.json"),
          "guard_manifest_sha256": sha(HERE / "guard-diagnostic/RESULT.json"),
          "compile": compile_argv, "link": link, "cwd": reference["cwd"], "modules": modules,
          "notes": ["Original full prediction and post-integration candidate passes retained.",
                    "candidates.csv emits per-step observer counts, then resets; any false certificate throws.",
                    "Same existing physics digest and native NVIDIA backend validation retained.",
                    "No engine rebuild or workload run is performed by this builder."]}
if not args.execute:
    print(json.dumps(report, indent=2))
    raise SystemExit(0)
out.mkdir(parents=True)
for name, argv in [("compile", compile_argv), ("link", link)]:
    with (out / (name + ".log")).open("w") as stream:
        result = subprocess.run(argv, cwd=reference["cwd"], stdout=stream,
                                stderr=subprocess.STDOUT, timeout=180)
    report[name + "_exit"] = result.returncode
    (out / "BUILD.json").write_text(json.dumps(report, indent=2) + "\n")
    if result.returncode:
        raise SystemExit(result.returncode)
report.update(binary=str(binary), binary_sha256=sha(binary),
              finished_utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),
              modules_unchanged=all(sha(path) == expected for path, expected in modules.items()))
(out / "BUILD.json").write_text(json.dumps(report, indent=2) + "\n")
assert report["modules_unchanged"]
print(json.dumps({"binary": str(binary), "binary_sha256": sha(binary), "core_sha256": sha(core)}, indent=2))
