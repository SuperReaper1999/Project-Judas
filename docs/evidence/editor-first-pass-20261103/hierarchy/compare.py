import csv
import sys
from pathlib import Path

def read(path):
    keys = ("objects", "ui_scale", "tagged", "scroll")
    with Path(path).open(newline="") as source:
        return {tuple(row[key] for key in keys): row for row in csv.DictReader(source)}

old, new = map(read, sys.argv[1:])
if old.keys() != new.keys():
    raise SystemExit("unmatched workload keys")
print("objects scale tagged scroll samples old_median new_median old_p95 new_p95 old_worst new_worst")
for key, before in old.items():
    after = new[key]
    if before["samples"] != after["samples"]:
        raise SystemExit("unmatched sample counts")
    print(*key, before["samples"], *(row[metric] for metric in ("median_ms", "p95_ms", "worst_ms")
                                     for row in (before, after)))
