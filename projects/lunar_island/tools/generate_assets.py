#!/usr/bin/env python3
"""Offline authoring. Prompt3D and its installed sources are strictly read-only."""
import argparse
import copy
import hashlib
import json
import math
from pathlib import Path
import shutil
import sys


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--prompt3d", type=Path, required=True)
    parser.add_argument(
        "--style", choices=["realistic", "lowpoly"], default="realistic"
    )
    args = parser.parse_args()
    sys.dont_write_bytecode = True
    sys.path.insert(0, str(args.prompt3d))
    import numpy as np
    from prompt3d.pipeline import generate, build_styled
    from prompt3d.export import to_glb
    from prompt3d import geometry as G, textures as T, scans
    from prompt3d.generators.lettering import lettering
    from material_finish import finish_static

    root = Path(__file__).resolve().parents[1]
    repo = root.parents[1]
    catalog, receipts, used = {}, [], set()
    realistic = args.style == "realistic"
    # Do not accidentally consume mutable, locally generated overrides. These
    # installed scan sets have named CC0 sources and are copied with provenance.
    scans.GEN_DIR = str(root / "ArtSource/no-texture-overrides")

    def save(scene, name, **source):
        path = root / "Assets/models" / (name + ".glb")
        to_glb(scene, str(path), textures=realistic, contrast=1, flat=not realistic)
        used.update(T.kind_for(m) for m in scene.materials.values())
        low, high = scene.bounds()
        catalog[name] = {
            "path": str(path.relative_to(root)),
            "bounds": [low.tolist(), high.tolist()],
            "style": args.style,
            "sha256": sha(path),
            **source,
        }
        return path

    for i, (name, subject) in enumerate(
        [
            ("coastal_pine", "pine tree 8 metres tall"),
            ("silver_birch", "birch tree 7 metres tall"),
            ("island_oak", "oak tree 9 metres tall"),
            ("fern", "fern"),
        ]
    ):
        prompt = "a " + ("realistic " if realistic else "low poly ") + subject
        seed = 85320 + i
        result = generate(
            prompt, seed, str(root / "ArtSource/prompt3d"), obj=False, rewrite=False
        )
        scene = build_styled(result["category"], result["params"], seed, args.style)
        if name == "fern":
            for mats in scene.groups.values():
                for material in list(mats):
                    if not material.startswith("leaf_"):
                        del mats[material]
            low, _ = scene.bounds()
            scene.transform_all(np.eye(3) * 2.5, (0, -low[1] * 2.5, 0))
        path = save(
            scene, name, source="Prompt3D prompt plus ground-root adaptation for fern"
        )
        receipts.append(
            {
                "prompt": prompt,
                "seed": seed,
                "category": result["category"],
                "params": result["params"],
                "finish": args.style,
                "sha256": sha(path),
            }
        )

    bush = copy.deepcopy(scene)
    bush.name = "Wild berry patch"
    bush.material("berries", "#a33a39", roughness=0.6)
    for i in range(10):
        angle = i * math.tau / 10
        bush.add(
            bush.name,
            "berries",
            G.sphere(0.045, 8, 5),
            None,
            (math.cos(angle) * 0.22, 0.22 + i % 3 * 0.08, math.sin(angle) * 0.22),
        )
    save(
        bush,
        "berry_bush",
        source="Original ground-rooted fern derivative with berry clusters",
    )

    previous = repo / "projects/hearthworld/Assets/models"
    for name in [
        "axe",
        "pickaxe",
        "rock",
        "campfire",
        "chopping_stump",
        "barrel",
        "character_adventurer",
        "log",
        "well",
        "wood_crate",
    ]:
        path = root / "Assets/models" / (name + ".glb")
        shutil.copyfile(
            previous / ("rock_cluster.glb" if name == "rock" else name + ".glb"), path
        )
        source = {
            "path": str(path.relative_to(root)),
            "source": "Original operator-owned Hearthworld art",
            "sourceSha256": sha(path),
            "finish": "original",
        }
        if realistic and name != "character_adventurer":
            finished = finish_static(path, G, to_glb)
            used.update(T.kind_for(m) for m in finished.materials.values())
            source["finish"] = (
                "Prompt3D PBR material finish; original geometry retained"
            )
        source["sha256"] = sha(path)
        catalog[name] = source

    sign = G.Scene("Homestead trail sign")
    for name, color in [
        ("wood", "#956c43"),
        ("board", "#384438"),
        ("letters", "#eddcaa"),
    ]:
        sign.material(
            name, color, roughness=0.9, tex="wood" if name == "board" else None
        )
    for x in [-1, 1]:
        sign.add(sign.name, "wood", G.box(0.13, 1.8, 0.16), None, (x, 0.9, 0))
    sign.add(sign.name, "board", G.box(3.2, 0.95, 0.13), None, (0, 1.24, 0.04))
    for word, y in [("HOMESTEAD", 1.32), ("FOREST TRAIL", 0.95)]:
        sign.add(
            sign.name, "letters", lettering(word, 0.23, 0.025), None, (0, y, 0.112)
        )
    save(sign, "woodsign", source="Original raised lettering and timber sign")

    # Native terrain painting consumes colour layers. Detailed normal/ORM maps
    # remain on the models; do not claim M75's baked atlas is a tiled PBR material.
    material_sources = root / "ArtSource/materials"
    material_sources.mkdir(exist_ok=True)
    for name, kind in [("grass", "grass"), ("rock", "stone"), ("earth", "soil")]:
        scan = scans.load(kind)
        if scan is None:
            raise RuntimeError("Missing installed, licensed scan for ground: " + kind)
        color = scan["color"]
        if name == "grass":
            color = color.mean(axis=(0, 1), keepdims=True) * 0.75 + color * 0.25
        (root / "Assets/textures" / (name + ".png")).write_bytes(T.png(color))
        used.add(kind)
    _, normal, orm = T.maps("grass", 1)
    (root / "Assets/textures/grass_normal.png").write_bytes(normal)
    (root / "Assets/textures/grass_orm.png").write_bytes(orm)
    rng = np.random.default_rng(85330)
    sand = np.clip(
        np.array([0.70, 0.64, 0.49]) + rng.normal(0, 0.035, (256, 256, 1)), 0, 1
    )
    (root / "Assets/textures/sand.png").write_bytes(T.png(sand))
    scan_receipts = []
    for kind in sorted(k for k in used if k):
        path = Path(scans.DIR) / (kind + ".npz")
        if path.exists():
            shutil.copyfile(path, material_sources / path.name)
            source = scans.SETS[kind][0]
            scan_receipts.append(
                {
                    "kind": kind,
                    "source": "https://ambientcg.com/a/" + source,
                    "creator": "ambientCG",
                    "license": "CC0-1.0",
                    "attributionRequired": False,
                    "sha256": sha(path),
                }
            )
        else:
            scan_receipts.append(
                {
                    "kind": kind,
                    "source": "Prompt3D original procedural texture",
                    "implementation": "prompt3d/textures.py",
                }
            )
    shutil.copyfile(
        Path(scans.DIR) / "LICENSE.txt", material_sources / "AMBIENTCG_LICENSE.txt"
    )
    (material_sources / "PROVENANCE.json").write_text(
        json.dumps(scan_receipts, indent=2) + "\n"
    )
    (root / "ArtSource/asset_catalog.json").write_text(
        json.dumps(catalog, indent=2) + "\n"
    )
    for file in [
        "nlu.py",
        "pipeline.py",
        "geometry.py",
        "export.py",
        "textures.py",
        "scans.py",
    ]:
        path = args.prompt3d / "prompt3d" / file
        receipts.append({"tool_source": file, "sha256": sha(path)})
    (root / "ArtSource/generator_receipts.json").write_text(
        json.dumps(receipts, indent=2) + "\n"
    )


if __name__ == "__main__":
    main()
