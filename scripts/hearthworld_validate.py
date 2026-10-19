#!/usr/bin/env python3
"""Run Hearthworld's real application checks, including a fresh-process restore.

The ordinary Application owns resource publication, scripts, fixed steps, UI,
rendering and saves. This runner only provides an isolated user-data directory,
retains raw observations and checks that source/save bytes did not change while
qualifying them. It never rebuilds, changes acceptance thresholds or edits a save.
"""

from __future__ import annotations

import argparse
import datetime
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import time


ROOT = Path(__file__).resolve().parents[1]


def digest(path: Path) -> str:
    result = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            result.update(block)
    return result.hexdigest()


def source_inputs(project: Path, binary: Path) -> dict[str, str]:
    tracked = subprocess.check_output(
        ["git", "ls-files", "-z", "src", "cmake", "CMakeLists.txt", "third_party"],
        cwd=ROOT,
    ).split(b"\0")
    paths = {ROOT / os.fsdecode(name) for name in tracked if name}
    paths.update(path for path in project.parent.rglob("*") if path.is_file()
                 and not any(part in {"Saves", ".cache", "__pycache__"}
                             for part in path.relative_to(project.parent).parts))
    paths.update({Path(__file__).resolve(), ROOT / "tests/HearthworldApplicationTests.cpp",
                  binary})
    return {str(path): digest(path) for path in sorted(paths) if path.is_file()}


def write_json(path: Path, value: object) -> None:
    path.write_text(json.dumps(value, indent=2, sort_keys=True) + "\n")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True,
                        help="New evidence directory; existing results are never overwritten")
    parser.add_argument("--project", type=Path,
                        default=ROOT / "projects/hearthworld/hearthworld.judasproj")
    parser.add_argument("--binary", type=Path,
                        default=ROOT / "build/judas_hearthworld_application_tests")
    parser.add_argument("--timeout", type=float, default=300,
                        help="Wall-clock watchdog per process, not a simulation quality setting")
    parser.add_argument("--performance", action="store_true",
                        help="Also measure a separate representative gameplay process")
    args = parser.parse_args()
    output = args.output.resolve()
    project, binary = args.project.resolve(), args.binary.resolve()
    if not project.is_file() or not binary.is_file():
        parser.error("Build the application observer and provide an existing project first")
    if output.exists():
        parser.error("Use a new --output path to preserve earlier failures and observations")
    output.mkdir(parents=True)
    inputs = source_inputs(project, binary)
    write_json(output / "INPUTS.json", inputs)
    snapshots = output / "observer-source"
    snapshots.mkdir()
    for source in (Path(__file__).resolve(), ROOT / "tests/HearthworldApplicationTests.cpp"):
        target = snapshots / source.name
        shutil.copyfile(source, target)
        if digest(target) != inputs[str(source)]:
            raise RuntimeError("Observer source changed while preserving its reproduction bytes")
    environment = {key: value for key, value in os.environ.items()
                   if not key.startswith("JUDAS_")}
    environment.update(SDL_AUDIODRIVER="dummy", JUDAS_WORLD_STATE="none",
                       XDG_DATA_HOME=str(output / "user-data"))
    runs: list[dict[str, object]] = []
    result: dict[str, object] = {
        "utc": datetime.datetime.now(datetime.timezone.utc).isoformat(),
        "head": subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=ROOT,
                                        text=True).strip(),
        "scope": "Ordinary Hearthworld gameplay and cold-process physical persistence",
        "project": str(project), "binary": str(binary),
        "environment": {key: environment[key] for key in
                        ("SDL_AUDIODRIVER", "JUDAS_WORLD_STATE", "XDG_DATA_HOME")},
        "runs": runs,
    }
    modes = ["write", "read", "controls"] + (["performance"] if args.performance else [])
    failed = False
    saved: dict[str, str] = {}
    try:
        for mode in modes:
            command = [str(binary), str(project), mode, str(output / "application")]
            run_environment = environment.copy()
            run_environment["JUDAS_PROFILE"] = "1" if mode == "performance" else "0"
            started = time.monotonic()
            record: dict[str, object] = {"mode": mode, "argv": command,
                                        "workingDirectory": str(ROOT),
                                        "profilerEnabled": mode == "performance"}
            runs.append(record)
            with (output / (mode + ".log")).open("w") as stream:
                try:
                    completed = subprocess.run(command, cwd=ROOT, env=run_environment,
                                               stdout=stream, stderr=subprocess.STDOUT,
                                               timeout=args.timeout, check=False)
                    record["exit"] = completed.returncode
                    failed = completed.returncode != 0
                except subprocess.TimeoutExpired:
                    record["exit"] = None
                    record["watchdogExpired"] = True
                    failed = True
            record["wallSeconds"] = time.monotonic() - started
            if mode == "write" and not failed:
                saved = {str(path): digest(path) for path in
                         (output / "user-data").rglob("*.save")}
                record["durableSaveFiles"] = saved
                if not saved:
                    record["failure"] = "Writer returned without a durable save file"
                    failed = True
            if mode == "read" and not failed:
                changed = [path for path, expected in saved.items()
                           if not Path(path).is_file() or digest(Path(path)) != expected]
                record["originalSaveBytesUnchanged"] = not changed
                if changed:
                    record["failure"] = "Reader altered original save bytes"
                    record["changedSaveFiles"] = changed
                    failed = True
            write_json(output / "RESULTS.json", result)
            print(mode + (": FAILED" if failed else ": passed"), flush=True)
            if failed:
                break
    finally:
        actual = source_inputs(project, binary)
        changed = sorted(path for path in set(inputs) | set(actual)
                         if inputs.get(path) != actual.get(path))
        result["inputsUnchanged"] = not changed
        result["changedInputs"] = changed
        result["passed"] = not failed and not changed and len(runs) == len(modes)
        write_json(output / "RESULTS.json", result)
    return 0 if result["passed"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
