#!/usr/bin/env python3
"""Compare frozen baseline and diagnostic steps.csv without running workloads."""

import argparse
import csv
from decimal import Decimal, InvalidOperation
import hashlib
import json
import math
from pathlib import Path
import statistics


COUNTERS = ("bodies", "awake", "sleeping", "candidates", "contacts", "impact_events",
            "impact_queries", "impact_iterations", "impact_peak", "nonfinite_fields")
TIMES = ("fixed_ms", "total_physics_ms", "broadphase_ms", "narrowphase_ms", "solver_ms")
PHYSICAL = ("max_linear_speed", "max_angular_speed", "nonfinite_fields")


def read(path):
    rows = {}
    with path.open(newline="", encoding="utf-8") as stream:
        reader = csv.DictReader(stream)
        required = {"step", "trajectory_digest", "fixed_ms"}
        if not required.issubset(reader.fieldnames or ()):
            raise ValueError(f"{path}: missing required columns {sorted(required - set(reader.fieldnames or ())) }")
        columns = reader.fieldnames
        for row in reader:
            step = int(row["step"])
            if step in rows:
                raise ValueError(f"{path}: duplicate step {step}")
            if not row["trajectory_digest"].strip():
                raise ValueError(f"{path}: empty digest at step {step}")
            rows[step] = row
    return rows, columns


def percentile(values, q):
    index = (len(values) - 1) * q
    lower = int(index)
    upper = min(lower + 1, len(values) - 1)
    return values[lower] + (values[upper] - values[lower]) * (index - lower)


def stats(values):
    finite = sorted(value for value in values if math.isfinite(value))
    result = {"count": len(values), "finite_count": len(finite), "nonfinite_count": len(values) - len(finite)}
    if finite:
        result.update(mean=statistics.fmean(finite), median=statistics.median(finite),
                      p95=percentile(finite, .95), worst=max(finite), total=math.fsum(finite))
    return result


def equivalent(left, right):
    try:
        return Decimal(left) == Decimal(right)
    except InvalidOperation:
        return left == right


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("baseline", type=Path)
    parser.add_argument("diagnostic", type=Path)
    parser.add_argument("--start", type=int, default=300, help="Inclusive ACTUAL step number")
    parser.add_argument("--end", type=int, default=900, help="Inclusive ACTUAL step number")
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    if args.start > args.end:
        parser.error("--start exceeds --end")
    baseline, baseline_columns = read(args.baseline)
    diagnostic, diagnostic_columns = read(args.diagnostic)
    select = lambda rows: {step for step in rows if args.start <= step <= args.end}
    left, right = select(baseline), select(diagnostic)
    common = sorted(left & right)
    missing_baseline, missing_diagnostic = sorted(right - left), sorted(left - right)
    expected = set(range(args.start, args.end + 1))
    digest_differences = [{"step": step, "baseline": baseline[step]["trajectory_digest"],
                           "diagnostic": diagnostic[step]["trajectory_digest"]}
                          for step in common if baseline[step]["trajectory_digest"].strip().lower() != diagnostic[step]["trajectory_digest"].strip().lower()]
    report = {"schema": "judas.matched-step-comparison.v1", "actual_step_window_inclusive": [args.start, args.end],
              "files": {label: {"path": str(path), "sha256": hashlib.sha256(path.read_bytes()).hexdigest()}
                        for label, path in (("baseline", args.baseline), ("diagnostic", args.diagnostic))},
              "selected_counts": {"baseline": len(left), "diagnostic": len(right), "common": len(common)},
              "missing_in_baseline": missing_baseline, "missing_in_diagnostic": missing_diagnostic,
              "expected_window_missing_in_both": sorted(expected - (left | right)),
              "digest_parity_on_common_steps": bool(common) and not digest_differences,
              "complete_window_digest_parity": bool(common) and not digest_differences and left == right == expected,
              "digest_mismatches": digest_differences, "workload_counters": {}, "physical_observations": {}, "timings_ms": {},
              "notes": ["Actual step numbers differ from sampler last-completed-step tags; choose ranges deliberately.",
                        "Timing statistics use identical common step IDs; p95 uses linear interpolation at (N-1)*0.95.",
                        "Profiler categories can be inclusive and overlap; do not sum category timings.",
                        "Digest/workload parity detects observer interference; timing differences alone do not establish causation or significance.",
                        "Complete-window parity requires every requested step in both files; partial overlap is separately reported."]}
    for field in COUNTERS:
        if field not in baseline_columns or field not in diagnostic_columns:
            report["workload_counters"][field] = {"available_in_both": False}
            continue
        mismatches = [{"step": step, "baseline": baseline[step][field], "diagnostic": diagnostic[step][field]}
                      for step in common if not equivalent(baseline[step][field], diagnostic[step][field])]
        report["workload_counters"][field] = {"available_in_both": True, "parity": bool(common) and not mismatches,
                                                   "mismatches": mismatches}
    for field in TIMES:
        if field not in baseline_columns or field not in diagnostic_columns:
            report["timings_ms"][field] = {"available_in_both": False}
            continue
        a = [float(baseline[step][field]) for step in common]
        b = [float(diagnostic[step][field]) for step in common]
        sa, sb = stats(a), stats(b)
        report["timings_ms"][field] = {"available_in_both": True, "baseline": sa, "diagnostic": sb,
                                             "paired_diagnostic_minus_baseline": stats([y - x for x, y in zip(a, b)]),
                                             "ratios_diagnostic_over_baseline": {key: sb[key] / sa[key] if sa.get(key) else None for key in ("mean", "median", "p95", "worst", "total")}}
    for field in PHYSICAL:
        if field not in baseline_columns or field not in diagnostic_columns:
            report["physical_observations"][field] = {"available_in_both": False}
            continue
        mismatches = [{"step": step, "baseline": baseline[step][field], "diagnostic": diagnostic[step][field]}
                      for step in common if not equivalent(baseline[step][field], diagnostic[step][field])]
        report["physical_observations"][field] = {"available_in_both": True, "parity": bool(common) and not mismatches,
                                                  "mismatches": mismatches}
    report["available_workload_counter_count"] = sum(entry["available_in_both"] for entry in report["workload_counters"].values())
    report["all_available_workload_counters_match"] = bool(common) and all(
        entry.get("parity", True) for entry in report["workload_counters"].values())
    report["all_physical_observations_present_and_match"] = bool(common) and all(
        entry.get("parity", False) for entry in report["physical_observations"].values())
    report["complete_window_physical_and_work_parity"] = report["complete_window_digest_parity"] and report[
        "all_available_workload_counters_match"] and report["all_physical_observations_present_and_match"] and all(
            entry["available_in_both"] for entry in report["workload_counters"].values())
    encoded = json.dumps(report, indent=2, allow_nan=False) + "\n"
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(encoded)
        print(json.dumps({"output": str(args.output), "complete_window_digest_parity": report["complete_window_digest_parity"],
                          "common_steps": len(common), "digest_mismatches": len(digest_differences),
                          "all_available_workload_counters_match": report["all_available_workload_counters_match"]}, indent=2))
    else:
        print(encoded, end="")


if __name__ == "__main__":
    main()
