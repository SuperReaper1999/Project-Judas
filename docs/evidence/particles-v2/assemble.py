#!/usr/bin/env python3
"""Preserve the bounded Particles V2 qualification; performs file I/O only.

Run from the checkout after qualification. No build, benchmark or GPU command
is launched. Existing raw cache evidence is read without alteration.
"""
import csv
import datetime
import gzip
import hashlib
import io
import json
import platform
from pathlib import Path
import re
import subprocess
import tarfile

CHECKPOINT = "2cd2f6d59d30c986fc5eee01ccc29cc3cceef0d7"
BASELINE = "621d6ed1cea6eefb93d3d1760ab6c262d8c09b41"
HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]
CACHE = ROOT / ".cache/particles-v2"


def sha(data):
    return hashlib.sha256(data).hexdigest()


def load(path):
    return json.loads(path.read_text(encoding="utf-8"))


def write(name, value):
    (HERE / name).write_text(json.dumps(value, indent=2) + "\n", encoding="utf-8")


def counts(path):
    text = path.read_text(encoding="utf-8", errors="replace")
    patterns = [r"(\d+) checks[, ]+(\d+) failures", r"checks=(\d+) failures=(\d+)", r"(\d+) checks PASS"]
    for pattern in patterns:
        found = re.findall(pattern, text)
        if found:
            last = found[-1]
            return {"checks": int(last[0] if isinstance(last, tuple) else last),
                    "failures": int(last[1]) if isinstance(last, tuple) else 0}
    return {"checks": None, "failures": None}


