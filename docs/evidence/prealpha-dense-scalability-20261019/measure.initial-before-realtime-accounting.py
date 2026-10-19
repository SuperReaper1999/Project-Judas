#!/usr/bin/env python3
"""Owned, non-production dense-contact measurements; no alternate physics path.

freeze copies the actual binaries/archive and source inputs before an experiment.
run records every command/exit and never represents a timeout as a completed run.
compare checks physical rows before summarising timing. Realtime and synthetic
cadence are deliberately separate; synthetic frame counts are never FPS claims.
"""
from __future__ import annotations

import argparse
import csv
import datetime as dt
import gzip
import hashlib
import json
import math
import os
from pathlib import Path
import platform
import shutil
import signal
import statistics
import subprocess
import sys
import tarfile
import time


OWNED = Path(__file__).resolve().parent
ROOT = OWNED.parents[1]
SCENE = "projects/physics_stress_lab/Scenes/room1024.judas"
PHYSICAL_FIELDS = (
    "step", "bodies", "awake", "sleeping", "candidates", "contacts",
    "impact_events", "impact_queries", "impact_iterations", "impact_peak",
    "trajectory_digest", "max_linear_speed", "max_angular_speed", "nonfinite_fields",
)
ENV_FIELDS = (
    "DISPLAY", "WAYLAND_DISPLAY", "SDL_VIDEODRIVER", "SDL_AUDIODRIVER",
    "XDG_SESSION_TYPE", "XDG_RUNTIME_DIR", "LIBGL_ALWAYS_SOFTWARE",
    "__GL_SYNC_TO_VBLANK", "__NV_PRIME_RENDER_OFFLOAD",
)


def utc():
    return dt.datetime.now(dt.timezone.utc).isoformat()


def sha(path):
    h = hashlib.sha256()
    with Path(path).open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            h.update(block)
    return h.hexdigest()


def write(path, value):
    path = Path(path)
    temporary = path.with_suffix(path.suffix + ".tmp")
    temporary.write_text(json.dumps(value, indent=2, sort_keys=True, allow_nan=False) + "\n")
    temporary.replace(path)


def capture(argv, cwd=ROOT, timeout=20):
    try:
        result = subprocess.run(argv, cwd=cwd, text=True, stdout=subprocess.PIPE,
                                stderr=subprocess.STDOUT, timeout=timeout)
        return {"argv": list(map(str, argv)), "exit": result.returncode, "output": result.stdout}
    except (OSError, subprocess.TimeoutExpired) as error:
        return {"argv": list(map(str, argv)), "exit": None, "error": str(error)}


def git(*args):
    return subprocess.check_output(["git", *args], cwd=ROOT, text=True).strip()


def description(path):
    path = Path(path)
    st = path.stat()
    return {"path": str(path.resolve()), "sha256": sha(path), "bytes": st.st_size,
            "mtime_ns": st.st_mtime_ns, "mode": oct(st.st_mode & 0o777)}


