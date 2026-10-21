#!/usr/bin/env python3
"""Summarize bounded response/event observers without interpreting small deltas as disposable.

Usage: python3 summarize.py RUNFOLDER [--start-step N] [--end-step N] [--output FILE]
Reads only existing CSVs; stdout is JSON unless --output is given. No simulation,
build, source changes, timing adjustment or physical acceptance judgement occurs.
"""
import argparse
from collections import Counter, defaultdict
import csv
import hashlib
import json
import math
from pathlib import Path
import statistics


def number(value):
    if value in (None, ""):
        return 0
    try:
        return int(value)
    except ValueError:
        value = float(value)
        if not math.isfinite(value):
            raise ValueError("nonfinite diagnostic CSV value")
        return value


def percentile(values, fraction):
    if not values:
        return None
    values = sorted(values)
    return values[max(0, math.ceil(len(values) * fraction) - 1)]


def distribution(values):
    values = list(values)
    if not values:
        return {"count": 0, "sum": 0, "mean": None, "median": None, "p95": None, "max": None}
    return {"count": len(values), "sum": sum(values), "mean": statistics.mean(values),
            "median": statistics.median(values), "p95": percentile(values, 0.95), "max": max(values)}


def ratio(numerator, denominator):
    return numerator / denominator if denominator else None


def read_rows(path, start, end):
    with path.open(newline="") as handle:
        for line_number, row in enumerate(csv.DictReader(handle), 2):
            if None in row or any(value in (None, "") for value in row.values()):
                raise ValueError(f"incomplete/malformed CSV row {path}:{line_number}")
            values = {key: number(value) for key, value in row.items() if key is not None}
            step = values.get("step", -1)
            if (start is None or step >= start) and (end is None or step <= end):
                yield values


def header(path):
    with path.open(newline="") as handle:
        return set(next(csv.reader(handle), []))


def locate(folder, preferred, required):
    candidates = [folder / name for name in preferred]
    candidates.extend(sorted(folder.glob("*.csv")))
    seen = set()
    for path in candidates:
        if path in seen or not path.is_file():
            continue
        seen.add(path)
        if required.issubset(header(path)):
            return path
    return None


def identity(path):
    if path is None:
        return None
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for block in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(block)
    return {"path": str(path.resolve()), "bytes": path.stat().st_size, "sha256": digest.hexdigest()}


def count_summary(rows, field):
    return distribution(row.get(field, 0) for row in rows)


