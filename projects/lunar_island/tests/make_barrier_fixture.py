#!/usr/bin/env python3
"""Copy the game and author one unseen guard outside a cottage window.

The production game modules are unchanged. Only this isolated scene/encounter
fixture changes the guard's initial placement, patrol and sight distance.
"""
import argparse
import json
import shutil
from pathlib import Path

source = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser()
parser.add_argument("destination", type=Path)
args = parser.parse_args()
if args.destination.exists():
    raise SystemExit("Choose a new empty destination; no existing project is replaced.")
shutil.copytree(
    source,
    args.destination,
    ignore=shutil.ignore_patterns("Review", "ArtSource", "__pycache__"),
)
data = json.loads((source / "Authoring/encounter_manifest.json").read_text())
data["actors"]["bandit_guard"].update(
    position=[-12.5, 14.24, -3.9], route=[[-12.5, 14.24, -3.9]], sight=0
)
weapons = json.loads((source / "Authoring/encounters.json").read_text())["weapons"]
(args.destination / "Assets/scripts/encounter-data.js").write_text(
    "export const WEAPONS = "
    + json.dumps(weapons)
    + ";\nexport const PEOPLE = "
    + json.dumps(data)
    + ";\n"
)
p = args.destination / "Scenes/island.judas"
scene = json.loads(p.read_text())
for obj in scene["data"]["objects"]:
    if obj["id"] in [6210, 6211]:
        obj["fields"]["position"] = [-12.5, 14.24, -3.9]
p.write_text(json.dumps(scene, indent=2) + "\n")
print(args.destination / "lunar_island.judasproj")