def freeze(args):
    destination = args.output.resolve()
    destination.mkdir(parents=True, exist_ok=False)
    build = args.build.resolve()
    manifest = {
        "schema": "judas.dense.freeze.v1", "utc": utc(), "root": str(ROOT),
        "git_head": git("rev-parse", "HEAD"),
        "git_branch": git("branch", "--show-current"),
        "tracked_status": git("status", "--short", "--untracked-files=no"),
        "artifacts": {}, "source_inputs": {},
        "environment": {key: os.environ.get(key) for key in ENV_FIELDS},
        "platform": platform.platform(),
        "hardware": {
            "cpu": capture(["lscpu"]),
            "memory": capture(["free", "-b"]),
            "gpu": capture(["nvidia-smi", "--query-gpu=name,driver_version,memory.total", "--format=csv,noheader"]),
        },
        "compiler": capture(["c++", "--version"]),
        "notes": [
            "Exact copies; executable mode preserved and source SHA-256 checked.",
            "Source archive contains tracked engine/build inputs, current live/scaling drivers and the actual project.",
            "External downloaded build dependencies are identified by the copied build configuration, not bundled as unrelated assets.",
        ],
    }
    (destination / "bin").mkdir()
    for filename, target in (
        ("judas_physics_stress_live_benchmark", "bin/judas_physics_stress_live_benchmark"),
        ("judas_physics_scaling_diagnostics", "bin/judas_physics_scaling_diagnostics"),
        ("libjudas_engine.a", "libjudas_engine.a"),
        ("judas-build-info-Release.json", "judas-build-info-Release.json"),
        ("CMakeCache.txt", "CMakeCache.txt"),
        ("compile_commands.json", "compile_commands.json"),
    ):
        source = build / filename
        if not source.exists():
            if filename in {"compile_commands.json"}:
                manifest["artifacts"][target] = {"unavailable": True}
                continue
            raise FileNotFoundError(source)
        copied = destination / target
        shutil.copy2(source, copied)
        entry = description(source)
        entry["copy_sha256"] = sha(copied)
        if entry["sha256"] != entry["copy_sha256"]:
            raise RuntimeError(f"copy mismatch: {source}")
        manifest["artifacts"][target] = entry
    inputs = subprocess.check_output([
        "git", "ls-files", "-z", "src", "include", "cmake", "CMakeLists.txt",
        "CMakePresets.json", "tests/PhysicsStressLiveBenchmark.cpp",
        "tests/PhysicsScalingBenchmarks.cpp", "projects/physics_stress_lab",
    ], cwd=ROOT).split(b"\0")
    source_archive = destination / "source-inputs.tar.gz"
    with source_archive.open("wb") as raw:
        with gzip.GzipFile(filename="", mode="wb", fileobj=raw, mtime=0) as compressed:
            with tarfile.open(fileobj=compressed, mode="w") as archive:
                for encoded in sorted(x for x in inputs if x):
                    name = os.fsdecode(encoded)
                    source = ROOT / name
                    if not source.is_file():
                        manifest["source_inputs"][name] = {"missing": True}
                        continue
                    manifest["source_inputs"][name] = {"sha256": sha(source), "bytes": source.stat().st_size}
                    info = archive.gettarinfo(str(source), arcname=name)
                    info.uid = info.gid = 0
                    info.uname = info.gname = ""
                    info.mtime = 0
                    with source.open("rb") as stream:
                        archive.addfile(info, stream)
    manifest["source_archive"] = description(source_archive)
    manifest["fixture"] = description(ROOT / SCENE)
    manifest["finished_utc"] = utc()
    write(destination / "FROZEN.json", manifest)
    print(json.dumps({"snapshot": str(destination), "head": manifest["git_head"],
                      "tracked_status": manifest["tracked_status"], "inputs": len(manifest["source_inputs"]),
                      "manifest_sha256": sha(destination / "FROZEN.json")}, indent=2))


def rows(path):
    return list(csv.DictReader(Path(path).open())) if Path(path).exists() else []


def distribution(values):
    values = sorted(values)
    if not values:
        return {"count": 0}
    return {"count": len(values), "mean": statistics.mean(values),
            "median": statistics.median(values), "p95_floor": values[int(.95 * (len(values)-1))],
            "p99_floor": values[int(.99 * (len(values)-1))], "maximum": values[-1]}


def profile_summary(directory):
    path = directory / "profile-shutdown.json"
    if not path.exists():
        return {"available": False}
    profile = json.loads(path.read_text())
    retained = profile.get("frames", [])
    spikes = profile.get("spikes", [])
    unique = {}
    for frame in retained + spikes:
        unique[frame["id"]] = frame
    fixed = [frame.get("fixed", {}) for frame in unique.values()]
    return {"available": True, "retained_unique_frames": len(unique),
            "cap_frames_in_retained_subset": sum(bool(x.get("cap_reached")) for x in fixed),
            "discarded_seconds_in_retained_subset": sum(x.get("discarded_seconds", 0) for x in fixed),
            "coverage": "Bounded retained profiler history/spikes only; not an all-run cap counter."}