def summarize_response(rows):
    totals = Counter()
    for row in rows:
        for key, value in row.items():
            if key not in ("step", "enabled"):
                totals[key] += value
    step_count = len(rows)
    excluded_suffix = ("_bytes", "_ns", "_sum", "_max")
    work = [(key, value) for key, value in totals.items()
            if not key.startswith("ns_") and not key.endswith(excluded_suffix)]
    rank = [{"counter": key, "total": value, "per_step": ratio(value, step_count)}
            for key, value in sorted(work, key=lambda item: item[1], reverse=True) if value]
    timings = [{"counter": key, "total_ms": value / 1e6,
                "mean_ms_per_step": ratio(value / 1e6, step_count),
                "per_step_ns": count_summary(rows, key)}
               for key, value in sorted(totals.items(), key=lambda item: item[1], reverse=True)
               if key.startswith("ns_") and value]
    allocations = {key: value for key, value in totals.items()
                   if ("alloc" in key or "growth" in key) and not key.startswith("ns_")}
    contexts = {}
    for name in ("ordinary", "legacy", "growing"):
        visits = totals[name + "_rows"]
        changes = totals[name + "_accumulator_changes"]
        contexts[name] = {"row_visits": visits, "accumulator_changes": changes,
                          "endpoint_changes": totals[name + "_endpoint_changes"],
                          "changed_accumulator_no_endpoint_delta": totals[name + "_zero_velocity_changes"],
                          "unchanged_accumulator_rows": totals[name + "_no_accumulator_changes"],
                          "accumulator_change_fraction": ratio(changes, visits)}
    return {
        "steps": step_count,
        "first_step": min((r["step"] for r in rows), default=None),
        "last_step": max((r["step"] for r in rows), default=None),
        "disabled_rows": sum(not row.get("enabled", 0) for row in rows),
        "counter_totals": dict(totals),
        "ranked_work_counters_incomparable_units": rank,
        "inclusive_timings_do_not_sum": timings,
        "allocation_observations_not_allocator_total": allocations,
        "row_contexts": contexts,
        "fixed_points": {
            "full_sweeps": totals["solver_sweeps"],
            "exact_fixed_point_sweeps": totals["solver_sweep_fixed_points"],
            "later_fixed_point_sweeps": totals["solver_sweep_after_fixed_point"],
            "later_fixed_point_row_visits": totals["solver_fixed_point_row_visits"],
            "exact_fixed_point_fraction": ratio(totals["solver_sweep_fixed_points"], totals["solver_sweeps"]),
            "later_fixed_point_row_fraction": ratio(totals["solver_fixed_point_row_visits"], totals["growing_rows"]),
            "range_calls_per_full_sweep": ratio(totals["range_calls"], totals["solver_sweeps"]),
        },
        "duplicate_domain_bookkeeping": {
            "pair_set_duplicate_fraction": ratio(totals["pair_duplicates"], totals["pair_attempts"]),
            "callbacks_already_expanded_fraction": ratio(totals["callback_already_expanded"], totals["callback_activated_inputs"]),
            "pending_batch_duplicate_fraction": ratio(totals["callback_next_duplicates"], totals["callback_next_before_unique"]),
            "forced_queue_already_expanded_fraction": ratio(totals["forced_queue_already_expanded"], totals["forced_queue_scans"]),
            "forced_queue_scans_per_force_activated_body": ratio(totals["forced_queue_scans"], totals["deferred_gravity_bodies"]),
            "spatial_candidates_per_query": ratio(totals["query_candidates"], totals["query_slots_calls"]),
            "manifold_points_per_query": ratio(totals["manifold_points"], totals["manifold_queries"]),
            "continuing_row_fraction": ratio(totals["continuing_rows"], totals["appended_domain_rows"]),
        },
        "warm_provided_inputs": {
            name: {"prepare_rows": totals[name + "_prepare_rows"],
                   "normal_nonzero": totals[name + "_warm_normal_nonzero"],
                   "tangent_nonzero": totals[name + "_warm_tangent_nonzero"],
                   "any_nonzero": totals[name + "_warm_any_nonzero"],
                   "zero": totals[name + "_warm_zero"],
                   "normal_sum": totals[name + "_warm_normal_sum"],
                   "nonzero_fraction": ratio(totals[name + "_warm_any_nonzero"], totals[name + "_prepare_rows"])}
            for name in ("ordinary", "legacy", "growing")},
    }


def summarize_events(rows):
    sums = Counter()
    by_step = defaultdict(Counter)
    repeated_keys = Counter()
    for row in rows:
        step = row["step"]
        by_step[step]["events"] += 1
        repeated_keys[(row.get("a"), row.get("b"), row.get("pa"), row.get("pb"))] += 1
        for key in ("changed", "delta_le_1e_7", "delta_le_1e_5", "delta_le_one_ulp",
                    "warm_entries_zeroed", "warm_entries_removed", "warm_normal_discarded",
                    "wall_ns", "cpu_ns", "island", "rows"):
            sums[key] += row.get(key, 0)
            by_step[step][key] += row.get(key, 0)
    groups = {}
    for label, predicate in (("incremental", lambda r: r.get("incremental", 0)),
                             ("legacy", lambda r: not r.get("incremental", 0))):
        selected = [row for row in rows if predicate(row)]
        groups[label] = {"events": len(selected),
                         **{key: count_summary(selected, key)
                            for key in ("island", "rows", "changed", "wall_ns", "cpu_ns",
                                        "warm_entries_zeroed", "warm_entries_removed")}}
    changed = sums["changed"]
    tiny = sums["delta_le_1e_7"]
    small = sums["delta_le_1e_5"]
    return {"events": len(rows), "steps_with_events": len(by_step),
            "totals": dict(sums), "groups": groups,
            "per_step": {key: distribution(values[key] for values in by_step.values())
                         for key in ("events", "changed", "warm_entries_zeroed", "warm_entries_removed", "wall_ns", "cpu_ns")},
            "descriptive_delta_bins_only": {
                "maximum_component_delta_le_1e_7": tiny,
                "maximum_component_delta_gt_1e_7_le_1e_5": small - tiny,
                "maximum_component_delta_gt_1e_5": changed - small,
                "all_components_le_one_ordered_ulp": sums["delta_le_one_ulp"],
                "changed_body_observations": changed,
                "le_1e_7_fraction": ratio(tiny, changed),
                "le_1e_5_fraction": ratio(small, changed),
                "le_one_ulp_fraction": ratio(sums["delta_le_one_ulp"], changed),
                "max_component_delta": max((row.get("max_component_delta", 0) for row in rows), default=None),
                "min_positive_component_delta": min((row.get("min_nonzero_component_delta", 0) for row in rows
                                                     if row.get("min_nonzero_component_delta", 0) > 0), default=None),
            },
            "most_repeated_pair_primitive_identities": [
                {"key": list(key), "events": count} for key, count in repeated_keys.most_common(20)],
            }, by_step


