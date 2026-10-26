#!/usr/bin/env python3
"""Stage 2 content authoring; reads protected source art, writes this project only.

Run after the Stage 1 world generator when rebuilding from source. Re-running this
file replaces only Stage 2's reserved entity range (3000..5999) and its prefabs.
"""
import argparse, json, math, shutil, sys
from pathlib import Path
from author_project import ROOT, REPO, ident, meta, write, obj, mesh, body, interpolate

CATALOG = json.loads((ROOT / "Authoring/construction.json").read_text())
WINDOW_BOXES = [
    {"center": [x, 0, 0], "halfExtents": [0.4, 1.35, 0.12]} for x in [-1.1, 1.1]
] + [
    {"center": [0, -0.9, 0], "halfExtents": [0.7, 0.45, 0.12]},
    {"center": [0, 1.05, 0], "halfExtents": [0.7, 0.3, 0.12]},
]


def frame_boxes(half):
    x, y, z = half
    return [
        {"center": [s * (x - 0.08), -0.05, 0], "halfExtents": [0.08, y - 0.05, z]}
        for s in [-1, 1]
    ] + [{"center": [0, y - 0.08, 0], "halfExtents": [x, 0.08, z]}]


def collision(kind, motion="dynamic"):
    d = CATALOG[kind]
    f = body(
        d["half"],
        mass=25 if d["category"] in ["Furniture", "Decoration"] else 65,
        motion=motion,
    )
    boxes = (
        WINDOW_BOXES
        if kind == "window_wall"
        else frame_boxes(d["half"]) if kind in ["door", "gate"] else None
    )
    if boxes:
        f["body"] = {"motion": {"symbol": motion}, "shape": {"symbol": "compound"}}
        f["body.compound-count"] = len(boxes)
        f["body.compound-box"] = boxes
    return f


