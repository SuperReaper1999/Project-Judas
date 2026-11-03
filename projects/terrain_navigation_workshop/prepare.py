"""Create the terrain and bake navigation in a fresh copy of this workshop."""
import argparse
import os
from pathlib import Path
import re
import subprocess

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("sdk", type=lambda path: Path(path).resolve())
args = parser.parse_args()
root = Path(__file__).resolve().parent
suffix = ".exe" if os.name == "nt" else ""


def run(tool, *arguments):
    subprocess.run([str(args.sdk / (tool + suffix)), *arguments], cwd=root, check=True)


scene = root / "Scenes/main.judas"
template = scene.read_text(encoding="utf-8")
products = {
    "render.mesh-asset": "terrain.judasmodel",
    "render.texture-asset": "terrain.tga",
    "body.collision-asset": "terrain.judascollision",
}
if any(f'  {key} ""\n' not in template for key in products):
    parser.error("use a fresh copy of the workshop")

source = "Imports/yard.judasterrain"
(root / "Imports").mkdir(exist_ok=True)
run("judas_terrain_author", "create", source, "65", "65", "32", "32", "Assets/Terrain/yard")
run("judas_terrain_author", "brush", source, "raise", "0", "0", "8", "3", "0.6")
run("judas_terrain_author", "cook", "Workshop.judasproj", source)

for key, product in products.items():
    sidecar = (root / "Assets/Terrain/yard" / (product + ".judasmeta")).read_text(encoding="utf-8")
    asset = re.search(r'^id "([^"]+)"$', sidecar, re.MULTILINE)
    if not asset:
        raise ValueError(f"Missing asset ID for {product}")
    template = template.replace(f'  {key} ""\n', f'  {key} "{asset[1]}"\n', 1)

scene.write_text(template, encoding="utf-8")
run("judas_navigation_bake", "Workshop.judasproj", "Scenes/main.judas")