def live_summary(directory, entry, args):
    steps = rows(directory / "steps.csv")
    frames = rows(directory / "frames.csv")
    step_times = [float(row["fixed_ms"]) for row in steps]
    intervals = [float(row["interval_ms"]) for row in frames[1:]]
    quality = directory / "quality.json"
    body_counts = sorted({int(row["bodies"]) for row in steps})
    malformed = [i + 1 for i, row in enumerate(steps)
                 if int(row["step"]) != i + 1 or int(row["nonfinite_fields"]) != 0]
    observed_wall_ms = max((float(row["wall_ms"]) for row in frames), default=0)
    simulation_seconds = len(steps) / 60.0
    entry.update({
        "completed_steps": len(steps), "rendered_frames": len(frames),
        "fixed_ms": distribution(step_times), "frame_interval_ms": distribution(intervals),
        "bodies_observed": body_counts, "nonfinite_or_bad_sequence_rows": malformed,
        "simulated_seconds": simulation_seconds, "observed_application_wall_seconds": observed_wall_ms/1000,
        "simulation_seconds_per_wall_second": simulation_seconds/(observed_wall_ms/1000) if observed_wall_ms else None,
        "final_digest": steps[-1]["trajectory_digest"] if steps else None,
        "worst_step": max(steps, key=lambda row: float(row["fixed_ms"])) if steps else None,
        "profile_retained_state": profile_summary(directory),
        "cadence": "synthetic one-60Hz-step/frame: NOT displayed FPS" if args.mode == "exact" else "measured application frame cadence",
        "endpoint_quality": json.loads(quality.read_text()) if quality.exists() else None,
        "frame_intervals_over_33_333ms": sum(x > 1000/30 for x in intervals),
        "frame_intervals_over_41_667ms": sum(x > 1000/24 for x in intervals),
    })
    entry["blocks_of_30_steps"] = [
        {"first_step": i+1, "last_step": min(i+30, len(step_times)), **distribution(step_times[i:i+30])}
        for i in range(0, len(step_times), 30)
    ]
    entry["valid_completion"] = (
        entry["exit"] == 0 and not entry["timed_out"] and bool(steps) and not malformed
        and quality.exists() and (args.mode != "exact" or len(steps) == args.steps)
    )


def execute(argv, output, wall_limit, root):
    entry = {"argv": list(map(str, argv)), "utc": utc(), "cwd": str(root),
             "timeout_seconds": wall_limit, "timed_out": False}
    with (output / "run.log").open("w") as log:
        command = ["/usr/bin/time", "-v", "-o", str(output / "rusage.log"), *entry["argv"]]
        start = time.monotonic()
        process = subprocess.Popen(command, cwd=root, stdout=log, stderr=subprocess.STDOUT, start_new_session=True)
        entry["process_group"] = process.pid
        write(output / "AS_RUN.json", entry)
        try:
            process.wait(timeout=wall_limit)
        except subprocess.TimeoutExpired:
            entry["timed_out"] = True
            os.killpg(process.pid, signal.SIGTERM)
            try:
                process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                os.killpg(process.pid, signal.SIGKILL)
                process.wait()
        entry["exit"] = process.returncode
        entry["wall_seconds"] = time.monotonic() - start
    entry["finished_utc"] = utc()
    usage = output / "rusage.log"
    entry["maximum_rss_kib"] = None
    if usage.exists():
        for line in usage.read_text().splitlines():
            if "Maximum resident set size" in line:
                entry["maximum_rss_kib"] = int(line.rsplit(":", 1)[1].strip())
    return entry


def run(args):
    snapshot = args.snapshot.resolve()
    frozen = json.loads((snapshot / "FROZEN.json").read_text())
    root = args.root.resolve()
    fixture = root / SCENE
    if sha(fixture) != frozen["fixture"]["sha256"]:
        raise RuntimeError("actual fixture differs from frozen fixture; refusing comparison")
    binary = snapshot / "bin/judas_physics_stress_live_benchmark"
    if sha(binary) != frozen["artifacts"]["bin/judas_physics_stress_live_benchmark"]["copy_sha256"]:
        raise RuntimeError("snapshot executable changed")
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    live = output / "live"
    frames = args.steps if args.mode == "exact" else args.maximum_frames
    cadence = "0.016666667" if args.mode == "exact" else "0"
    argv = [binary, fixture, live, str(args.seconds), str(frames), str(args.swap),
            str(int(args.tour)), str(int(not args.visible)), cadence, str(args.profile), "0", "1"]
    context = {"schema": "judas.dense.run.v1", "snapshot": str(snapshot),
               "snapshot_manifest_sha256": sha(snapshot / "FROZEN.json"),
               "fixture_sha256": sha(fixture), "binary_sha256": sha(binary),
               "source_head": frozen["git_head"], "mode": args.mode,
               "requested_steps": args.steps if args.mode == "exact" else None,
               "requested_wall_seconds": args.seconds, "profile": args.profile,
               "resolution_note": "Runtime default project/window settings; preserve actual runtime renderer log.",
               "environment": {key: os.environ.get(key) for key in ENV_FIELDS}}
    write(output / "CONTEXT.json", context)
    result = execute(argv, output, args.timeout, root)
    live_summary(live, result, args)
    result["context"] = context
    write(output / "RESULTS.json", result)
    print(json.dumps({"output": str(output), "exit": result["exit"], "valid_completion": result["valid_completion"],
                      "completed_steps": result["completed_steps"], "fixed_ms": result["fixed_ms"],
                      "cadence": result["cadence"]}, indent=2))
    return 0 if result["valid_completion"] else 1