def main():
    raw = {}

    def add(path):
        if path.is_file():
            raw["raw/" + path.relative_to(CACHE).as_posix()] = path.read_bytes()

    text_suffixes = {".json", ".csv", ".log", ".txt", ".md", ".py", ".cpp", ".h", ".test", ".err", ".d"}
    for name in ["measurements-final", "measurements", "checkpoint-gates", "regressions-checkpoint",
                 "application-checkpoint", "application-backpressure", "application-source-checkpoint", "handoff-source-gates", "handoff-fingerprints", "first-units", "ownership-final", "early-collision", "early-gpu",
                 "cpu-review", "cold-birth", "mesa-minimum", "portable-compile", "final-fingerprints"]:
        for path in sorted((CACHE / name).rglob("*")):
            if path.suffix in text_suffixes:
                add(path)
    for path in sorted(CACHE.glob("*.log")):
        add(path)
    # Preserve the CI failure log, not its extracted artifact/cache tree.
    for path in sorted((CACHE / "ci").glob("*")):
        if path.suffix in text_suffixes:
            add(path)
    for name in ["run_benchmarks_final.py", "run_regressions_checkpoint.py", "run_final_application.py", "qualify_sdk_delivery.py", "qualify_sdk_delivery_final.py",
                 "baseline/PROVENANCE.json", "baseline/OBSERVER_BUILD.json", "baseline/ParticlesV2Benchmark.d",
                 "baseline/observer-build.log", "baseline/judas-build-info-Release.json", "pre-spawn/gpu-source.tgz"]:
        add(CACHE / name)
    for folder in ["delivery", "delivery-final-source"]:
        for path in sorted((CACHE / folder).glob("*")):
            if path.suffix in text_suffixes | {".png", ".trace", ".judas"}:
                add(path)
    for name in ["BUILD_PROVENANCE.json", "DEPENDENCIES.json", "RUNTIME_REQUIREMENTS.txt", "judas-package.txt", "game.judasproj"]:
        add(CACHE / "delivery/SurfaceWorkshop-Linux" / name)
    for name in ["editor/run.log", "editor/shot.edit.png", "editor/shot.play.png", "editor-stress/run.log", "editor-stress/shot.play.png"]:
        add(CACHE / name)
    for path in sorted((CACHE / "measurements-final").glob("*/*.png")):
        add(path)
    for path in sorted((CACHE / "application-checkpoint/cold").glob("*.png")):
        add(path)
    for name in ["tests/ParticlesV2Benchmark.cpp", "tests/CpuParticlesTests.cpp", "tests/ParticleCollisionTests.cpp",
                 "tests/GpuParticlesTests.cpp", "tests/ParticleWorkshopApplicationTests.cpp", "tools/benchmarks/BuildParticlesV2Baseline.py"]:
        raw["reproduction/" + name] = subprocess.check_output(["git", "show", CHECKPOINT + ":" + name], cwd=ROOT)

    runs = load(CACHE / "measurements-final/RUNS.json")
    assert len(runs) == 13 and all(row["exit"] == 0 for row in runs)
    baseline_receipt = load(CACHE / "baseline/OBSERVER_BUILD.json")
    assert baseline_receipt["observer_sha256"] == sha(raw["reproduction/tests/ParticlesV2Benchmark.cpp"])
    performance = []
    for run in runs:
        result = load(CACHE / "measurements-final" / run["name"] / "RESULTS.json")
        assert result["gpu_timing_valid"] and result["gpu_samples"] == 240 and result["gpu_dropped"] == 0
        assert result["proof"]["actual_live"] == result["proof"]["expected_live"]
        performance.append({"name": run["name"], "command": run["argv"], "binary_sha256": run["binary_sha256"], **result})
    image_controls = []
    for a, b in [("baseline-100k-1", "cpu-100k-1"), ("baseline-100k-2", "cpu-100k-2"),
                 ("baseline-100k-2", "baseline-100k-2-repeat"), ("gpu-100k-2", "gpu-100k-2-repeat"),
                 ("cpu-collision-1k", "gpu-collision-1k")]:
        for image in ["main.png", "secondary.png"]:
            left = CACHE / "measurements-final" / a / image
            right = CACHE / "measurements-final" / b / image
            if left.is_file() and right.is_file():
                image_controls.append({"left": a + "/" + image, "right": b + "/" + image,
                                       "png_bytes_identical": left.read_bytes() == right.read_bytes(),
                                       "left_sha256": sha(left.read_bytes()), "right_sha256": sha(right.read_bytes())})
    write("PERFORMANCE.json", {"implementation_checkpoint": CHECKPOINT, "baseline": BASELINE,
        "scope": "Real fixed-step particle simulation, CPU presentation/GPU view preparation and draw submission at 1920x1080; not a complete engine/game frame.",
        "runs": performance, "capture_controls": image_controls})
    fields = ["name", "backend", "case", "count", "views", "frames", "cpu_median_ms", "cpu_p95_ms", "cpu_p99_ms", "cpu_max_ms",
              "gpu_median_ms", "gpu_p95_ms", "gpu_p99_ms", "gpu_max_ms", "simulation_median_ms", "draw_median_ms", "rss_bytes",
              "gpu_owned_bytes", "contacts", "query_failures", "snapshot_capture_ms", "first_simulation_ms", "first_draw_completed_ms"]
    with (HERE / "PERFORMANCE.csv").open("w", newline="", encoding="utf-8") as stream:
        writer = csv.DictWriter(stream, fields, lineterminator="\n")
        writer.writeheader()
        for run in performance:
            summary, proof = run["summary"], run["proof"]
            row = {"name": run["name"], **{k: run["workload"][k] for k in ["backend", "case", "count", "views", "frames"]}}
            for kind, key in [("cpu", "complete_cpu_ms"), ("gpu", "gpu_ms")]:
                row.update({f"{kind}_{metric}_ms": summary[key][metric] for metric in ["median", "p95", "p99", "max"]})
            row.update(simulation_median_ms=summary["simulation_ms"]["median"], draw_median_ms=summary["presentation_draw_ms"]["median"],
                rss_bytes=run["process_peak_rss_bytes"], gpu_owned_bytes=proof.get("gpu_owned_bytes"), contacts=proof.get("contacts"),
                query_failures=proof.get("query_failures"), snapshot_capture_ms=run["cold"]["snapshot_capture_ms_included_in_resource_setup"],
                first_simulation_ms=run["cold"]["first_simulation_ms"], first_draw_completed_ms=run["cold"]["first_draw_completed_ms"])
            writer.writerow(row)

    index = {name: {"bytes": len(data), "sha256": sha(data)} for name, data in sorted(raw.items())}
    write("ARCHIVE_CONTENTS.json", index)
    archive = HERE / "raw-evidence.tar.gz"
    # Fixed metadata gives an identical archive when the input evidence is unchanged.
    with archive.open("wb") as file:
        with gzip.GzipFile(filename="", fileobj=file, mode="wb", mtime=0) as compressed:
            with tarfile.open(mode="w", fileobj=compressed) as tar:
                entries = {**raw, "RAW_SHA256SUMS.json": (json.dumps(index, indent=2) + "\n").encode()}
                for name, data in sorted(entries.items()):
                    info = tarfile.TarInfo(name)
                    info.size = len(data)
                    info.mode = 0o644
                    tar.addfile(info, io.BytesIO(data))
    assert archive.stat().st_size < 5 * 1024 * 1024
    gate = load(CACHE / "checkpoint-gates/RESULTS.json")
    qualifications = []
    for row in gate["tests"]:
        log = CACHE / "checkpoint-gates" / (row["name"] + ".log")
        qualifications.append({"name": row["name"], "exit": row["exit"], "binary_sha256": row["binarySha256"],
                               "seconds": row["seconds"], **counts(log)})
    regressions = load(CACHE / "regressions-checkpoint/RESULTS.json")
    for number, row in enumerate(regressions):
        log = CACHE / "regressions-checkpoint" / f"{number}-{Path(row['argv'][0]).name}.log"
        qualifications.append({"name": Path(row["argv"][0]).name, "argv": row["argv"], "exit": row["exit"],
                               "seconds": row["seconds"], **counts(log)})
    cpu_info = Path("/proc/cpuinfo").read_text() if Path("/proc/cpuinfo").exists() else ""
    cpu_match = re.search(r"model name\s*:\s*(.*)", cpu_info)
    qualification = {"schema": 1, "assembled_utc": datetime.datetime.now(datetime.timezone.utc).isoformat(),
        "implementation_checkpoint": CHECKPOINT, "baseline": BASELINE,
        "final_delivery_checkpoint": None,
        "follow_up_status": "Independent small-pool controls pass after Mesa padding, but the first integrated NVIDIA run retains a capacity-one admission assertion failure. GPU owner is investigating. A separate lab save/backpressure review correction is also in progress. Final integrated follow-up commit/qualification is owned by root and is not assigned a hash by this record.",
        "tested_build_identity": gate["expectedSourceIdentity"], "core_gate_passed": gate["passed"],
        "core_gate_invocations": len(gate["tests"]), "regression_invocations": len(regressions), "tests": qualifications,
        "cold_process_workshop": load(CACHE / "application-checkpoint/RESULTS.json"),
        "sdk_workflow": load(CACHE / "delivery/RESULTS.json"), "delivery": load(CACHE / "delivery/DELIVERY.json"),
        "relocation_proof": load(CACHE / "delivery/RELOCATION_PROOF.json"),
        "portability_syntax_only": load(CACHE / "portable-compile/final/receipt.json"),
        "mesa_follow_up": {"native_nvidia": counts(CACHE / "mesa-minimum/nvidia-fixed.log"),
            "software_mesa": counts(CACHE / "mesa-minimum/mesa-fixed.log"),
            "first_red": counts(CACHE / "mesa-minimum/mesa-first-red.log"),
            "integrated_first_nvidia": counts(CACHE / "mesa-minimum/integrated/nvidia.log"),
            "integrated_first_mesa": counts(CACHE / "mesa-minimum/integrated/mesa.log"),
            "scope": "Independent production storage-block reflection and padding controls; not Windows/Android runtime qualification."},
        "machine_observed_at_assembly": {"platform": platform.platform(), "cpu": cpu_match.group(1) if cpu_match else None,
            "memory_total": Path("/proc/meminfo").read_text().splitlines()[0] if Path("/proc/meminfo").exists() else None,
            "note": "CPU/memory inspected while assembling evidence; run-time graphics/compiler/platform values remain in immutable raw results."},
        "benchmark_runs": len(runs), "measured_frames_per_run": 240, "warmup_frames": 60,
        "all_benchmark_gpu_samples_valid": True, "raw_archive_sha256": sha(archive.read_bytes()),
        "raw_archive_bytes": archive.stat().st_size, "raw_file_count": len(raw),
        "advanced_fluids": "unchanged and locked", "human_acceptance": "not established by this evidence",
        "limitations": ["100k performance uses visual particles, not 100k rigid bodies or a moving full-world geometry snapshot.",
            "Collision performance uses 13 fixed native boxes and one cold snapshot; no MK2 solver stepping or moving-geometry capture cost is in frame samples.",
            "One-view GPU elapsed intervals increase versus CPU particles even though CPU submission cost falls sharply.",
            "CPU/GPU five-second collision paths are not asserted bit-identical; 1k contact totals differ and independent known-answer trajectory/surface tests define correctness.",
            "Timers are asynchronous GPU intervals that include submission bubbles, not pure shader execution times. No per-frame full-state readback or finish occurs.",
            "Short, serial runs on one shared desktop; no claim of isolated long-session percentile stability or universal platform performance.",
            "Android guard and Emscripten syntax controls are not device/browser runtime qualification; Windows runtime remains untested for this change."]}
    followup = HERE / "FOLLOWUP.json"
    if followup.exists():
        qualification["follow_up"] = load(followup)
        qualification["final_delivery_checkpoint"] = qualification["follow_up"]["qualified_source_checkpoint"]
        qualification["follow_up_status"] = qualification["follow_up"]["status"]
    write("QUALIFICATION.json", qualification)
    sums = []
    for path in sorted(HERE.iterdir()):
        if path.is_file() and path.name != "SHA256SUMS":
            sums.append(sha(path.read_bytes()) + "  " + path.name)
    (HERE / "SHA256SUMS").write_text("\n".join(sums) + "\n", encoding="utf-8")
    print(f"Preserved {len(raw)} raw files; {archive.stat().st_size} compressed bytes; {len(performance)} benchmark runs.")


if __name__ == "__main__":
    main()