def author_art(tool):
    sys.dont_write_bytecode = True
    sys.path.insert(0, str(tool))
    import numpy as np
    from prompt3d import geometry as G, textures as T, scans
    from prompt3d.export import to_glb
    from material_finish import finish_static

    scans.GEN_DIR = str(ROOT / "ArtSource/no-texture-overrides")
    catalog = json.loads((ROOT / "ArtSource/asset_catalog.json").read_text())

    def scene(name):
        sc = G.Scene(name)
        for material, color in [
            ("timber", "#876448"),
            ("plank_wood", "#a18561"),
            ("stone", "#93928b"),
            ("roof_shingles", "#484c49"),
            ("iron", "#656c70"),
            ("fabric", "#667a76"),
            ("paint", "#7b98a0"),
        ]:
            sc.material(material, color, roughness=0.87)
        return sc

    def box(sc, mat, p, h):
        sc.add(sc.name, mat, G.box(*(2 * v for v in h)), None, p)

    def save(sc, name):
        p = ROOT / f"Assets/models/{name}.glb"
        to_glb(sc, str(p), textures=True, contrast=1, flat=False)
        lo, hi = sc.bounds()
        catalog[name] = {
            "path": str(p.relative_to(ROOT)),
            "bounds": [lo.tolist(), hi.tolist()],
            "source": "Original Stage 2 mesh/PBR finish; source author_stage2.py",
        }
        meta(p, "mesh", "model/" + name)

    for kind in [
        "foundation",
        "floor",
        "wall",
        "window_wall",
        "door",
        "roof",
        "fence",
        "gate",
        "rug",
        "painting",
    ]:
        sc = scene(kind)
        h = CATALOG[kind]["half"]
        if kind == "window_wall":
            for b in WINDOW_BOXES:
                box(sc, "plank_wood", b["center"], b["halfExtents"])
            for x in [-0.72, 0.72]:
                box(sc, "timber", [x, 0.15, 0.14], [0.035, 0.65, 0.04])
            for y in [-0.48, 0.78]:
                box(sc, "timber", [0, y, 0.14], [0.75, 0.035, 0.04])
        elif kind in ["door", "gate"]:
            for b in frame_boxes(h):
                box(sc, "timber", b["center"], b["halfExtents"])
        elif kind == "fence":
            for x in [-1.44, 1.44]:
                box(sc, "timber", [x, 0, 0], [0.06, 0.6, 0.07])
            for x in np.linspace(-1.3, 1.3, 12):
                box(sc, "plank_wood", [x, 0.04, 0], [0.045, 0.52, 0.04])
            for y in [-0.27, 0.27]:
                box(sc, "timber", [0, y, -0.05], [1.5, 0.045, 0.04])
        elif kind == "painting":
            box(sc, "paint", [0, 0, 0], h)
            # A small original coastal landscape, actual raised mesh colour layers.
            sc.material("sea", "#426c7b")
            sc.material("hills", "#607958")
            sc.material("sun", "#d7c48e")
            box(sc, "sea", [0, -0.12, 0.044], [0.4, 0.14, 0.006])
            box(sc, "hills", [-0.12, -0.02, 0.053], [0.25, 0.07, 0.005])
            sc.add(sc.name, "sun", G.sphere(0.055, 10, 6), None, (0.22, 0.17, 0.06))
            for x in [-0.44, 0.44]:
                box(sc, "timber", [x, 0, 0.02], [0.025, 0.34, 0.035])
            for y in [-0.31, 0.31]:
                box(sc, "timber", [0, y, 0.02], [0.44, 0.025, 0.035])
        else:
            mat = {"foundation": "stone", "roof": "roof_shingles", "rug": "fabric"}.get(
                kind, "plank_wood"
            )
            box(sc, mat, [0, 0, 0], h)
            if kind in ["floor", "wall"]:
                for k in range(-4, 5):
                    if kind == "floor":
                        box(sc, "timber", [k * 0.32, 0.101, 0], [0.008, 0.004, 1.49])
                    else:
                        box(sc, "timber", [k * 0.32, 0, 0.121], [0.008, 1.34, 0.004])
        save(sc, "build_" + kind)
    for kind, height in [("door_leaf", 1.175), ("gate_leaf", 0.475)]:
        sc = scene(kind)
        box(sc, "plank_wood", [0, 0, 0], [0.65, height, 0.055])
        for y in [-height * 0.7, height * 0.7]:
            box(sc, "iron", [0, y, 0.065], [0.62, 0.035, 0.01])
        sc.add(sc.name, "iron", G.sphere(0.045, 8, 6), None, (0.5, 0, 0.09))
        save(sc, "build_" + kind)
    sc = scene("Timber gable")
    for i in range(18):
        x = -4.25 + i * 0.5
        h = 0.15 + (4.5 - abs(x)) * math.tan(math.radians(20))
        box(sc, "plank_wood", [x, h / 2, 0], [0.25, h / 2, 0.1])
    save(sc, "build_gable")
    source = REPO / "LowPolyAssets/low_poly_starter/Exports"
    for kind, original in [
        ("chest", "treasure_chest"),
        ("bed", "bed"),
        ("chair", "chair"),
        ("table", "table"),
        ("sofa", "sofa"),
        ("cupboard", "cabinet"),
        ("shelf", "bookshelf"),
        ("workbench", "smith_workbench"),
        ("lamp", "desk_lamp"),
    ]:
        temp = ROOT / f"Assets/models/build_{kind}.glb"
        shutil.copyfile(source / (original + ".glb"), temp)
        sc = finish_static(temp, G, to_glb)
        lo, hi = sc.bounds()
        desired = np.array(CATALOG[kind]["half"]) * 2
        sc.transform_all(
            np.diag(desired / (hi - lo)), -(lo + hi) / 2 * desired / (hi - lo)
        )
        save(sc, "build_" + kind)
    for name, half, metal in [
        ("barricade_wood", [0.8, 0.55, 0.08], False),
        ("barricade_metal", [0.8, 0.55, 0.08], True),
    ]:
        sc = scene(name)
        if metal:
            box(sc, "iron", [0, 0, 0], half)
        else:
            for y in [-0.42, -0.14, 0.14, 0.42]:
                box(sc, "plank_wood", [0, y, 0], [0.8, 0.12, 0.06])
            for x in [-0.63, 0.63]:
                box(sc, "iron", [x, 0, 0.07], [0.018, 0.52, 0.012])
        save(sc, name)
    (ROOT / "ArtSource/asset_catalog.json").write_text(
        json.dumps(catalog, indent=2) + "\n"
    )
    # Record/copy newly used texture source arrays; no mutable tool overrides.
    receipts = json.loads((ROOT / "ArtSource/materials/PROVENANCE.json").read_text())
    known = {r["kind"] for r in receipts}
    import hashlib

    for kind in ["fabric", "leather"]:
        p = Path(scans.DIR) / (kind + ".npz")
        if p.exists() and kind not in known:
            shutil.copyfile(p, ROOT / "ArtSource/materials" / p.name)
            receipts.append(
                {
                    "kind": kind,
                    "source": "https://ambientcg.com/a/" + scans.SETS[kind][0],
                    "creator": "ambientCG",
                    "license": "CC0-1.0",
                    "attributionRequired": False,
                    "sha256": hashlib.sha256(p.read_bytes()).hexdigest(),
                }
            )
    write(ROOT / "ArtSource/materials/PROVENANCE.json", receipts)