def compare(args):
    baseline = args.baseline.resolve()
    candidate = args.candidate.resolve()
    a = json.loads((baseline / "RESULTS.json").read_text())
    b = json.loads((candidate / "RESULTS.json").read_text())
    ar = rows(baseline / "live/steps.csv")
    br = rows(candidate / "live/steps.csv")
    mismatches = []
    mismatch_count = 0
    for index, (left, right) in enumerate(zip(ar, br), 1):
        for field in PHYSICAL_FIELDS:
            if left.get(field) != right.get(field):
                mismatch_count += 1
                if len(mismatches) < 100:
                    mismatches.append({"step": index, "field": field, "baseline": left.get(field), "candidate": right.get(field)})
    exact = len(ar) == len(br) and mismatch_count == 0
    aq = a.get("endpoint_quality")
    bq = b.get("endpoint_quality")
    aq = {k: v for k, v in (aq or {}).items() if k != "observation_ms"}
    bq = {k: v for k, v in (bq or {}).items() if k != "observation_ms"}
    result = {
        "schema": "judas.dense.compare.v1", "utc": utc(),
        "baseline": str(baseline), "candidate": str(candidate),
        "both_completed": bool(a.get("valid_completion")) and bool(b.get("valid_completion")),
        "same_fixture": a["context"]["fixture_sha256"] == b["context"]["fixture_sha256"],
        "same_mode": a["context"]["mode"] == b["context"]["mode"],
        "same_profile_setting": a["context"]["profile"] == b["context"]["profile"],
        "baseline_steps": len(ar), "candidate_steps": len(br),
        "compared_physical_fields": list(PHYSICAL_FIELDS),
        "exact_physical_rows": exact, "physical_mismatch_count": mismatch_count,
        "first_100_mismatches": mismatches, "exact_endpoint_geometry": aq == bq,
        "baseline_fixed_ms": a["fixed_ms"], "candidate_fixed_ms": b["fixed_ms"],
        "baseline_frame_intervals_ms": a["frame_interval_ms"], "candidate_frame_intervals_ms": b["frame_interval_ms"],
        "baseline_wall_seconds": a["wall_seconds"], "candidate_wall_seconds": b["wall_seconds"],
        "timing_note": "Single paired observation, not a universal bound or statistical significance claim. Synthetic cadence is not FPS.",
        "trajectory_note": "Non-identical trajectories require independently justified physical acceptance; this comparison does not silently accept them.",
    }
    result["exact_comparison_pass"] = (result["both_completed"] and result["same_fixture"] and result["same_mode"]
                                       and result["same_profile_setting"] and exact and aq == bq)
    write(args.output.resolve(), result)
    print(json.dumps({k: v for k, v in result.items() if k not in {"first_100_mismatches", "compared_physical_fields"}}, indent=2))
    return 0 if result["exact_comparison_pass"] else 1


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="command", required=True)
    f = commands.add_parser("freeze")
    f.add_argument("--build", type=Path, default=ROOT / "build")
    f.add_argument("--output", type=Path, required=True)
    r = commands.add_parser("run")
    r.add_argument("--snapshot", type=Path, required=True)
    r.add_argument("--root", type=Path, default=ROOT)
    r.add_argument("--output", type=Path, required=True)
    r.add_argument("--mode", choices=["exact", "realtime"], default="exact")
    r.add_argument("--steps", type=int, default=270)
    r.add_argument("--seconds", type=float, default=105)
    r.add_argument("--timeout", type=float, default=150)
    r.add_argument("--maximum-frames", type=int, default=10000000)
    r.add_argument("--swap", type=int, choices=[0, 1], default=0)
    r.add_argument("--tour", action="store_true")
    r.add_argument("--visible", action="store_true")
    r.add_argument("--profile", type=int, choices=[0, 1], default=1)
    c = commands.add_parser("compare")
    c.add_argument("--baseline", type=Path, required=True)
    c.add_argument("--candidate", type=Path, required=True)
    c.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    if args.command == "freeze":
        freeze(args)
        return 0
    if args.command == "run":
        if args.steps < 1 or args.seconds <= 0 or args.timeout <= args.seconds:
            parser.error("steps/seconds must be positive and subprocess timeout must exceed application wall cap")
        return run(args)
    return compare(args)


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, ValueError, RuntimeError, KeyError, subprocess.SubprocessError) as error:
        print(f"dense measurements: {error}", file=sys.stderr)
        raise SystemExit(2)
