#!/usr/bin/env python3
import json
import statistics
import sys
from pathlib import Path

for path in map(Path, sys.argv[1:]):
    report = json.loads(path.read_text())
    frames = {row["id"]: row for row in report["frames"] + report["spikes"]}
    starts = [row for row in frames.values() if "World build" in row["boundaries"]]
    result = {"case": path.parent.name, "transition_frames": len(starts)}
    if len(starts) == 1:
        transition = starts[0]
        result["transition_frame_outer_ms"] = (transition["end_ns"] - transition["start_ns"]) / 1e6
        if following := frames.get(transition["id"] + 1):
            result["next_play_frame_outer_ms"] = (following["end_ns"] - following["start_ns"]) / 1e6
        for s in transition["scopes"]:
            if s["name"] in ("World build", "Editor Play startup", "Editor Play model preflight"):
                result[s["name"]] = s["inclusive_ns"] / 1e6
        for c in transition["counters"]:
            if c["name"] == "Editor Play model archives checked":
                result[c["name"]] = c["value"]
    edits = [row for row in frames.values() if row["mode"] == "editor edit"
             and row["id"] > 6 and "World build" not in row["boundaries"]]
    for counter in ("Hierarchy matched rows", "Hierarchy submitted rows"):
        series = [c["value"] for frame in edits for c in frame["counters"] if c["name"] == counter]
        if series:
            result[counter] = max(series)
    if edits:
        result["retained_edit_frame_median_ms"] = statistics.median(
            (row["end_ns"] - row["start_ns"]) / 1e6 for row in edits)
    print(json.dumps(result))
