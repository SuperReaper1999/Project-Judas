#!/usr/bin/env python3
"""Summarize private CCDAudit CSVs without running physics or inferring speedups.

Usage: python3 analyze_ccd.py RUN_FOLDER [--output summary.json] [--markdown summary.md]
An explicit ccd.csv file is also accepted. Only Python's standard library is used.
"""

from __future__ import annotations

import argparse
import csv
import hashlib
import json
import math
from collections import Counter
from pathlib import Path
import sys


KINDS = ("manifold", "touches", "penetration")
ORIGINS = ("unknown", "initial", "reschedule", "domain", "support", "other")
QUERY_FIELDS = (
    "calls", "unique", "repeat", "same_anchor", "cross_anchor", "true", "false",
    "repeat_true", "repeat_false", "points", "repeat_points", "uncertain", "mismatch",
    "untracked", "aborted", "zero_margin", "positive_margin", "repeat_zero_margin",
    "repeat_positive_margin", "native_ns", "repeat_native_ns", "new_native_ns",
)
REQUEST_FIELDS = (
    "calls", "captured", "early_bounds", "uncaptured", "aborted", "hit", "no_impact",
    "previous_pair", "same_input", "same_input_cross_anchor", "same_input_mismatch",
    "same_geometry", "changed_velocity_same_geometry", "changed_interval_same_geometry",
    "changed_motion_anchor_same_geometry", "untracked", "native_inclusive_ns",
)
TRUTH_FIELDS = ("hit", "miss", "hit_true", "hit_false", "miss_true", "miss_false")
NOTES = [
    "Duplicate means an identical directed geometry input was observed earlier in the same step; it does not establish a profitable cache.",
    "Shape contents must be immutable and exactly one world must be observed between Reset calls; world tokens and shape contents are not part of keys.",
    "Input map hash collisions cannot produce matches because all raw key words are compared. Manifold output equality uses two explicit-field 64-bit digests, not retained byte-for-byte outputs.",
    "Per-origin repeats are against the shared per-step history, including other origins; per-origin unique means first observed globally in that origin.",
    "Anchor classification compares with first observation, not the previous call. Different events can share the same anchor time.",
    "Overflow/untracked calls make duplicate counts lower bounds; missing keys are never assumed unique.",
    "Impact same-input compares only the previous completed captured request for a pair; it is not an exhaustive all-request duplicate search.",
    "Timing is instrumented wall time, not CPU cycles. Nested penetration/manifold and Impact scopes are inclusive and must not be summed as independent work.",
    "New native time includes untracked calls as well as newly observed keys. Repeat time is an observed-work category, not a speedup estimate.",
    "No benchmark, engine build, physics test or cache experiment is performed by this analysis script.",
]


def fraction(numerator: int, denominator: int) -> float | None:
    return numerator / denominator if denominator else None


def percentile(values: list[int], percent: float) -> float | None:
    if not values:
        return None
    ordered = sorted(values)
    position = (len(ordered) - 1) * percent / 100
    low, high = math.floor(position), math.ceil(position)
    return ordered[low] + (ordered[high] - ordered[low]) * (position - low)


def distribution(values: list[int], scale: float = 1) -> dict:
    return {
        "samples": len(values),
        "mean": sum(values) / len(values) / scale if values else None,
        "median": percentile(values, 50) / scale if values else None,
        "p95": percentile(values, 95) / scale if values else None,
        "worst": max(values) / scale if values else None,
    }


def integer(value: str | None, path: Path, line: int, field: str) -> int:
    if value is None or not value.strip():
        raise ValueError(f"{path}:{line}: missing value for {field}")
    if value in ("true", "false"):
        return int(value == "true")
    result = int(value)
    if result < 0:
        raise ValueError(f"{path}:{line}: negative counter {field}={value}")
    return result


