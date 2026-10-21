#!/usr/bin/env python3
"""Compare complete 900-step physical/work records; never starts workloads."""
import argparse
import json
from pathlib import Path
import subprocess
import sys


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("root", type=Path)
    parser.add_argument("--reference", default="sampled900")
    parser.add_argument("--runs", nargs="+", default=["disabled900", "observed900", "unobserved900"])
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    output = args.output or args.root / "sampler-comparisons-all900"
    output.mkdir(parents=True, exist_ok=True)
    reference = args.root / args.reference / "steps.csv"
    helper = Path(__file__).with_name("compare_steps.py")
    if not reference.is_file():
        parser.error(f"reference missing: {reference}")
    results = []
    for name in args.runs:
        path = args.root / name / "steps.csv"
        if not path.is_file():
            results.append({"run": name, "status": "missing", "path": str(path)})
            continue
        target = output / f"{name}.json"
        run = subprocess.run([sys.executable, str(helper), str(reference), str(path),
                              "--start", "1", "--end", "900", "--output", str(target)],
                             capture_output=True, text=True, check=False)
        if run.returncode:
            results.append({"run": name, "status": "comparison_error", "stderr": run.stderr})
            continue
        report = json.loads(target.read_text())
        complete = report["complete_window_physical_and_work_parity"]
        results.append({"run": name, "status": "complete_exact_match" if complete else "partial_or_different",
                        "report": str(target), "common_steps": report["selected_counts"]["common"],
                        "digest_mismatch_count": len(report["digest_mismatches"]),
                        "complete_physical_and_work_parity": complete,
                        "fixed_ms": report["timings_ms"]["fixed_ms"]})
    summary = {"schema": "judas.all900-observer-comparison.v1", "reference": str(reference),
               "actual_steps_inclusive": [1, 900], "results": results,
               "all_requested_runs_complete_and_exact": bool(results) and all(
                   row["status"] == "complete_exact_match" for row in results),
               "notes": ["Missing or incomplete files are explicit; rerun after outstanding measurements finish.",
                         "All 900 per-step physical digests, velocity maxima, nonfinite counts and workload counters are compared.",
                         "Timing differences do not establish observer overhead without matched build/hardware/load/cadence context."]}
    (output / "summary.json").write_text(json.dumps(summary, indent=2) + "\n")
    print(json.dumps(summary, indent=2))


if __name__ == "__main__":
    main()
