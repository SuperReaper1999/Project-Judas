#!/usr/bin/env python3
"""Rebuild Hearthworld's original low-poly art with the verified local Prompt3D.

The external tool is read only. Its trained parser and procedural geometry are
used for the trees; the remaining game-specific meshes use its mesh writer.
This is an offline authoring dependency, never a runtime requirement.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import math
from pathlib import Path
import shutil
import sys


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--prompt3d", type=Path, required=True)
    ap.add_argument("--starter-pack", type=Path, required=True)
    args = ap.parse_args()
    sys.dont_write_bytecode = True
    sys.path.insert(0, str(args.prompt3d))
    import numpy as np
    from prompt3d import geometry as G
    from prompt3d.export import to_glb
    from prompt3d.pipeline import build_styled, generate
    from prompt3d.generators.lettering import lettering

    root = Path(__file__).resolve().parents[1]
    models = root / "Assets/models"
    sources = root / "Assets/art_sources"
    models.mkdir(parents=True, exist_ok=True)
    sources.mkdir(parents=True, exist_ok=True)
    catalog = {}
    receipts = []
    G.BEVEL = (0.07, 0.025)
    G.MAX_SEGMENTS = 10

    palette = {
        "timber": "#7d4e32", "timber_light": "#b08251", "cut_wood": "#d7b97a",
        "trim": "#e3d2aa", "plaster": "#bfbc9a", "roof": "#286b68",
        "roof_dark": "#1c494f", "stone": "#7c8791", "stone_light": "#a9b3b2",
        "steel": "#b4c4ca", "iron": "#455463", "copper": "#ca8749",
        "grip": "#364d4d", "leaf": "#4e8c53", "glass": "#7cc8cf",
    }

    def scene(name):
        s = G.Scene(name)
        for material, color in palette.items():
            s.material(material, color, roughness=0.85, metallic=0.5 if material == "steel" else 0)
        return s

    def box(s, mat, pos, size, rotation=None):
        s.add(s.name, mat, G.box(*size), rotation, pos)

    def bar(s, mat, a, b, radius, radius_top=None):
        s.add(s.name, mat, G.tube(a, b, radius, radius_top, 10))

    def save(s, name=None, **meta):
        name = name or s.name
        path = models / (name + ".glb")
        to_glb(s, str(path), textures=False, flat=True)
        low, high = s.bounds()
        catalog[name] = {"path": "Assets/models/" + path.name, "sha256": sha(path),
                         "bounds": [low.tolist(), high.tolist()], "stats": s.stats(), **meta}

    # Actual prompt -> trained slot parser -> procedural tree. An explicit height
    # keeps these from resolving to an unchanged library-model lookup.
    for name, prompt, seed in [
        ("pine", "a low poly pine tree 7 metres tall", 7421),
        ("birch", "a low poly birch tree 6 metres tall", 7422),
    ]:
        generated = generate(prompt, seed, str(sources / "prompt3d"), obj=False)
        generated.pop("seconds", None)
        generated["files"] = {"glb": "Assets/models/tree_" + name + ".glb"}
        receipts.append(generated)
        tree = build_styled(generated["category"], generated["params"], seed, "lowpoly")
        save(tree, "tree_" + name, source="Claude Prompt3D trained parser and procedural tree generator")
        for suffix, foliage in [("trunk", False), ("canopy", True)]:
            selected = G.Scene("tree_" + name + "_" + suffix)
            selected.materials.update(tree.materials)
            for group, mats in tree.groups.items():
                for material, prims in mats.items():
                    if material.startswith("leaves_") == foliage:
                        for prim in prims:
                            selected.add(group, material, prim)
            save(selected)
    # The raw CLI export duplicates the whole tree. Keep its small JSON receipt,
    # not a second identical runtime mesh.
    for path in (sources / "prompt3d").glob("*.glb"):
        path.unlink()
    for path in (sources / "prompt3d").glob("*.json"):
        path.unlink()
    (sources / "prompt3d").rmdir()
    G.BEVEL = (0.07, 0.025)
    G.MAX_SEGMENTS = 10

    # Enterable one-room home: openings are real missing triangles, not painted
    # windows. The matching conservative boxes are supplied to the scene author.
    s = scene("home")
    colliders = []

    def house_box(mat, pos, size, collision=True):
        box(s, mat, pos, size)
        if collision:
            colliders.append({"position": list(pos), "halfExtents": [v / 2 for v in size]})

    house_box("timber", (0, 0.04, 0), (8, 0.22, 6))
    # Floor boards add detail without another set of collision surfaces.
    for z in np.linspace(-2.82, 2.82, 18):
        house_box("timber_light" if int((z + 3) * 10) % 3 else "timber", (0, 0.158, z), (7.8, 0.025, 0.28), False)
    house_box("plaster", (0, 1.575, -2.9), (8, 2.85, 0.2))
    for x in (-2.4, 2.4):
        house_box("plaster", (x, 1.575, 2.9), (3.2, 2.85, 0.2))
    house_box("plaster", (0, 2.75, 2.9), (1.6, 0.5, 0.2))
    # Both side windows: z[-.8,.8], y[1.2,2.25].
    for x in (-3.9, 3.9):
        house_box("plaster", (x, 1.575, -1.85), (0.2, 2.85, 2.1))
        house_box("plaster", (x, 1.575, 1.85), (0.2, 2.85, 2.1))
        house_box("plaster", (x, 0.675, 0), (0.2, 1.05, 1.6))
        house_box("plaster", (x, 2.625, 0), (0.2, 0.75, 1.6))
        for z in (-0.86, 0.86):
            house_box("trim", (x, 1.73, z), (0.28, 1.2, 0.1), False)
        for y in (1.18, 2.3):
            house_box("trim", (x, y, 0), (0.28, 0.1, 1.8), False)
    for x in (-3.86, 3.86):
        for z in (-2.86, 2.86):
            house_box("timber", (x, 1.6, z), (0.24, 3.1, 0.24), False)
    for x in (-0.86, 0.86):
        house_box("trim", (x, 1.35, 3), (0.12, 2.6, 0.22), False)
    house_box("trim", (0, 2.62, 3), (1.85, 0.13, 0.24), False)
    house_box("timber", (0, 0.12, 3.55), (3.6, 0.2, 1.1))
    # Two roof planes, roof ridge along Z. Sloped collision is authored separately
    # as oriented boxes from this same specification.
    rise, half_width = 1.45, 4.35
    slope = math.atan2(rise, half_width)
    roof_colliders = []
    for side in (-1, 1):
        center = (side * half_width / 2, 3.0 + rise / 2, 0)
        dims = (math.hypot(rise, half_width), 0.2, 6.7)
        rotation = G.rot_z(-side * slope)
        box(s, "roof", center, dims, rotation)
        roof_colliders.append({"position": list(center), "halfExtents": [v / 2 for v in dims], "rotationZRadians": -side * slope})
        for z in np.linspace(-3.3, 3.3, 18):
            box(s, "roof_dark", (center[0], center[1] + 0.105, z), (dims[0], 0.035, 0.035), rotation)
    house_box("roof_dark", (0, 4.47, 0), (0.22, 0.18, 6.8), False)
    # Gables fill above front/back walls, preserving the lower doorway.
    for z in (-2.9, 2.9):
        tri = G.extrude([(-3.9, 0), (3.9, 0), (0, 1.3)], 0.16)
        s.add(s.name, "timber_light", tri, None, (0, 3.0, z))
    save(s, colliders=colliders, roofColliders=roof_colliders,
         doorway={"frontZ": 3, "width": 1.6, "height": 2.5},
         source="Original Hearthworld authored geometry; matched box collision specification")

    # Collectable tools: a grip pivot makes the same GLB useful on rack and hand.
    for tool in ("axe", "shovel", "pickaxe"):
        s = scene(tool)
        handle_h = 0.88 if tool == "axe" else 1.15
        bar(s, "timber_light", (0, -0.25, 0), (0, handle_h, 0), 0.035, 0.028)
        for y in np.linspace(-0.2, 0.14, 7):
            s.add(s.name, "grip", G.cylinder(0.039, 0.025, 10), None, (0, y, 0))
        if tool == "axe":
            blade = G.extrude([(-0.09, -0.12), (0.38, -0.23), (0.42, 0.23), (-0.09, 0.12)], 0.085)
            s.add(s.name, "steel", blade, None, (0, handle_h - 0.06, 0))
            box(s, "iron", (-0.055, handle_h - 0.06, 0), (0.16, 0.23, 0.12))
        elif tool == "pickaxe":
            bar(s, "steel", (-0.45, handle_h - 0.15, 0), (0, handle_h + 0.03, 0), 0.008, 0.065)
            bar(s, "steel", (0, handle_h + 0.03, 0), (0.42, handle_h - 0.12, 0), 0.065, 0.008)
            box(s, "iron", (0, handle_h, 0), (0.16, 0.14, 0.12))
        else:
            blade = G.extrude([(-0.17, 0.1), (0.17, 0.1), (0.18, -0.2), (0, -0.36), (-0.18, -0.2)], 0.04)
            s.add(s.name, "steel", blade, None, (0, handle_h + 0.22, 0))
            box(s, "iron", (0, handle_h + 0.07, 0.025), (0.1, 0.12, 0.1))
        save(s, source="Original Hearthworld procedural tool; grip pivot at origin")

    # Building pieces have centered pivots and a simple conservative collision
    # proxy; all boards/posts are visible actual geometry.
    s = scene("fence")
    for x in (-1.08, 1.08):
        box(s, "timber", (x, 0, 0), (0.18, 1.35, 0.2))
        s.add(s.name, "cut_wood", G.cylinder(0.14, 0.12, 4, 0), G.rot_y(math.pi / 4), (x, 0.675, 0))
    for y in (-0.3, 0.3):
        box(s, "timber_light", (0, y, 0), (2.4, 0.13, 0.12))
    for x in np.linspace(-0.8, 0.8, 7):
        box(s, "timber_light", (x, 0, 0.04), (0.13, 1.05, 0.1))
    save(s, collisionHalfExtents=[1.2, 0.75, 0.12])
    s = scene("wall")
    for x in np.linspace(-1.08, 1.08, 9):
        box(s, "timber_light" if int((x + 2) * 10) % 3 else "timber", (x, 0, 0), (0.25, 2.5, 0.14))
    for y in (-1.12, 1.12):
        box(s, "timber", (0, y, -0.1), (2.4, 0.16, 0.12))
    save(s, collisionHalfExtents=[1.2, 1.25, 0.15])
    s = scene("floor")
    for x in np.linspace(-1.04, 1.04, 8):
        box(s, "timber_light" if int((x + 2) * 10) % 3 else "timber", (x, 0, 0), (0.285, 0.16, 2.4))
    save(s, collisionHalfExtents=[1.2, 0.08, 1.2])
    s = scene("stone_wall")
    for row in range(5):
        for col in range(6):
            box(s, "stone_light" if (row + col) % 3 else "stone", (-1.0 + col * 0.4, -1.0 + row * 0.5, 0), (0.38, 0.48, 0.35))
    save(s, collisionHalfExtents=[1.2, 1.25, 0.18])
    s = scene("pillar")
    box(s, "timber", (0, 0, 0), (0.24, 2.5, 0.24))
    for y in (-1.13, 1.13):
        box(s, "iron", (0, y, 0), (0.28, 0.13, 0.28))
    save(s, collisionHalfExtents=[0.14, 1.25, 0.14])
    s = scene("stonebase")
    for x in (-0.6, 0.6):
        for z in (-0.6, 0.6):
            box(s, "stone_light" if x == z else "stone", (x, 0, z), (1.18, 0.4, 1.18))
    save(s, collisionHalfExtents=[1.2, 0.2, 1.2])
    s = scene("gravity_gun")
    box(s, "grip", (0, 0.12, 0.15), (0.22, 0.22, 0.45))
    box(s, "timber", (0, -0.06, 0.04), (0.12, 0.3, 0.17), G.rot_x(-0.22))
    box(s, "copper", (0, 0.18, 0.19), (0.25, 0.05, 0.3))
    for side in (-1, 1):
        bar(s, "steel", (side * 0.12, 0.11, 0.27), (side * 0.23, 0.1, 0.53), 0.027)
        bar(s, "steel", (side * 0.23, 0.1, 0.53), (side * 0.12, 0.1, 0.64), 0.027, 0.017)
    bar(s, "steel", (0, 0.25, 0.27), (0, 0.35, 0.53), 0.027)
    bar(s, "steel", (0, 0.35, 0.53), (0, 0.22, 0.64), 0.027, 0.017)
    s.material("coil", "#71dfd5", emissive="#1c7d7a", roughness=0.4)
    s.add(s.name, "coil", G.sphere(0.075, 10, 6), None, (0, 0.14, 0.43))
    save(s, source="Original Hearthworld three-prong field manipulator; grip origin, forward +Z")
    s = scene("hammer")
    bar(s, "timber_light", (0, -0.23, 0), (0, 0.62, 0), 0.035, 0.027)
    for y in np.linspace(-0.19, 0.1, 7):
        s.add(s.name, "grip", G.cylinder(0.039, 0.025, 10), None, (0, y, 0))
    box(s, "steel", (0.06, 0.62, 0), (0.44, 0.19, 0.17))
    box(s, "iron", (0.29, 0.62, 0), (0.08, 0.23, 0.21))
    bar(s, "steel", (-0.18, 0.65, -0.045), (-0.31, 0.49, -0.045), 0.035, 0.012)
    bar(s, "steel", (-0.18, 0.65, 0.045), (-0.31, 0.49, 0.045), 0.035, 0.012)
    save(s, source="Original Hearthworld claw hammer; grip origin, head +Y")
    s = scene("woodsign")
    for x in (-1.0, 1.0):
        box(s, "timber", (x, 0.9, 0), (0.13, 1.8, 0.16))
    box(s, "roof_dark", (0, 1.24, 0.04), (2.8, 0.95, 0.13))
    for y in (0.77, 1.73):
        box(s, "timber_light", (0, y, 0.08), (2.94, 0.09, 0.19))
    for word, y in (("BACKYARD", 1.29), ("DIG ZONE", 0.89)):
        letters = lettering(word, 0.28, 0.025)
        s.add(s.name, "trim", letters, None, (0, y, 0.112))
    save(s, collisionHalfExtents=[1.47, 0.9, 0.16],
         source="Original Hearthworld sign; actual raised mesh lettering from inspected Prompt3D lettering primitive")
    s = scene("log")
    s.add(s.name, "timber", G.cylinder(0.22, 2.2, 10, 0.20))
    s.add(s.name, "cut_wood", G.cylinder(0.205, 0.025, 10), None, (0, 2.2, 0))
    s.add(s.name, "cut_wood", G.cylinder(0.215, 0.025, 10), None, (0, -0.025, 0))
    save(s, source="Original Hearthworld log, axis +Y from ground origin")

    # Reuse the creator's original starter-pack assets, retaining exact bytes.
    pack_catalog = json.loads((args.starter_pack.parent / "catalog.json").read_text())
    pack_entries = {entry["name"]: entry for entry in pack_catalog["assets"]}
    for name in ("rock_cluster", "wood_crate", "barrel", "bed", "bench", "tool_rack", "well", "campfire", "chopping_stump", "hanging_lantern", "character_adventurer"):
        src = args.starter_pack / (name + ".glb")
        dst = models / src.name
        shutil.copyfile(src, dst)
        original = pack_entries[name]
        lo, hi = original["blenderBounds"]
        bounds = [[lo[0], lo[2], -hi[1]], [hi[0], hi[2], -lo[1]]]
        catalog[name] = {"path": "Assets/models/" + dst.name, "sha256": sha(dst), "bounds": bounds,
                         "stats": {"triangles": original["triangles"]},
                         "source": "Original Judas Low Poly Starter; verbatim reused GLB"}

    source_files = sorted((args.prompt3d / "prompt3d").glob("*.py"))
    source_files += sorted((args.prompt3d / "prompt3d/generators").glob("*.py"))
    source_files += sorted((args.prompt3d / "prompt3d/models").glob("*.npz"))
    provenance = {
        "tool": "Claude Prompt3D, trained CPU parser + procedural geometry; not a text-to-mesh diffusion model",
        "externalToolFiles": {str(p.relative_to(args.prompt3d)): sha(p) for p in source_files},
        "generated": receipts,
        "runtimeRequirements": "Only the exported self-contained GLBs; no Prompt3D, Python or ML service",
    }
    (sources / "generator_receipts.json").write_text(json.dumps(provenance, indent=2) + "\n")
    (sources / "asset_catalog.json").write_text(json.dumps(catalog, indent=2) + "\n")
    print(f"Wrote {len(catalog)} models ({sum(p.stat().st_size for p in models.glob('*.glb')):,} bytes)")


if __name__ == "__main__":
    main()