def query_summary(totals: Counter, prefix: str) -> dict:
    raw = {field: totals[f"{prefix}_{field}"] for field in QUERY_FIELDS}
    calls, repeats, tracked = raw["calls"], raw["repeat"], raw["unique"] + raw["repeat"]
    raw.update({
        "duplicate_fraction_of_all_calls_lower_bound": fraction(repeats, calls),
        "duplicate_fraction_of_tracked_calls": fraction(repeats, tracked),
        "duplicate_fraction_is_complete": raw["untracked"] == 0,
        "cross_anchor_fraction_of_calls_lower_bound": fraction(raw["cross_anchor"], calls),
        "repeat_false_fraction_of_false_calls_lower_bound": fraction(raw["repeat_false"], raw["false"]),
        "repeat_true_fraction_of_true_calls_lower_bound": fraction(raw["repeat_true"], raw["true"]),
        "result_comparison_count": repeats,
        "result_comparison": "two-64-bit-explicit-field-digests" if prefix.endswith("manifold") else "exact-boolean",
        "consistent_observed_results": raw["mismatch"] == 0 and repeats > 0,
        "native_ms": raw["native_ns"] / 1e6,
        "repeat_native_ms": raw["repeat_native_ns"] / 1e6,
        "new_or_untracked_native_ms": raw["new_native_ns"] / 1e6,
        "repeat_native_fraction": fraction(raw["repeat_native_ns"], raw["native_ns"]),
    })
    return raw


def request_summary(totals: Counter, prefix: str) -> dict:
    raw = {field: totals[f"{prefix}_{field}"] for field in REQUEST_FIELDS}
    raw.update({
        "capture_fraction": fraction(raw["captured"], raw["calls"]),
        "no_impact_fraction": fraction(raw["no_impact"], raw["calls"]),
        "same_input_fraction_of_captured": fraction(raw["same_input"], raw["captured"]),
        "changed_velocity_same_geometry_fraction_of_previous_pairs": fraction(raw["changed_velocity_same_geometry"], raw["previous_pair"]),
        "native_inclusive_ms": raw["native_inclusive_ns"] / 1e6,
        "same_input_result_comparison": "exact-raw-double-return-bits",
    })
    return raw


def truth_summary(totals: Counter, prefix: str) -> dict:
    raw = {field: totals[f"{prefix}_{field}"] for field in TRUTH_FIELDS}
    raw["requests"] = raw["hit"] + raw["miss"]
    raw["hit_fraction"] = fraction(raw["hit"], raw["requests"])
    return raw


def row_integrity(row: dict[str, int], fields: set[str]) -> list[str]:
    errors = []

    def equal(label: str, actual: int, expected: int) -> None:
        if actual != expected:
            errors.append(f"{label}: {actual} != {expected}")

    for origin in (None, *ORIGINS):
        for kind in KINDS:
            prefix = f"{origin}_{kind}" if origin else kind
            q = lambda key: row.get(f"{prefix}_{key}", 0)
            equal(f"{prefix} call partition", q("calls"), q("unique") + q("repeat") + q("untracked"))
            equal(f"{prefix} truth partition", q("calls"), q("true") + q("false"))
            equal(f"{prefix} repeat truth partition", q("repeat"), q("repeat_true") + q("repeat_false"))
            equal(f"{prefix} anchor partition", q("repeat"), q("same_anchor") + q("cross_anchor"))
            equal(f"{prefix} native time partition", q("native_ns"), q("repeat_native_ns") + q("new_native_ns"))
        prefix = f"{origin}_impact" if origin else "impact"
        r = lambda key: row.get(f"{prefix}_{key}", 0)
        equal(f"{prefix} capture partition", r("calls"), r("captured") + r("uncaptured"))
        equal(f"{prefix} result partition", r("calls"), r("hit") + r("no_impact"))
        prefix = f"{origin}_local_truth" if origin else "local_truth"
        t = lambda key: row.get(f"{prefix}_{key}", 0)
        equal(f"{prefix} hit partition", t("hit"), t("hit_true") + t("hit_false"))
        equal(f"{prefix} miss partition", t("miss"), t("miss_true") + t("miss_false"))

    for kind, metrics in ((kind, QUERY_FIELDS) for kind in KINDS):
        for field in metrics:
            full = f"{kind}_{field}"
            if full in fields:
                equal(f"{full} origin sum", row[full], sum(row.get(f"{origin}_{full}", 0) for origin in ORIGINS))
    for kind, metrics in (("impact", REQUEST_FIELDS), ("local_truth", TRUTH_FIELDS)):
        for field in metrics:
            full = f"{kind}_{field}"
            if full in fields:
                equal(f"{full} origin sum", row[full], sum(row.get(f"{origin}_{full}", 0) for origin in ORIGINS))

    equal("geometry key count", row["geometry_keys"], sum(row.get(f"{kind}_unique", 0) for kind in KINDS))
    equal("overflow count", row["overflow"], row.get("impact_untracked", 0) + sum(row.get(f"{kind}_untracked", 0) for kind in KINDS))
    if row["geometry_keys"] + row["request_pairs"] > row["key_limit"]:
        errors.append("combined retained keys exceed configured key limit")
    return errors


