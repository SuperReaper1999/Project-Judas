#!/usr/bin/env python3
"""Offline analysis of MainThreadSampler files; never starts an engine workload.

PCs name where the main thread received a software CPU timer signal. A step tag
is the LAST COMPLETED step, not the step currently executing. Inclusive tags
300..900 usually sample work in steps 301..901 and intervening observer/render
work. Counts are neither hardware cycles nor inclusive call-stack attribution.
"""

import argparse
import bisect
import collections
import csv
import hashlib
import json
from pathlib import Path
import re
import subprocess


def integer(value):
    return int(value, 16) if value.lower().startswith("0x") else int(value)


def fingerprint(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def read_csv(path, required):
    with path.open(newline="", encoding="utf-8") as stream:
        reader = csv.DictReader(stream)
        if not required.issubset(reader.fieldnames or ()):
            raise ValueError(f"{path}: missing columns {sorted(required - set(reader.fieldnames or ())) }")
        return list(reader)


def run_tool(command, addresses=None):
    return subprocess.run(command, input=addresses, text=True, capture_output=True,
                          check=False, timeout=90)


def symbols_for(path, addresses, addr2line, nm, warnings):
    """Return exact debug names first; bounded ELF-symbol ranges as fallback."""
    result = {}
    try:
        for begin in range(0, len(addresses), 2048):
            batch = addresses[begin:begin + 2048]
            completed = run_tool([addr2line, "-C", "-f", "-e", str(path)],
                                 "".join(f"0x{address:x}\n" for address in batch))
            lines = completed.stdout.splitlines()
            if completed.returncode or len(lines) != len(batch) * 2:
                warnings.append(f"addr2line {path}: {completed.stderr.strip()[:400] or 'unexpected output'}")
                continue
            for index, address in enumerate(batch):
                name, location = lines[index * 2:index * 2 + 2]
                if name and name != "??":
                    result[address] = (name, location, "addr2line")
    except (OSError, subprocess.TimeoutExpired) as error:
        warnings.append(f"addr2line {path}: {error}")

    unresolved = [address for address in addresses if address not in result]
    if unresolved:
        # Do not label an arbitrary nearest symbol as the executing function.
        # Accept only a text-symbol range containing PC (or its exact zero-size
        # start). This fallback may name an enclosing non-inlined ELF symbol.
        ranges = []
        try:
            completed = run_tool([nm, "-n", "-S", "--defined-only", "-C", str(path)])
            if completed.returncode:
                warnings.append(f"nm {path}: {completed.stderr.strip()[:400]}")
            for line in completed.stdout.splitlines():
                match = re.match(r"^([0-9a-fA-F]+)\s+([0-9a-fA-F]+)\s+([TtWwIi])\s+(.+)$", line)
                if match:
                    start, size, kind, name = match.groups()
                    ranges.append((int(start, 16), int(size, 16), name, kind))
            ranges.sort()
            starts = [item[0] for item in ranges]
            for address in unresolved:
                index = bisect.bisect_right(starts, address) - 1
                matches = []
                if index >= 0:
                    start = ranges[index][0]
                    while index >= 0 and ranges[index][0] == start:
                        _, size, name, kind = ranges[index]
                        if (size and address < start + size) or address == start:
                            matches.append(name)
                        index -= 1
                if matches:
                    result[address] = (" | ".join(sorted(set(matches))), "??", "nm-containing-range")
        except (OSError, subprocess.TimeoutExpired) as error:
            warnings.append(f"nm {path}: {error}")
    for address in addresses:
        result.setdefault(address, (f"UNRESOLVED@ELF+0x{address:x}", "??", "unresolved"))
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("directory", type=Path, help="Sampler output directory")
    parser.add_argument("--output", type=Path, help="Analysis output directory; default: DIRECTORY/symbolized")
    parser.add_argument("--start", type=int, default=300, help="Inclusive last-completed-step tag")
    parser.add_argument("--end", type=int, default=900, help="Inclusive last-completed-step tag")
    parser.add_argument("--binary", type=Path, help="Local ELF for the main executable (module 0)")
    parser.add_argument("--module-file", action="append", default=[], metavar="RECORDED_PATH=LOCAL_ELF")
    parser.add_argument("--addr2line", default="addr2line")
    parser.add_argument("--nm", default="nm")
    args = parser.parse_args()
    if args.start > args.end:
        parser.error("--start exceeds --end")
    output = args.output or args.directory / "symbolized"
    files = {"samples": args.directory / "cpu-pc.csv", "modules": args.directory / "modules.csv"}
    sample_rows = read_csv(files["samples"], {"sample", "pc", "last_completed_step", "reported_signal_overruns"})
    module_rows = read_csv(files["modules"], {"module", "path", "load_bias", "segment_vaddr", "segment_memsz",
                                             "file_offset", "flags", "runtime_begin", "runtime_end"})
    replacements = {}
    for mapping in args.module_file:
        if "=" not in mapping:
            parser.error("--module-file requires RECORDED_PATH=LOCAL_ELF")
        recorded, local = mapping.split("=", 1)
        replacements[recorded] = Path(local)

    modules, segments = {}, []
    for row in module_rows:
        identity = integer(row["module"])
        bias = integer(row["load_bias"])
        module = {"module": identity, "path": row["path"], "load_bias": bias,
                  "category": "main_executable" if identity == 0 else "shared_image"}
        if identity in modules and modules[identity] != module:
            raise ValueError(f"inconsistent metadata for module {identity}")
        modules[identity] = module
        start, end = integer(row["runtime_begin"]), integer(row["runtime_end"])
        if start != bias + integer(row["segment_vaddr"]) or end != start + integer(row["segment_memsz"]):
            raise ValueError(f"inconsistent PT_LOAD range for module {identity}")
        segments.append((start, end, integer(row["flags"]), identity))
    segments.sort()
    starts = [segment[0] for segment in segments]
    prefix_ends, longest = [], 0
    for segment in segments:
        longest = max(longest, segment[1])
        prefix_ends.append(longest)

    def module_for(pc):
        index = bisect.bisect_right(starts, pc) - 1
        matches = []
        while index >= 0 and prefix_ends[index] > pc:
            start, end, flags, identity = segments[index]
            if start <= pc < end:
                matches.append((not bool(flags & 1), end - start, identity))
            index -= 1
        return modules[min(matches)[2]] if matches else None

    # Aggregate identical PCs before tool invocation; preserve exact counts.
    counts = collections.defaultdict(lambda: {"window": 0, "other": 0, "overruns": 0})
    unique_sample_ids = set()
    for row in sample_rows:
        sample_id = int(row["sample"])
        if sample_id in unique_sample_ids:
            raise ValueError(f"duplicate sample id {sample_id}")
        unique_sample_ids.add(sample_id)
        pc, step = integer(row["pc"]), int(row["last_completed_step"])
        counts[pc]["window" if args.start <= step <= args.end else "other"] += 1
        counts[pc]["overruns"] += max(0, int(row["reported_signal_overruns"]))
    pc_modules = {pc: module_for(pc) for pc in counts}
    requests = collections.defaultdict(set)
    for pc, module in pc_modules.items():
        if module:
            requests[module["module"]].add(pc - module["load_bias"])
    resolved, warnings, image_records = {}, [], []
    for identity, addresses in sorted(requests.items()):
        module = modules[identity]
        local = args.binary if identity == 0 and args.binary else replacements.get(module["path"], Path(module["path"]))
        record = dict(module, local_file=str(local), available=local.is_file())
        image_records.append(record)
        if local.is_file():
            resolved[identity] = symbols_for(local, sorted(addresses), args.addr2line, args.nm, warnings)
        else:
            warnings.append(f"image unavailable: module {identity}: {module['path']} (local {local})")
            resolved[identity] = {address: (f"UNRESOLVED@ELF+0x{address:x}", "??", "missing-image")
                                  for address in addresses}

    grouped = {}
    per_pc = []
    distribution = {part: {"image_categories": collections.Counter(), "symbol_quality": collections.Counter()}
                    for part in ("window", "other", "all")}
    for pc, count in counts.items():
        module = pc_modules[pc]
        if module:
            address = pc - module["load_bias"]
            name, location, quality = resolved[module["module"]][address]
            identity, path, category = module["module"], module["path"], module["category"]
        else:
            address, identity, path, category = pc, -1, "<outside-recorded-PT_LOAD>", "unmapped"
            name, location, quality = f"UNMAPPED@0x{pc:x}", "??", "unmapped"
        key = identity, name, quality
        if key not in grouped:
            grouped[key] = {"module": identity, "path": path, "category": category, "symbol": name,
                            "symbol_quality": quality, "representative_location": location,
                            "window_samples": 0, "other_samples": 0, "unique_pcs": 0}
        group = grouped[key]
        group["window_samples"] += count["window"]
        group["other_samples"] += count["other"]
        group["unique_pcs"] += 1
        for part in ("window", "other"):
            distribution[part]["image_categories"][category] += count[part]
            distribution[part]["symbol_quality"][quality] += count[part]
        distribution["all"]["image_categories"][category] += count["window"] + count["other"]
        distribution["all"]["symbol_quality"][quality] += count["window"] + count["other"]
        per_pc.append({"pc": f"0x{pc:x}", "elf_address": f"0x{address:x}", "module": identity,
                       "path": path, "category": category, "symbol": name, "location": location,
                       "symbol_quality": quality, "window_samples": count["window"],
                       "other_samples": count["other"], "reported_signal_overruns": count["overruns"]})
    window_count = sum(count["window"] for count in counts.values())
    groups = sorted(grouped.values(), key=lambda group: (-group["window_samples"], -group["other_samples"], group["path"], group["symbol"]))
    for group in groups:
        group["all_samples"] = group["window_samples"] + group["other_samples"]
        group["window_percent"] = group["window_samples"] * 100.0 / window_count if window_count else 0
    output.mkdir(parents=True, exist_ok=True)
    for name, rows, columns in (
        ("symbols.csv", groups, ["module", "path", "category", "symbol", "symbol_quality", "all_samples", "window_samples", "other_samples", "window_percent", "unique_pcs", "representative_location"]),
        ("pcs.csv", sorted(per_pc, key=lambda item: integer(item["pc"])), ["pc", "elf_address", "module", "path", "category", "symbol", "location", "symbol_quality", "window_samples", "other_samples", "reported_signal_overruns"])):
        with (output / name).open("w", newline="", encoding="utf-8") as stream:
            writer = csv.DictWriter(stream, fieldnames=columns)
            writer.writeheader()
            writer.writerows(rows)
    metadata_path = args.directory / "metadata.json"
    summary = {"schema": "judas.cpu-pc-symbol-summary.v1", "input_files": {key: {"path": str(path), "sha256": fingerprint(path)} for key, path in files.items()},
               "last_completed_step_window_inclusive": [args.start, args.end], "all_samples": len(sample_rows),
               "window_samples": window_count, "other_samples": len(sample_rows) - window_count,
               "unique_pcs": len(counts), "distribution": distribution, "images": image_records,
               "top_window_symbols": groups[:50], "warnings": warnings,
               "limitations": ["Last-completed-step tags are not exact current-step identities; observer and rendering work retain the same tag.",
                               "Sample shares estimate main-thread software CPU-PC occupancy, not hardware cycles, wall time, or inclusive call-graph cost.",
                               "Module 0 is the main executable; every other recorded loaded image is classified as shared_image, including the loader and vDSO.",
                               "addr2line symbol names retain demangled arguments; nm fallback uses containing text-symbol ranges and does not identify inlined frames.",
                               "Unresolved PCs remain explicit; reported overrun expirations have no reconstructed PC."]}
    if metadata_path.is_file():
        summary["sampler_metadata"] = json.loads(metadata_path.read_text())
    (output / "summary.json").write_text(json.dumps(summary, indent=2) + "\n")
    print(json.dumps({"output": str(output), "samples": len(sample_rows), "window_samples": window_count,
                      "top_window_symbols": groups[:15], "warnings": warnings}, indent=2))


if __name__ == "__main__":
    main()
