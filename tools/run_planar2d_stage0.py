#!/usr/bin/env python3
"""Run the isolated Stage 0 mobility/camera proof and its broken-build control."""
import argparse
import datetime
import hashlib
import json
import os
from pathlib import Path
import platform
import subprocess


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=Path(".cache/planar2d-stage0-20261030"))
    args = parser.parse_args()
    root = Path(__file__).resolve().parent.parent
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    source = root / "tools/prototypes/planar2d_stage0.cpp"
    compiler = os.environ.get("CXX", "g++")
    common = [compiler, "-std=c++17", "-O3", "-Wall", "-Wextra", "-Werror", "-pedantic", "-Isrc", str(source)]
    commands = []
    results = {}
    for name, defines in (("mobility-probe", []), ("missing-rows-control", ["-DJUDAS_PLANAR_PROBE_DISABLE_ROWS"])):
        command = common + defines + ["-o", str(output / name)]
        commands.append(command)
        built = subprocess.run(command, cwd=root, text=True, capture_output=True)
        (output / (name + "-compile.log")).write_text(built.stdout + built.stderr, encoding="utf-8")
        built.check_returncode()
        run = subprocess.run([str(output / name)], cwd=root, text=True, capture_output=True)
        (output / (name + ".log")).write_text(run.stdout + run.stderr, encoding="utf-8")
        if name == "mobility-probe":
            run.check_returncode()
            results["positive"] = json.loads(run.stdout)
        else:
            if run.returncode != 1 or "rows/reduced linear equivalence" not in run.stderr:
                raise RuntimeError("Missing-row negative control did not fail at the expected physical contract")
            results["negative"] = {"exit_code": run.returncode, "diagnostic": run.stderr.strip()}
    inputs = [source, source.parent / "orthographic_stage0.h", Path(__file__).resolve(), root / "src/CameraProjection.h"]
    results.update({
        "utc": datetime.datetime.now(datetime.timezone.utc).isoformat(),
        "base_commit": subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=root, text=True).strip(),
        "compiler": subprocess.check_output([compiler, "--version"], text=True).splitlines()[0],
        "host": platform.platform(),
        "commands": commands,
        "sources": {str(p.relative_to(root)): digest(p) for p in inputs},
        "binaries": {name: digest(output / name) for name in ("mobility-probe", "missing-rows-control")},
        "scope": "Standalone response algebra and existing projection helper; no MK2/world/render integration",
    })
    (output / "RESULTS.json").write_text(json.dumps(results, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(results, indent=2))


if __name__ == "__main__":
    main()