def analyze(path: Path) -> dict:
    totals = Counter()
    enabled_rows, disabled_rows = [], 0
    errors = []
    seen_steps = set()
    required = {
        "step", "enabled", "timing", "key_limit", "geometry_keys", "request_pairs",
        "overflow", "observer_ns", "clock_calls", "clock_pair_floor_ns",
    }
    for kind in KINDS:
        required.update(f"{kind}_{field}" for field in QUERY_FIELDS)
    required.update(f"impact_{field}" for field in REQUEST_FIELDS)
    required.update(f"local_truth_{field}" for field in TRUTH_FIELDS)
    with path.open(newline="", encoding="utf-8") as handle:
        reader = csv.DictReader(handle)
        columns = reader.fieldnames or []
        if len(columns) != len(set(columns)):
            raise ValueError(f"{path}: duplicate column names")
        missing = required - set(columns)
        if missing:
            raise ValueError(f"{path}: missing required columns: {sorted(missing)}")
        for line, source in enumerate(reader, start=2):
            if None in source:
                raise ValueError(f"{path}:{line}: row has more values than the header")
            row = {field: integer(value, path, line, field) for field, value in source.items()}
            if not row["enabled"]:
                disabled_rows += 1
                continue
            step = row["step"]
            if step in seen_steps:
                errors.append({"step": step, "problem": "duplicate step row; do not concatenate the same run twice"})
            seen_steps.add(step)
            for problem in row_integrity(row, set(columns)):
                errors.append({"step": step, "problem": problem})
            enabled_rows.append(row)
            totals.update(row)
    if not enabled_rows:
        raise ValueError(f"{path}: no enabled observation rows")

    mismatches = sum(totals[f"{kind}_mismatch"] for kind in KINDS)
    request_mismatches = totals["impact_same_input_mismatch"]
    query_aborts = sum(totals[f"{kind}_aborted"] for kind in KINDS)
    impact_aborts = totals["impact_aborted"]
    timing_rows = [row for row in enabled_rows if row["timing"]]
    row_stats = {
        "overflow_rows": sum(row["overflow"] > 0 for row in enabled_rows),
        "max_geometry_keys": max(row["geometry_keys"] for row in enabled_rows),
        "max_request_pairs": max(row["request_pairs"] for row in enabled_rows),
        "max_combined_keys": max(row["geometry_keys"] + row["request_pairs"] for row in enabled_rows),
        "observer_wall_ms": distribution([row["observer_ns"] for row in timing_rows], 1e6),
        "impact_inclusive_wall_ms": distribution([row["impact_native_inclusive_ns"] for row in timing_rows], 1e6),
        "clock_pair_floor_ns_values": sorted({row["clock_pair_floor_ns"] for row in timing_rows}),
    }
    for kind in KINDS:
        row_stats[f"{kind}_calls_per_step"] = distribution([row[f"{kind}_calls"] for row in enabled_rows])
        row_stats[f"{kind}_native_wall_ms"] = distribution([row[f"{kind}_native_ns"] for row in timing_rows], 1e6)
    return {
        "csv": str(path.resolve()),
        "csv_sha256": hashlib.sha256(path.read_bytes()).hexdigest(),
        "observed_steps": len(enabled_rows),
        "first_step": min(seen_steps),
        "last_step": max(seen_steps),
        "disabled_rows_ignored": disabled_rows,
        "timing_rows": len(timing_rows),
        "timing_scope": "instrumented steady-clock wall time; inclusive scopes are not additive",
        "overflow": totals["overflow"],
        "observer_wall_ms_total": totals["observer_ns"] / 1e6,
        "clock_calls": totals["clock_calls"],
        "query_result_mismatches": mismatches,
        "same_impact_input_result_mismatches": request_mismatches,
        "aborted_query_scopes": query_aborts,
        "aborted_impact_scopes": impact_aborts,
        "integrity_errors": errors,
        "observation_consistency": "failed or incomplete" if mismatches or request_mismatches or errors or query_aborts or impact_aborts else "no observed mismatches or accounting errors",
        "queries": {kind: query_summary(totals, kind) for kind in KINDS},
        "requests": request_summary(totals, "impact"),
        "local_truth": truth_summary(totals, "local_truth"),
        "origins": {
            origin: {
                "queries": {kind: query_summary(totals, f"{origin}_{kind}") for kind in KINDS},
                "requests": request_summary(totals, f"{origin}_impact"),
                "local_truth": truth_summary(totals, f"{origin}_local_truth"),
            }
            for origin in ORIGINS
        },
        "per_step": row_stats,
    }