def author_world():
    scene = ROOT / "Scenes/island.judas"
    j = json.loads(scene.read_text())
    settings = j["data"]["settings"]
    rows = [r for r in j["data"]["objects"] if not 3000 <= r["id"] < 6000]
    terrain = json.loads((ROOT / "Authoring/homestead.judasterrain").read_text())

    def ground(x, z):
        return interpolate(terrain, x, z)

    def add(i, name, p, kind, motion="static", **extra):
        row = obj(
            i,
            name,
            p,
            **{**mesh("build_" + kind), **collision(kind, motion), "tags": 64, **extra},
        )
        rows.append(row)
        return row

    fixtures = []
    doors = []
    sockets = []
    homes = []
    for n, (cx, cz, label) in enumerate(
        [(-17, -10, "Seabird Cottage"), (58, 42, "Birchside Cabin")]
    ):
        base = 3000 + n * 200
        floorY = (
            max(ground(cx + x, cz + z) for x in [-6, 6] for z in [-4.5, 4.5]) + 0.18
        )
        floor = obj(
            base,
            label + " / collision floor",
            [cx, floorY - 0.1, cz],
            **{
                **mesh("build_floor"),
                **body([6, 0.1, 4.5]),
                "scale": [4, 1, 3],
                "tags": 64,
            },
        )
        rows.append(floor)
        # Continuous walls use repeated ordinary modules. Front has a real door gap.
        for k, x in enumerate([-4.5, -1.5, 1.5, 4.5]):
            for side, z in enumerate([-4.5, 4.5]):
                if side == 1 and k == 1:
                    continue
                kind = "window_wall" if k in [0, 3] else "wall"
                i = base + 10 + side * 10 + k
                add(i, label + " / " + kind, [cx + x, floorY + 1.35, cz + z], kind)
                if kind == "window_wall":
                    sockets.append(
                        {
                            "entity": str(i),
                            "position": [
                                cx + x,
                                floorY + 1.5,
                                cz + z + (0.19 if side else -0.19),
                            ],
                            "rotation": [1, 0, 0, 0],
                            "name": label + " window",
                            "door": None,
                        }
                    )
        for side, x in enumerate([-6, 6]):
            for k, z in enumerate([-3, 0, 3]):
                add(
                    base + 40 + side * 10 + k,
                    label + " / side wall",
                    [cx + x, floorY + 1.35, cz + z],
                    "wall",
                    rotation=[math.sqrt(0.5), 0, math.sqrt(0.5), 0],
                )
        # Doorway width 1.7m and closed infill strips instead of an invisible wall.
        dx, dz = cx - 1.5, cz + 4.5
        frame = base + 60
        leaf = base + 61
        add(frame, label + " / doorway frame", [dx, floorY + 1.35, dz], "door")
        for k, x in enumerate([-1.175, 1.175]):
            rows.append(
                obj(
                    base + 62 + k,
                    label + " / door infill",
                    [dx + x, floorY + 1.35, dz],
                    **{
                        **mesh("build_wall"),
                        **body([0.325, 1.35, 0.12]),
                        "scale": [0.325 / 1.5, 1, 1],
                        "tags": 64,
                    },
                )
            )
        rows.append(
            obj(
                leaf,
                label + " / physical hinged leaf",
                [dx, floorY + 1.2, dz],
                **{
                    **mesh("build_door_leaf"),
                    **body([0.65, 1.175, 0.055], mass=18, motion="dynamic"),
                    "tags": 64,
                },
            )
        )
        doors.append(
            {
                "entity": str(leaf),
                "frame": str(frame),
                "anchorA": [-0.65, 0, -0.075],
                "anchorB": [-0.65, -0.15, -0.075],
            }
        )
        sockets.append(
            {
                "entity": str(frame),
                "position": [dx, floorY + 1.25, dz + 0.22],
                "rotation": [1, 0, 0, 0],
                "name": label + " entrance",
                "door": str(leaf),
            }
        )
        # Flat roof modules and a shallow porch keep every opening genuinely traversable.
        angle = math.radians(20)
        halfDepth = 2.25 / math.cos(angle)
        for side in [-1, 1]:
            rows.append(
                obj(
                    base + 70 + (side + 1) // 2,
                    label + " / pitched roof",
                    [cx, floorY + 2.85 + 2.25 * math.tan(angle), cz + side * 2.25],
                    **{
                        **mesh("build_roof"),
                        **body([6, 0.1, halfDepth]),
                        "rotation": [
                            math.cos(angle / 2),
                            side * math.sin(angle / 2),
                            0,
                            0,
                        ],
                        "scale": [4, 1, halfDepth / 1.5],
                        "tags": 64,
                    },
                )
            )
        gableBoxes = []
        for i in range(18):
            x = -4.25 + i * 0.5
            h = 0.15 + (4.5 - abs(x)) * math.tan(angle)
            gableBoxes.append(
                {"center": [x, h / 2, 0], "halfExtents": [0.25, h / 2, 0.1]}
            )
        for side in [-1, 1]:
            rows.append(
                obj(
                    base + 170 + (side + 1) // 2,
                    label + " / timber gable",
                    [cx + side * 6, floorY + 2.7, cz],
                    **{
                        **mesh("build_gable"),
                        **body([4.5, 1, 0.1], shape="compound"),
                        "body.compound-count": len(gableBoxes),
                        "body.compound-box": gableBoxes,
                        "rotation": [math.sqrt(0.5), 0, math.sqrt(0.5), 0],
                        "tags": 64,
                    },
                )
            )
        for k, (kind, x, z) in enumerate(
            [
                ("bed", -4, -2.7),
                ("chest", -4, 2.5),
                ("workbench", 3, -2.6),
                ("chair", -0.8, 1),
                ("table", 1, 0.5),
                ("sofa", 3, 2.5),
                ("cupboard", 4.9, -0.7),
                ("shelf", -5.1, -0.3),
                ("lamp", 2, -1.1),
                ("rug", 0, -1.2),
                ("painting", 0, -4.3),
            ]
        ):
            i = base + 100 + k
            d = CATALOG[kind]
            extra = {}
            if d.get("use") == "lamp":
                extra = {
                    "light": {"symbol": "point"},
                    "light.color": [1, 0.73, 0.4],
                    "light.range": 6,
                    "light.intensity": 2,
                    "light.cone": [20, 35],
                }
            add(
                i,
                label + " / " + d["name"],
                [
                    cx + x,
                    floorY + (1.6 if kind == "painting" else d["half"][1] + 0.015),
                    cz + z,
                ],
                kind,
                **extra,
            )
            fixtures.append(
                {
                    "entity": str(i),
                    "kind": kind,
                    "owned": False,
                    "movable": True,
                    "support": str(base),
                    "stock": (
                        {"planks": 8, "nails": 8, "scrap": 6, "rations": 2}
                        if kind == "chest" and n == 0
                        else {"fiber": 8, "bandage": 2} if kind == "cupboard" else {}
                    ),
                }
            )
        if n == 0:
            i = base + 120
            add(
                i,
                "Packing crate / clear the entrance",
                [dx, floorY + 0.415, dz + 1],
                "chest",
            )
            fixtures.append(
                {
                    "entity": str(i),
                    "kind": "chest",
                    "owned": False,
                    "movable": True,
                    "support": str(base),
                    "stock": {"planks": 2, "nails": 2},
                }
            )
        # Real 22 cm steps connect raised floors to uneven exterior terrain.
        for step in range(24):
            z = dz + 0.28 + step * 0.46
            top = floorY - 0.22 * (step + 1)
            baseGround = ground(dx, z)
            if top <= baseGround + 0.03:
                break
            half = (top - baseGround) / 2
            rows.append(
                obj(
                    base + 130 + step,
                    label + " / entrance step",
                    [dx, baseGround + half, z],
                    **{
                        **mesh("build_floor"),
                        **body([0.9, half, 0.23]),
                        "scale": [0.6, half / 0.1, 0.23 / 1.5],
                        "tags": 64,
                    },
                )
            )
        homes.append(
            {
                "name": label,
                "floor": str(base),
                "bed": str(base + 100),
                "position": [cx, floorY, cz],
                "door": str(leaf),
                "frame": str(frame),
            }
        )
    # Ordinary reusable construction prefabs and authored non-physical previews.
    prefabs = {}
    previews = {}
    items = json.loads((ROOT / "Authoring/items.json").read_text())
    for k, (kind, d) in enumerate(CATALOG.items()):
        assert (
            len(d["half"]) == 3 and all(v > 0 for v in d["half"]) and d["health"] > 0
        ), kind
        assert all(
            k in items and isinstance(n, int) and n > 0 for k, n in d["cost"].items()
        ), kind
        model = ROOT / f"Assets/models/build_{kind}.glb"
        if not model.exists():
            raise RuntimeError("Provide a centred mesh for " + kind)
        meta(model, "mesh", "model/build_" + kind)
        root = obj(
            1, d["name"], **{**mesh("build_" + kind), **collision(kind), "tags": 64}
        )
        if d.get("use") == "lamp":
            root["fields"].update(
                {
                    "light": {"symbol": "point"},
                    "light.color": [1, 0.73, 0.4],
                    "light.range": 6,
                    "light.intensity": 2,
                    "light.cone": [20, 35],
                }
            )
        children = []
        if kind in ["door", "gate"]:
            h = 1.175 if kind == "door" else 0.475
            children = [
                obj(
                    2,
                    "Physical hinged leaf",
                    [0, -0.15, 0],
                    **{
                        "parent": 1,
                        **mesh("build_" + kind + "_leaf"),
                        **body([0.65, h, 0.055], mass=18, motion="dynamic"),
                        "tags": 64,
                    },
                )
            ]
        path = ROOT / f"Assets/prefabs/build_{kind}.judasprefab"
        write(
            path,
            {
                "kind": "prefab",
                "schema": 1,
                "data": {
                    "settings": {**settings, "world-origin": [0, 0, 0], "next-id": 3},
                    "objects": [root, *children],
                },
            },
        )
        prefabs[kind] = meta(path, "prefab", "build/" + kind)
        i = 5000 + k
        rows.append(
            obj(
                i,
                "Placement preview / " + d["name"],
                [0, -200, 0],
                **{
                    **mesh("build_" + kind),
                    "render-visible": False,
                    "render.alpha": 0.45,
                },
            )
        )
        previews[kind] = str(i)
    for kind in ["wood", "metal"]:
        path = ROOT / f"Assets/prefabs/barricade_{kind}.judasprefab"
        root = obj(
            1,
            "Barricade / " + kind,
            **{
                **mesh("barricade_" + kind),
                **body([0.8, 0.55, 0.08], mass=12, motion="dynamic"),
                "tags": 64,
            },
        )
        write(
            path,
            {
                "kind": "prefab",
                "schema": 1,
                "data": {
                    "settings": {**settings, "world-origin": [0, 0, 0], "next-id": 2},
                    "objects": [root],
                },
            },
        )
        prefabs["barricade_" + kind] = meta(path, "prefab", "build/barricade_" + kind)
    settings["next-id"] = 6000
    write(scene, j | {"data": {"settings": settings, "objects": rows}})
    for path in (ROOT / "Assets/scripts").glob("*.js"):
        meta(path, "script")
    project = json.loads((ROOT / "lunar_island.judasproj").read_text())
    d = project["data"]
    d["runtimeAssets"] = [ident("loot-prefab"), *prefabs.values()]
    d["saveIdentity"] = (
        __import__("hashlib").sha256(b"LunarIsland-Stage2-World-v2").hexdigest()
    )
    entries = d["classification"]["tags"]["entries"]
    entries = [e for e in entries if e["id"] < 6] + [{"id": 6, "name": "buildable"}]
    d["classification"]["tags"].update(entries=entries, nextId=7)
    d["input"] = [
        e for e in d["input"] if e["name"] not in ["build_menu", "build_rotate"]
    ] + [
        {
            "name": name,
            "axis": False,
            "bindings": [{"control": key, "scale": 1, "deadzone": 0}],
        }
        for name, key in [("build_menu", "key:B"), ("build_rotate", "key:R")]
    ]
    write(ROOT / "lunar_island.judasproj", project)
    data = {
        "schema": 2,
        "catalog": CATALOG,
        "prefabs": prefabs,
        "previews": previews,
        "fixtures": fixtures,
        "doors": doors,
        "sockets": sockets,
        "homes": homes,
    }
    write(ROOT / "Authoring/housing_manifest.json", data)
    (ROOT / "Assets/scripts/housing-data.js").write_text(
        "export const HOUSING = " + json.dumps(data, indent=2) + ";\n"
    )
    meta(ROOT / "Assets/scripts/housing-data.js", "script")
    ui = json.loads((ROOT / "Assets/ui/game.judasui").read_text())
    es = ui["data"]["elements"]
    es = [
        e
        for e in es
        if e["id"] not in ["entries2", "secondary", "tertiary", "tab_build"]
    ]
    import copy

    def clone(source, new, **changes):
        e = copy.deepcopy(next(e for e in es if e["id"] == source))
        e.update(id=new, **changes)
        es.append(e)

    clone("entries", "entries2", visible=False, offset=[568, 160], size=[500, 220])
    clone("primary", "secondary", visible=False, text="Take one", offset=[568, 405])
    clone("drop", "tertiary", visible=False, text="Take stack")
    for e in es:
        if e["id"].startswith("tab_"):
            e["size"] = [132, 40]
        if e["id"] == "tab_pack":
            e["offset"] = [24, 72]
        if e["id"] == "tab_craft":
            e["offset"] = [168, 72]
        if e["id"] == "tab_map":
            e["offset"] = [456, 72]
        if e["id"] == "tab_help":
            e["offset"] = [600, 72]
        if e["id"] == "chapter":
            e["text"] = "HOMESTEAD / MAKE A HOUSE YOUR HOME"
    clone("tab_craft", "tab_build", text="Build / furnish", offset=[312, 72])
    ui["data"]["elements"] = es
    write(ROOT / "Assets/ui/game.judasui", ui)
    print(
        "Stage 2:",
        len(homes),
        "enterable houses;",
        len(fixtures),
        "functional fixtures;",
        len(CATALOG),
        "construction/furniture recipes",
    )


if __name__ == "__main__":
    ap = argparse.ArgumentParser()
    ap.add_argument("--prompt3d", type=Path)
    ap.add_argument("--art-only", action="store_true")
    args = ap.parse_args()
    if args.prompt3d:
        author_art(args.prompt3d)
    if not args.art_only:
        author_world()
