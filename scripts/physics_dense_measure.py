#!/usr/bin/env python3
"""Opt-in dense-contact measurements; no alternate physics path.

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
import re
import shutil
import signal
import statistics
import subprocess
import sys
import tarfile
import time


ROOT = Path(__file__).resolve().parents[1]
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


def extra_source_names(args):
    names = []
    for requested in getattr(args, "extra_source", []):
        path = (ROOT / requested).resolve()
        try:
            name = path.relative_to(ROOT).as_posix()
        except ValueError:
            raise RuntimeError(f"extra source is outside repository: {requested}")
        if not path.is_file():
            raise RuntimeError(f"explicit extra source is not a file: {requested}")
        names.append(name)
    return sorted(set(names))


def write_source_archive(path, names):
    inventory = {}
    with path.open("wb") as raw:
        with gzip.GzipFile(filename="", mode="wb", fileobj=raw, mtime=0) as compressed:
            with tarfile.open(fileobj=compressed, mode="w") as archive:
                for name in sorted(set(names)):
                    source = ROOT / name
                    if not source.is_file():
                        inventory[name] = {"missing": True}
                        continue
                    inventory[name] = {"sha256": sha(source), "bytes": source.stat().st_size}
                    info = archive.gettarinfo(str(source), arcname=name)
                    info.uid = info.gid = 0
                    info.uname = info.gname = ""
                    info.mtime = 0
                    with source.open("rb") as stream:
                        archive.addfile(info, stream)
    return inventory, description(path)


def supplement(args):
    """Link additional exact inputs without rewriting any earlier snapshot."""
    snapshot = args.snapshot.resolve()
    original = json.loads((snapshot / "FROZEN.json").read_text())
    destination = args.output.resolve()
    destination.mkdir(parents=True, exist_ok=False)
    names = extra_source_names(args)
    if not names:
        raise RuntimeError("supplement requires explicit --extra-source files")
    inventory, archive = write_source_archive(destination / "additional-source-inputs.tar.gz", names)
    receipt = {"schema": "judas.dense.source_supplement.v1", "utc": utc(),
               "parent_snapshot": str(snapshot), "parent_manifest_sha256": sha(snapshot / "FROZEN.json"),
               "parent_source_archive": original["source_archive"],
               "source_inputs": inventory, "source_archive": archive,
               "note": "Additional source bytes only; the original snapshot is untouched. Parent archive did not include these inputs unless separately listed there."}
    write(destination / "SOURCE_SUPPLEMENT.json", receipt)
    print(json.dumps({"supplement": str(destination), "files": len(inventory),
                      "manifest_sha256": sha(destination / "SOURCE_SUPPLEMENT.json")}, indent=2))


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
            "Source archive contains tracked engine/build inputs, current live/scaling drivers, actual project and explicitly named --extra-source inputs.",
            "Untracked source is not automatically bundled; its inventory is reported separately to avoid including unrelated experiments or assets.",
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
    extras = extra_source_names(args)
    names = [os.fsdecode(encoded) for encoded in inputs if encoded] + extras
    manifest["source_inputs"], manifest["source_archive"] = write_source_archive(destination / "source-inputs.tar.gz", names)
    manifest["explicit_extra_source_inputs"] = extras
    untracked = subprocess.check_output(["git", "ls-files", "--others", "--exclude-standard", "-z",
                                         "src", "include", "cmake", "tests"], cwd=ROOT).split(b"\0")
    manifest["untracked_source_paths_not_included"] = sorted(
        os.fsdecode(name) for name in untracked if name and os.fsdecode(name) not in manifest["source_inputs"])
    manifest["fixture"] = description(ROOT / SCENE)
    manifest["finished_utc"] = utc()
    write(destination / "FROZEN.json", manifest)
    print(json.dumps({"snapshot": str(destination), "head": manifest["git_head"],
                      "tracked_status": manifest["tracked_status"], "inputs": len(manifest["source_inputs"]),
                      "manifest_sha256": sha(destination / "FROZEN.json")}, indent=2))


def rows(path):
    path = Path(path)
    if not path.exists():
        return []
    with path.open(newline="") as stream:
        return list(csv.DictReader(stream))


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


def frozen_timing_contract(frozen, snapshot=None):
    """Read the actual unchanged catch-up limit from the frozen source inputs."""
    archive = Path(frozen["source_archive"]["path"])
    if snapshot is not None and (Path(snapshot) / "source-inputs.tar.gz").is_file():
        archive = Path(snapshot) / "source-inputs.tar.gz"
    if sha(archive) != frozen["source_archive"]["sha256"]:
        raise RuntimeError("frozen timing source archive changed")
    with tarfile.open(archive) as bundle:
        source = bundle.extractfile("src/SimulationTiming.h").read().decode()
    found = re.search(r"kMaxPhysicsStepsPerFrame\s*=\s*([0-9]+)\s*;", source)
    if not found:
        raise RuntimeError("cannot establish frozen per-frame physics catch-up limit")
    if not re.search(r"kFixedTimestep\s*=\s*1(?:\.0)?f\s*/\s*60(?:\.0)?f\s*;", source):
        raise RuntimeError("frozen source no longer has the required unchanged 60 Hz cadence")
    return {"source": "src/SimulationTiming.h", "source_sha256": hashlib.sha256(source.encode()).hexdigest(),
            "maximum_fixed_steps_per_frame": int(found.group(1)),
            "fixed_cadence_hz": 60,
            "note": "This workload preserves the existing 60 Hz contract; per-frame catch-up count is extracted from frozen actual source."}


def live_summary(directory, entry, args, timing=None):
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
    fixed_limit = timing["maximum_fixed_steps_per_frame"] if timing else None
    cap_frames = sum(int(row["fixed_steps"]) == fixed_limit for row in frames) if fixed_limit else None
    frame_step_count_agrees = sum(int(row["fixed_steps"]) for row in frames) == len(steps)
    requested_window_reached = observed_wall_ms >= args.seconds*1000
    entry.update({
        "completed_steps": len(steps), "rendered_frames": len(frames),
        "fixed_ms": distribution(step_times), "frame_interval_ms": distribution(intervals),
        "bodies_observed": body_counts, "nonfinite_or_bad_sequence_rows": malformed,
        "simulated_seconds": simulation_seconds, "observed_application_wall_seconds": observed_wall_ms/1000,
        "simulation_seconds_per_wall_second": simulation_seconds/(observed_wall_ms/1000) if observed_wall_ms else None,
        "realtime_requested_window_reached": requested_window_reached if args.mode == "realtime" else None,
        "catchup_limit_reached_frames_full_csv": cap_frames,
        "frame_and_step_csv_counts_agree": frame_step_count_agrees,
        "catchup_accounting_note": "Every frame with exactly the frozen maximum step count enters the source catch-up-cap branch. Discarded seconds remain available only for bounded retained profiler history; cap reach alone does not imply positive discarded time.",
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
    if args.mode == "realtime" and intervals:
        times = entry["frame_interval_ms"]
        entry["rendered_cadence_observation"] = {
            "mean_output_fps": 1000/times["mean"] if times["mean"] else None,
            "fps_from_median_interval": 1000/times["median"] if times["median"] else None,
            "fps_from_p95_interval": 1000/times["p95_floor"] if times["p95_floor"] else None,
            "fraction_intervals_within_30fps_budget": sum(x <= 1000/30 for x in intervals)/len(intervals),
            "fraction_intervals_within_24fps_budget": sum(x <= 1000/24 for x in intervals)/len(intervals),
            "note": "Measured application frame cadence. These observations do not establish physical correctness or real-time simulation progress by themselves.",
        }
    else:
        entry["rendered_cadence_observation"] = None
    entry["valid_completion"] = (
        entry["exit"] == 0 and not entry["timed_out"] and bool(steps) and not malformed and frame_step_count_agrees
        and quality.exists() and (len(steps) == args.steps if args.mode == "exact" else requested_window_reached)
    )
    entry["capture_completed"] = entry["valid_completion"]
    entry["acceptance_note"] = "Capture completion only. Target24/30 FPS, fixed-cadence simulation progress, physical accuracy and regression qualification require separate review; synthetic runs never certify FPS."


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
    timing = frozen_timing_contract(frozen, snapshot)
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
               "timing_contract": timing,
               "resolution_note": "Runtime default project/window settings; preserve actual runtime renderer log.",
               "environment": {key: os.environ.get(key) for key in ENV_FIELDS}}
    write(output / "CONTEXT.json", context)
    result = execute(argv, output, args.timeout, root)
    live_summary(live, result, args, timing)
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
    allow_query_reduction = getattr(args, "allow_query_reduction", False)
    query_reductions = []
    compared_fields = tuple(field for field in PHYSICAL_FIELDS
                            if not (allow_query_reduction and field == "impact_queries"))
    for index, (left, right) in enumerate(zip(ar, br), 1):
        if allow_query_reduction:
            reduction = int(left["impact_queries"]) - int(right["impact_queries"])
            query_reductions.append(reduction)
            if reduction < 0:
                mismatch_count += 1
                if len(mismatches) < 100:
                    mismatches.append({"step": index, "field": "unexpected_query_increase",
                                       "baseline": left["impact_queries"], "candidate": right["impact_queries"]})
        for field in compared_fields:
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
        "compared_physical_fields": list(compared_fields),
        "allow_query_reduction": allow_query_reduction,
        "impact_queries_removed": sum(query_reductions) if allow_query_reduction else None,
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
    f.add_argument("--extra-source", action="append", default=[], help="Explicit additional source/header/test path; repeat for untracked intended inputs.")
    s = commands.add_parser("supplement")
    s.add_argument("--snapshot", type=Path, required=True)
    s.add_argument("--output", type=Path, required=True)
    s.add_argument("--extra-source", action="append", default=[])
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
    c.add_argument("--allow-query-reduction", action="store_true",
                   help="Allow only fewer impact queries per step; all other work/physical fields and endpoint geometry must still match exactly.")
    args = parser.parse_args()
    if args.command == "freeze":
        freeze(args)
        return 0
    if args.command == "supplement":
        supplement(args)
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