def markdown(report: dict) -> str:
    lines = ["# CCD observation summary", "", "Identical inputs are an investigation lead, not evidence that a cache would be profitable.", ""]

    def percent(value: float | None) -> str:
        return f"{100 * value:.2f}%" if value is not None else "n/a"

    for run in report["runs"]:
        lines.extend([
            f"## {Path(run['csv']).parent.name}", "", f"Source: `{run['csv']}`",
            f"Observed steps: {run['observed_steps']} ({run['first_step']}..{run['last_step']}); timing rows: {run['timing_rows']}.",
            f"Consistency: {run['observation_consistency']}; overflow calls: {run['overflow']}; observer wall time: {run['observer_wall_ms_total']:.3f} ms.", "",
            "| Origin | Query | Calls | Repeats | Repeat fraction* | Cross-anchor | Output mismatch | Native wall ms† | Repeated wall ms† |",
            "|---|---|---:|---:|---:|---:|---:|---:|---:|",
        ])
        for origin in ("all", *ORIGINS):
            queries = run["queries"] if origin == "all" else run["origins"][origin]["queries"]
            for kind, values in queries.items():
                lines.append(f"| {origin} | {kind} | {values['calls']} | {values['repeat']} | {percent(values['duplicate_fraction_of_all_calls_lower_bound'])} | {values['cross_anchor']} | {values['mismatch']} | {values['native_ms']:.3f} | {values['repeat_native_ms']:.3f} |")
        request = run["requests"]
        lines.extend([
            "", "* Fractions are lower bounds when records overflow. Per-origin history includes prior calls from other origins.",
            "† Instrumented wall time. Inclusive scopes overlap and must not be summed.", "",
            f"Impact requests: {request['calls']}; captured: {request['captured']}; no impact: {percent(request['no_impact_fraction'])}; same complete input as previous pair request: {request['same_input']}; returned-time bit mismatches: {request['same_input_mismatch']}.",
            f"Same geometry with changed velocity: {request['changed_velocity_same_geometry']}; changed interval: {request['changed_interval_same_geometry']}; changed motion anchor: {request['changed_motion_anchor_same_geometry']}.",
            f"Local truth cache: {run['local_truth']['hit']} hits / {run['local_truth']['miss']} misses ({percent(run['local_truth']['hit_fraction'])} hits).", "",
        ])
        if run["integrity_errors"]:
            lines.append("Accounting errors: " + json.dumps(run["integrity_errors"]))
    lines.extend(["## Interpretation limits", "", *(f"- {note}" for note in report["notes"]), ""])
    return "\n".join(lines)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("run", type=Path, help="Run directory containing ccd.csv, or an explicit CSV")
    parser.add_argument("--output", type=Path, help="Write JSON here; otherwise print it")
    parser.add_argument("--markdown", type=Path, help="Also write a concise Markdown table here")
    args = parser.parse_args()
    if args.run.is_file():
        paths = [args.run]
    else:
        paths = sorted(args.run.rglob("ccd.csv"))
    if not paths:
        parser.error(f"no ccd.csv found under {args.run}")
    try:
        report = {"schema": "judas-private-ccd-summary-v1", "runs": [analyze(path) for path in paths], "notes": NOTES}
    except (ValueError, OSError) as error:
        print(str(error), file=sys.stderr)
        return 2
    encoded = json.dumps(report, indent=2, allow_nan=False) + "\n"
    if args.output:
        args.output.write_text(encoded, encoding="utf-8")
    else:
        sys.stdout.write(encoded)
    if args.markdown:
        args.markdown.write_text(markdown(report), encoding="utf-8")
    return 2 if any(run["integrity_errors"] or run["query_result_mismatches"] or run["same_impact_input_result_mismatches"] or run["aborted_query_scopes"] or run["aborted_impact_scopes"] for run in report["runs"]) else 0


if __name__ == "__main__":
    raise SystemExit(main())
