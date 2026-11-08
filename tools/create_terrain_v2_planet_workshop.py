#!/usr/bin/env python3
"""Create the owned planetary Workshop. Refuse to replace an existing project."""
import argparse
import json
import math
from pathlib import Path
import shutil
import subprocess

REPO = Path(__file__).resolve().parents[1]
SEED = REPO / "projects/terrain_v2_workshop"
IDS = {key: f"8a2300000000000000000000000000{index:02x}" for index, key in
       enumerate(("script", "model", "native", "preview", "config"), 1)}


def normalized(v):
    size = math.sqrt(sum(x * x for x in v))
    return [x / size for x in v]


def meta(path, key, kind, source=""):
    Path(str(path) + ".judasmeta").write_text(
        f'JudasAssetMeta 1\nid "{IDS[key]}"\ntype {kind}\nsource {json.dumps(source)}\n')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path,
                        default=REPO / "projects/terrain_v2_planet_workshop")
    parser.add_argument("--build", type=Path, default=REPO / "build")
    args = parser.parse_args()
    root = args.output.resolve()
    if root.exists():
        parser.error("Choose a new directory; existing projects and saves are never rewritten.")
    for directory in ("Authoring", "Assets/terrain", "Assets/scripts", "Scenes"):
        (root / directory).mkdir(parents=True)
    project = (SEED / "terrain_v2_workshop.judasproj").read_text().replace(
        "Terrain V2 — Highlands Workshop", "Terrain V2 — Planetary Workshop")
    # Decode the existing input string, retaining every ordinary action.
    lines = []
    for line in project.splitlines():
        if line.startswith("input-map "):
            text = json.loads(line[len("input-map "):])
            prefix, amount, rest = text.split(" ", 2)
            text = f'{prefix} {int(amount) + 1} {rest} "origin_shift" 0 1 "key:O" 1 0'
            line = "input-map " + json.dumps(text)
        lines.append(line)
    (root / "terrain_v2_planet_workshop.judasproj").write_text("\n".join(lines) + "\n")
    for directory in ("ui", "fonts"):
        shutil.copytree(SEED / "Assets" / directory, root / "Assets" / directory)
    for file in (SEED / "Assets/terrain").iterdir():
        if "-natural.png" in file.name or file.name == "REUSED_MATERIAL_NOTICES.txt":
            shutil.copyfile(file, root / "Assets/terrain" / file.name)
    planet = dict(version=1, radius=1000, collisionSpacing=.5, collisionRadius=12)
    (root / "Authoring/planet.json").write_text(json.dumps(planet, indent=2) + "\n")
    source_path = root / "Authoring/highlands.judasterrain2"
    tool = args.build.resolve() / "judas_terrain_author"
    subprocess.run([str(tool), "v2-planet", str(root / "Authoring/planet.json"),
                    str(source_path), ".09375", "1", ".09375", "128"], check=True)
    source = json.loads(source_path.read_text())
    chart = source["planetChart"]
    n, pitch = source["samplesX"], source["spacing"]
    for z in range(2, n - 2):
        for x in range(2, n - 2):
            px, pz = (x - (n - 1) / 2) * pitch, (z - (n - 1) / 2) * pitch
            fade = min(1, max(0, min(x - 1, z - 1, n - 2 - x, n - 2 - z) / 8))
            hill = 7 * math.exp(-pz * pz / 36 - px * px / 190)
            ridge = 4.2 * math.exp(-((px + 15) ** 2 + (pz + 13) ** 2) / 52)
            height = fade * (.65 + hill + ridge + .16 * math.sin(px * .35) * math.sin(pz * .29))
            source["heights"][z * n + x] += height
            rock = min(.82, max(0, (height - 3.4) / 5))
            earth = .13 + .12 * math.sin(px * .21 + pz * .16) ** 2
            source["weights"][z * n + x] = [1 - rock - earth, earth, rock, 0]
    source["layerTextures"] = [f"8a2200000000000000000000000000{index:02x}"
                               for index in range(9, 13)]
    source["layerColours"] = [[1, 1, 1]] * 4
    source_path.write_text(json.dumps(source, separators=(",", ":")) + "\n")
    model = root / "Assets/terrain/highlands.judasmodel"
    subprocess.run([str(tool), "v2-native", str(source_path), str(model)], check=True)
    for suffix, key, kind in ((".judasmodel", "model", "mesh"),
                              (".judasterrain", "native", "terrain"),
                              (".preview.tga", "preview", "texture")):
        meta(model.with_suffix(suffix), key, kind, "Authoring/highlands.judasterrain2")
    # Source chart is the authority; these game-side helpers author placement and
    # a repeatable tunnel route on its +Y cube face. Engine validation checks the
    # resulting pose/provider/rim before Play. No alternate runtime terrain lives here.
    count = 2 ** chart["level"]
    u = 2 * (chart["x"] + .5) / count - 1
    v = 2 * (chart["y"] + .5) / count - 1
    direction = normalized([u, 1, -v])
    origin = [c * (1000 + chart["originHeight"]) for c in direction]
    right = normalized([1 - direction[0] ** 2,
                        -direction[0] * direction[1], -direction[0] * direction[2]])
    back = [right[1] * direction[2] - right[2] * direction[1],
            right[2] * direction[0] - right[0] * direction[2],
            right[0] * direction[1] - right[1] * direction[0]]
    trace = right[0] + direction[1] + back[2]
    qw = math.sqrt(1 + trace) / 2
    rotation = [qw, (direction[2] - back[1]) / (4 * qw),
                (back[0] - right[2]) / (4 * qw), (right[1] - direction[0]) / (4 * qw)]
    def point(x, height, z):
        ray = normalized([u + 2 * x / (chart["span"] * count), 1,
                          -(v - 2 * z / (chart["span"] * count))])
        return [ray[i] * (1000 + height) for i in range(3)]
    config = dict(origin=origin, rotation=rotation,
                  tunnel=[point(0, 2.2, 10 - 1.1 * i) for i in range(19)])
    (root / "Assets/scripts/geography.js").write_text("export default " + json.dumps(config) + ";\n")
    meta(root / "Assets/scripts/geography.js", "config", "script")
    shutil.copyfile(REPO / "tools/templates/terrain_planet_explorer.js",
                    root / "Assets/scripts/explorer.js")
    meta(root / "Assets/scripts/explorer.js", "script", "script")
    scene = (SEED / "Scenes/highlands.judas").read_text()
    scene = scene.replace("Terrain V2 — Highlands", "Terrain V2 — Planetary Highlands")
    scene = scene.replace("world-origin 0 0 0", "world-origin " + " ".join(map(repr, origin)))
    for key in ("model", "native", "preview", "script"):
        old = {"model": 2, "native": 14, "preview": 13, "script": 1}[key]
        scene = scene.replace(f"8a2200000000000000000000000000{old:02x}", IDS[key])
    scene = scene.replace(' terrain.asset ', ' terrain.planet 3\n terrain.asset ', 1)
    owner_start = scene.index('object 1 "')
    owner_end = scene.index('\nend', owner_start)
    section = scene[owner_start:owner_end].replace("rotation 1 0 0 0", "rotation " + " ".join(map(repr, rotation)))
    section = section.replace('25 12 25', '35 24 35')
    scene = scene[:owner_start] + section + scene[owner_end:]
    scene = scene.replace('object 2 "Gravity"\n position 0 0 0',
                          'object 2 "Radial gravity"\n position ' + " ".join(str(-c) for c in origin))
    scene = scene.replace('gravity uniform 9.81', 'gravity radial 9.81')
    spawn = point(0, 1.9, 13)
    scene = scene.replace('position 0 1.9 13', 'position ' + " ".join(str(spawn[i] - origin[i]) for i in range(3)))
    scene += '\nobject 3 "Planetary source"\n position 0 0 0\n rotation 1 0 0 0\n scale 1 1 1\n planet.surface ' + json.dumps(json.dumps(planet, separators=(",", ":"))) + '\nend\n'
    (root / "Scenes/highlands.judas").write_text(scene)
    (root / ".gitignore").write_text("Saves/\n")
    print("Created", root)


if __name__ == "__main__":
    main()
