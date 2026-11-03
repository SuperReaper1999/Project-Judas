#!/usr/bin/env python3
import re
import shutil
import sys
from pathlib import Path

repo = Path.cwd()
base = Path(sys.argv[1]).resolve()
base.mkdir(parents=True, exist_ok=False)
tiny = repo / "projects/tiny_game"
model = repo / "projects/planetfall/Assets/models/frontier_house.judasmodel"
asset = "0402f153efc0b8fa3ecd6dc16d9ee058"

for case, count in (("tiny", 0), ("repeated-model", 64), ("large-hierarchy", 1400)):
    folder = base / case
    (folder / "Assets").mkdir(parents=True)
    (folder / "Scenes").mkdir()
    shutil.copy2(tiny / "tiny_game.judasproj", folder / "sample.judasproj")
    scene = (tiny / "Scenes/main.judas").read_text()
    if case == "repeated-model":
        shutil.copy2(model, folder / "Assets" / model.name)
        shutil.copy2(str(model) + ".judasmeta", folder / "Assets" / (model.name + ".judasmeta"))
    if count:
        scene = re.sub(r"^  next-id \d+$", f"  next-id {1001 + count}", scene, flags=re.M)
    for i in range(count):
        scene += f'\nobject {1000+i} "Sample {i+1:04}"\n  position 0 0 0\n  rotation 1 0 0 0\n  scale 1 1 1\n'
        if case == "repeated-model":
            scene += (
                "  render mesh\n  render.visible false\n  render.half-extents 0.5 0.5 0.5\n"
                "  render.radius 0.5\n  render.color 1 1 1\n  render.alpha 1\n"
                "  render.secondary-color 0.8 0.8 0.8\n  render.secondary-alpha 1\n"
                f'  render.mesh-asset "{asset}"\n  render.texture-asset ""\n'
            )
        elif i % 5:
            scene += f"  parent {1000+i-i%5}\n"
        scene += "end\n"
    (folder / "Scenes/main.judas").write_text(scene)
