#!/usr/bin/env python3
"""Summarize existing matched renderer runs; never builds or runs the engine."""
import argparse
import csv
import hashlib
import json
import math
from collections import Counter
from pathlib import Path

from PIL import Image, ImageChops


def sha256(path):
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for part in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(part)
    return digest.hexdigest()


def summary(values):
    ordered = sorted(values)
    assert ordered
    return {
        "samples": len(ordered),
        **{
            name: ordered[max(0, math.ceil(len(ordered) * fraction) - 1)]
            for name, fraction in (("median", .5), ("p95", .95), ("p99", .99), ("max", 1))
        },
    }


def read_csv(path):
    with path.open(newline="", encoding="utf-8") as source:
        return list(csv.DictReader(source))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--raw", type=Path, default=Path(".cache/renderer-v2/final-matched"))
    parser.add_argument("--out", type=Path, default=Path(__file__).resolve().parent)
    args = parser.parse_args()
    args.out.mkdir(parents=True, exist_ok=True)
    manifest = json.loads((args.raw / "RUNS.json").read_text(encoding="utf-8"))
    assert len(manifest["runs"]) == 20
    groups, per_run, images, raw_files = {}, [], [], []
    counts = (
        "draws", "triangles", "instances", "instanced_draws", "particles", "lights",
        "light_overflow", "shadow_passes", "light_allocated_bytes", "shadow_allocated_bytes",
        "instance_upload_high_water_bytes", "uploaded_bytes", "upload_calls", "material_applications",
    )
    metrics = ("cpu_submission_ms", "process_cpu_submission_ms", "outer_frame_ms")
    baseline_images = {}
    for run in manifest["runs"]:
        assert run["exit"] == 0
        case, kind = run["case"], run["binary"]
        name = f"{case}-{run['iteration']}-{kind}"
        folder = args.raw / name
        result = json.loads((folder / "RESULTS.json").read_text(encoding="utf-8"))
        assert result["gpu_timing_valid"] and result["gpu_dropped"] == 0
        assert result["profiler_truncated_frames"] == 0
        rows, gpu_rows = read_csv(folder / "frames.csv"), read_csv(folder / "gpu.csv")
        whole_gpu = [row for row in gpu_rows if row["pass"] == "Renderer V2 whole scene"]
        assert len(rows) == result["workload"]["frames"] == len(whole_gpu)
        assert len({row["frame"] for row in whole_gpu}) == len(whole_gpu)
        assert len(whole_gpu) == result["gpu_samples_observed"]
        assert all(row["gpu_ms"] != "NA" for row in rows)
        key = (case, kind)
        aggregate = groups.setdefault(key, {
            "workload": result["workload"], "build": result["build"], "runs": [],
            "samples": {metric: [] for metric in (*metrics, "gpu_ms")},
            "counters": {field: Counter() for field in counts}, "rss_bytes": [],
            "appearance_owned_bytes": [], "cold_ms": {field: [] for field in result["cold"]},
        })
        assert aggregate["build"] == result["build"]
        assert aggregate["workload"] == result["workload"]
        aggregate["runs"].append(name)
        for metric in metrics:
            aggregate["samples"][metric].extend(float(row[metric]) for row in rows)
        aggregate["samples"]["gpu_ms"].extend(float(row["milliseconds"]) for row in whole_gpu)
        for field in counts:
            aggregate["counters"][field].update(row[field] for row in rows)
        aggregate["rss_bytes"].append(result["process_peak_rss_bytes"])
        aggregate["appearance_owned_bytes"].append(result["appearance_owned_bytes"])
        for field, value in result["cold"].items():
            aggregate["cold_ms"][field].append(value)
        per_run.append({"name": name, "run": run, "result": result})
        captured = sorted(folder.glob("*.png"))
        assert len(captured) == result["workload"]["views"]
        for path in captured:
            comparison_key = (case, path.name)
            reference = baseline_images.setdefault(comparison_key, path)
            with Image.open(reference) as reference_image, Image.open(path) as candidate_image:
                a, b = reference_image.convert("RGB"), candidate_image.convert("RGB")
                assert a.size == b.size
                difference = ImageChops.difference(a, b)
                histogram = difference.histogram()
                total_error = sum((i % 256) * count for i, count in enumerate(histogram))
                extrema = difference.getextrema()
                record = {
                    "run": name, "image": path.name, "reference": reference.relative_to(args.raw).as_posix(),
                    "width": a.width, "height": a.height, "pixel_equivalent": difference.getbbox() is None,
                    "maximum_channel_error": max(high for _, high in extrema),
                    "mean_absolute_channel_error": total_error / (a.width * a.height * 3),
                    "decoded_rgb_sha256": hashlib.sha256(b.tobytes()).hexdigest(),
                }
                assert record["pixel_equivalent"]
                images.append(record)
        for path in sorted(folder.iterdir()):
            if path.is_file():
                raw_files.append({"path": path.relative_to(args.raw).as_posix(),
                                  "bytes": path.stat().st_size, "sha256": sha256(path)})
        log = args.raw / (name + ".log")
        raw_files.append({"path": log.name, "bytes": log.stat().st_size, "sha256": sha256(log)})
    cases = {}
    flat = []
    for case in dict.fromkeys(run["case"] for run in manifest["runs"]):
        case_result = {}
        for kind in "AB":
            group = groups[(case, kind)]
            record = {key: value for key, value in group.items() if key not in ("samples", "counters")}
            record["summary"] = {metric: summary(values) for metric, values in group["samples"].items()}
            record["counters"] = {
                field: [{"value": value, "samples": count} for value, count in sorted(values.items())]
                for field, values in group["counters"].items()
            }
            case_result[kind] = record
            for metric, values in record["summary"].items():
                flat.append({"case": case, "binary": kind, "metric": metric, **values})
        case_result["median_cost_reduction_percent"] = {
            metric: (1 - case_result["B"]["summary"][metric]["median"] /
                     case_result["A"]["summary"][metric]["median"]) * 100
            for metric in (*metrics, "gpu_ms")
        }
        cases[case] = case_result
    output = {
        "format": 1, "raw_root": str(args.raw), "started_utc": manifest["started_utc"],
        "aggregation": "Nearest-rank percentiles of pooled raw samples from the two A or two B runs. CSV values have the observer's default stream precision; original full-precision individual-run summaries are retained in per-run.json.",
        "gpu_scope": "Whole-scene GPU timestamps may include CPU submission starvation; CPU/GPU durations are not additive.",
        "cases": cases, "images": images,
        "checked_runs": len(per_run), "checked_captures": len(images),
        "checked_cpu_frames": sum(len(group["samples"]["cpu_submission_ms"]) for group in groups.values()),
    }
    for name, data in (("SUMMARY.json", output), ("per-run.json", per_run),
                       ("RAW_SHA256.json", raw_files), ("RUNS.json", manifest)):
        (args.out / name).write_text(json.dumps(data, indent=2) + "\n", encoding="utf-8")
    with (args.out / "SUMMARY.csv").open("w", newline="", encoding="utf-8") as target:
        writer = csv.DictWriter(target, ("case", "binary", "metric", "samples", "median", "p95", "p99", "max"))
        writer.writeheader()
        writer.writerows(flat)
    print(f"Verified {len(per_run)} runs, {len(images)} equivalent captures, {output['checked_cpu_frames']} measured frames")


if __name__ == "__main__":
    main()