def compare_warm(response_rows, event_steps):
    pairs = []
    for row in response_rows:
        step = row["step"]
        previous = event_steps.get(step - 1, {})
        current = event_steps.get(step, {})
        pairs.append({"step": step,
                      "ordinary_prepare_rows": row.get("ordinary_prepare_rows", 0),
                      "ordinary_nonzero_warm_rows": row.get("ordinary_warm_any_nonzero", 0),
                      "ordinary_normal_sum": row.get("ordinary_warm_normal_sum", 0),
                      "same_step_event_discards": current.get("warm_entries_zeroed", 0) + current.get("warm_entries_removed", 0),
                      "previous_step_event_discards": previous.get("warm_entries_zeroed", 0) + previous.get("warm_entries_removed", 0),
                      "same_step_normal_discarded": current.get("warm_normal_discarded", 0),
                      "previous_step_normal_discarded": previous.get("warm_normal_discarded", 0)})
    return {"not_a_matched_cache_cohort": True,
            "note": "Provided warm rows and discarded cache entries have different multiplicity and timing; do not interpret their ratio as lost reuse. Previous-step comparisons exclude events outside the selected step window.",
            "ordinary_warm_rows_per_step": distribution(row["ordinary_nonzero_warm_rows"] for row in pairs),
            "same_step_event_discards_per_step": distribution(row["same_step_event_discards"] for row in pairs),
            "rows": pairs}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("runfolder", type=Path)
    parser.add_argument("--start-step", type=int)
    parser.add_argument("--end-step", type=int)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    if args.start_step is not None and args.end_step is not None and args.start_step > args.end_step:
        parser.error("--start-step exceeds --end-step")
    if not args.runfolder.is_dir():
        parser.error("runfolder is not a directory")
    response = locate(args.runfolder, ("response.csv",), {"step", "domain_attempts", "growing_rows"})
    events = locate(args.runfolder, ("events.csv", "event_records.csv"), {"step", "event", "warm_entries_zeroed"})
    stages = locate(args.runfolder, ("event_stages.csv", "event-stages.csv"), {"step", "initial_schedule_cpu_ns"})
    if response is None:
        parser.error("no response observer CSV found")
    response_rows = list(read_rows(response, args.start_step, args.end_step))
    event_rows = list(read_rows(events, args.start_step, args.end_step)) if events else []
    event_report, event_steps = summarize_events(event_rows)
    report = {
        "schema": "judas-response-audit-summary-v1",
        "files": {"response": identity(response), "events": identity(events), "event_stages": identity(stages)},
        "window": {"start_step": args.start_step, "end_step": args.end_step},
        "warnings": ["Inclusive response clocks overlap and must not be summed.",
                     "Counter rankings compare work counts with different units, not cycle costs.",
                     "Small represented velocity changes and ULP bins are descriptive, never a valid impulse cutoff.",
                     "Linear/angular component deltas mix dimensions and are not a physical error norm.",
                     "Warm input/discard counts are not a one-to-one cache-entry cohort.",
                     "Instrumentation overhead requires an uninstrumented timing control and digest parity."],
        "response": summarize_response(response_rows),
        "events": event_report if events else None,
        "warm_input_discard_comparison": compare_warm(response_rows, event_steps) if events else None,
    }
    if events is None:
        report["warnings"].append("Event records missing; warm discards and velocity bins unavailable.")
    if stages:
        stage_rows = list(read_rows(stages, args.start_step, args.end_step))
        keys = sorted({key for row in stage_rows for key in row if key.endswith(("_wall_ns", "_cpu_ns"))})
        report["exclusive_event_stage_timings"] = sorted(
            [{"counter": key, "total_ms": sum(row.get(key, 0) for row in stage_rows) / 1e6,
              "per_step_ns": count_summary(stage_rows, key)} for key in keys],
            key=lambda item: item["total_ms"], reverse=True)
        report["dropped_event_records"] = sum(row.get("dropped", 0) for row in stage_rows)
        if report["dropped_event_records"]:
            report["warnings"].append("Some event records were dropped; event summaries are incomplete.")
    text = json.dumps(report, indent=2, allow_nan=False) + "\n"
    if args.output:
        args.output.write_text(text)
    else:
        print(text, end="")


if __name__ == "__main__":
    main()
