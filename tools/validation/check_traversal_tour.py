#!/usr/bin/env python3
"""Independent observations of the shipped traversal fixture's native actor.
The fixture has actor 1 as its first live physical object in both scenes.
"""
import csv
import json
from pathlib import Path
import sys

lines = Path(sys.argv[1]).read_text(encoding="utf-8").splitlines()
header = next(line for line in lines if line.startswith("step,time,")).split(",")
rows = {}
for line in lines:
    if line and line[0].isdigit() and "," in line:
        row = dict(zip(header, next(csv.reader([line]))))
        rows[int(row["frame"])] = row
assert rows, "missing physical observations"
checks = 0
for row in rows.values():
    # Regression: ENTITY used to mix authored position with physical velocity.
    for axis in "XYZ":
        assert abs(float(row["pos" + axis]) - float(row["obj0Pos" + axis])) < 2e-5, (
            "ENTITY must report current native pose", row["frame"], axis)
        checks += 1
for frame, expected in [(150, (2.666667, 1.3)), (260, (6.666667, 2.7)),
                        (350, (1, 2)), (470, (1, 3)), (530, (0, 3))]:
    row = rows[frame]
    for axis, value in zip("XY", expected):
        assert abs(float(row["obj0Pos" + axis]) - value) < .025, (frame, axis, row)
        checks += 1
print(json.dumps({"checks": checks, "pass": True, "frames": len(rows)}))
