#!/usr/bin/env python3
"""Stage 5's streamed districts and resident road network. Project-only authoring.

Does not regenerate or overwrite Stage 1–4 art/content. Regional IDs are remapped
at runtime; all game references use the public stable region identity.
"""

import argparse, copy, json, math, sys, subprocess, hashlib, random
from pathlib import Path
from author_project import (
    ROOT,
    REPO,
    ident,
    meta,
    write,
    obj,
    mesh,
    body,
    interpolate,
    ORIGIN,
    terrain_source,
)

DEFS = json.loads((ROOT / "Authoring/regions.json").read_text())
TERRAINS = [
    json.loads((ROOT / f"Authoring/{k}.judasterrain").read_text())
    for k in ["coast_0", "coast_1", "homestead"]
]


def ground(x, z):
    if abs(x) < 320 and abs(z) < 320:
        return interpolate(TERRAINS[2], x, z)
    X, Z = x + ORIGIN[0], z + ORIGIN[1]
    return interpolate(TERRAINS[0 if X < 0 else 1], X - (-2800 if X < 0 else 2800), Z)


def source_art(tool):
    sys.dont_write_bytecode = True
    sys.path.insert(0, str(tool))
    from prompt3d.pipeline import generate, build_styled
    from prompt3d.export import to_glb
    from prompt3d import scans

    scans.GEN_DIR = str(ROOT / "ArtSource/no-texture-overrides")
    catalog = json.loads((ROOT / "ArtSource/asset_catalog.json").read_text())
    for assetIndex, (k, prompt, cat, params) in enumerate(
        [
            (
                "town_shop",
                "a realistic corner shop",
                "shop",
                {"type": "corner shop", "sign": "MOONBRIDGE"},
            ),
            (
                "clinic",
                "a realistic pharmacy",
                "shop",
                {"type": "pharmacy", "sign": "CLINIC"},
            ),
            (
                "city_tower",
                "a realistic concrete building",
                "building",
                {"type": "stepped", "floors": 5, "material": "concrete", "size": 0.45},
            ),
            (
                "market",
                "a realistic supermarket",
                "shop",
                {"type": "supermarket", "sign": "LUNAR STORES"},
            ),
            (
                "lighthouse",
                "a realistic lighthouse",
                "prop",
                {"type": "lighthouse", "size": 2.5},
            ),
        ]
    ):
        seed = 91557 + assetIndex
        receipt = generate(
            prompt,
            seed,
            str(ROOT / "ArtSource/stage5-prompt3d"),
            obj=False,
            rewrite=False,
        )
        p = copy.deepcopy(receipt["params"])
        p.update(params)
        sc = build_styled(cat, p, seed, "realistic")
        lo, hi = sc.bounds()
        sc.transform_all(None, (-(lo[0] + hi[0]) / 2, -lo[1], -(lo[2] + hi[2]) / 2))
        target = ROOT / f"Assets/models/region_{k}.glb"
        to_glb(sc, str(target), textures=True, contrast=1, flat=False)
        meta(target, "mesh", "model/region_" + k)
        lo, hi = sc.bounds()
        catalog["region_" + k] = {
            "path": str(target.relative_to(ROOT)),
            "bounds": [lo.tolist(), hi.tolist()],
            "prompt": prompt,
            "seed": seed,
            "generator": cat,
            "parameters": p,
            "sha256": hashlib.sha256(target.read_bytes()).hexdigest(),
        }
    write(ROOT / "ArtSource/asset_catalog.json", catalog)


def main(tool):
    sys.dont_write_bytecode = True
    sys.path.insert(0, str(tool))
    import numpy as np
    from prompt3d import geometry as G
    from prompt3d.export import to_glb

    root = json.loads((ROOT / "Scenes/island.judas").read_text())
    root["data"]["objects"] = [
        o for o in root["data"]["objects"] if not 8000 <= o["id"] < 10000
    ]
    objects = root["data"]["objects"]
    settings = copy.deepcopy(root["data"]["settings"])
    housing = json.loads((ROOT / "Authoring/housing_manifest.json").read_text())
    people = json.loads((ROOT / "Authoring/encounter_manifest.json").read_text())
    # The existing streaming contract accepts runtime root rigid/render members,
    # not a new prefab hierarchy. Bake the old visual-child offset into an
    # ordinary glTF parent node and author an equivalent one-entity corpse.
    from author_stage3 import read_glb, write_glb

    model = ROOT / "Assets/models/region_human_corpse.glb"
    j, b = read_glb(ROOT / "Assets/models/npc_bandit.glb")
    sceneIndex = j.get("scene", 0)
    children = j["scenes"][sceneIndex]["nodes"]
    newRoot = len(j["nodes"])
    j["nodes"].append(
        {
            "name": "Corpse origin offset",
            "translation": [0, -0.8, 0],
            "children": children,
        }
    )
    j["scenes"][sceneIndex]["nodes"] = [newRoot]
    write_glb(model, j, b)
    meta(model, "mesh", "model/region_human_corpse")
    prefab = json.loads((ROOT / "Assets/prefabs/human_corpse.judasprefab").read_text())
    corpse = prefab["data"]["objects"][0]
    corpse["fields"].update(mesh("region_human_corpse"))
    prefab["data"]["objects"] = [corpse]
    prefabPath = ROOT / "Assets/prefabs/region_human_corpse.judasprefab"
    write(prefabPath, prefab)
    people["regionHumanCorpse"] = meta(
        prefabPath, "prefab", "prefab/region_human_corpse"
    )
    catalog = json.loads((ROOT / "ArtSource/asset_catalog.json").read_text())
    catalog["region_human_corpse"] = {
        "path": str(model.relative_to(ROOT)),
        "source": "Original NPC bandit GLB; one glTF parent offsets the visual -0.8 m, matching the original corpse child. Source author_stage5.py.",
        "sourceSha256": hashlib.sha256(
            (ROOT / "Assets/models/npc_bandit.glb").read_bytes()
        ).hexdigest(),
        "sha256": hashlib.sha256(model.read_bytes()).hexdigest(),
    }
    write(ROOT / "ArtSource/asset_catalog.json", catalog)
    basePeople = copy.deepcopy(people)
    people["weapons"] = json.loads((ROOT / "Authoring/encounters.json").read_text())[
        "weapons"
    ]
    people["actors"] = {
        k: v for k, v in people["actors"].items() if not k.startswith("region_")
    }
    people["quests"] = {
        k: v
        for k, v in people["quests"].items()
        if k not in ["forestry", "clinic", "harbour", "radio", "islet"]
    }
    # Navigation sources are intentionally isolated per district collision layer.
    # This is normal authored filtering: root navigation does not consume streamed
    # city geometry; each district only consumes its own actual walkable bodies.
    for o in objects:
        if o["id"] == 6900:
            o["fields"]["nav.surface.sources"] = "1"
    routes = [
        (r["name"], r["points"])
        for r in json.loads((ROOT / "Authoring/routes.json").read_text())
    ]
    deckHeights = {
        k: (
            8.12
            if k == "islet"
            else max(
                ground(d["center"][0] + x, d["center"][1] + z)
                for x in [-60, 0, 60]
                for z in [-55, 0, 55]
            )
            + 0.35
        )
        for k, d in DEFS.items()
    }

    def roadHeight(x, z):
        h = ground(x, z) + 0.065
        for k, d in DEFS.items():
            if k == "islet":
                continue
            r = math.hypot(x - d["center"][0], z - d["center"][1])
            t = max(0, min(1, (145 - r) / 80))
            h = max(h, h * (1 - t) + (deckHeights[k] + 0.025) * t)
        return h

    roads = []
    for n, (name, points) in enumerate(routes):
        V = []
        F = []
        length = 0
        for a, b in zip(points, points[1:]):
            dist = math.dist(a, b)
            length += dist
            count = math.ceil(dist / 2)
            dx, dz = (b[0] - a[0]) / dist, (b[1] - a[1]) / dist
            for i in range(count + 1):
                x, z = (
                    a[0] + (b[0] - a[0]) * i / count,
                    a[1] + (b[1] - a[1]) * i / count,
                )
                for s in [-1, 1]:
                    xx, zz = x - dz * 3.4 * s, z + dx * 3.4 * s
                    V.append([xx, roadHeight(xx, zz), zz])
                if i:
                    k = len(V) - 4
                    F.extend([[k, k + 1, k + 2], [k + 1, k + 3, k + 2]])
        sc = G.Scene(name)
        sc.material("road", "#706c61", roughness=1)
        sc.add(name, "road", G.Prim(V, [[0, 1, 0]] * len(V), F))
        p = ROOT / f"Assets/models/road_{n}.glb"
        to_glb(sc, str(p), textures=False)
        aid = meta(p, "mesh", "road/" + str(n))
        c = ROOT / f"Assets/collision/road_{n}.judascollision"
        c.parent.mkdir(exist_ok=True)
        subprocess.run(
            [
                str(REPO / "build/judas_collision_cook"),
                "mesh",
                str(p),
                str(c),
                aid,
                "0",
                "--two-sided",
            ],
            check=True,
        )
        cid = meta(c, "collision", "collision/road/" + str(n))
        objects.append(
            obj(
                8000 + n,
                name,
                **{
                    **mesh("unused"),
                    **body([1, 1, 1], shape="triangle-mesh"),
                    "render.mesh-asset": aid,
                    "body.collision-asset": cid,
                    "body.collision-layer": 2,
                    "tags": 1,
                },
            )
        )
        roads.append({"name": name, "points": points, "length": length})
    # One common physical street slab and open shop/warehouse shell are authored
    # with real door/window gaps using the existing reusable cottage modules.
    regions = {}
    world = [
        "JudasWorld 1",
        "budget 2 16 2 16777216 67108864 67108864 1073741824",
        'region "transport_garage" "Scenes/transport_garage.judas" 2900 0 -1600 0 0 0 1 120 40 120 1 "snapshot" 524288 ""',
    ]
    regionVehicles = {}
    for index, (key, d) in enumerate(DEFS.items()):
        cx, cz = d["center"]
        base = 10000 + index * 10000 if index < 7 else 1000000 + (index - 7) * 10000
        layer = 3 + index
        cy = (
            max(ground(cx + x, cz + z) for x in [-60, 0, 60] for z in [-55, 0, 55])
            + 0.35
        )
        if key == "islet":
            cy = 8.12
        rows = []
        fixtures = []
        sockets = []
        doors = []
        resources = []
        nextId = base

        def add(name, p, fields):
            nonlocal nextId
            nextId += 1
            o = obj(nextId, name, p, **fields)
            rows.append(o)
            return o

        def box(name, p, h, color=(0.5, 0.5, 0.45), tags=64):
            return add(
                name,
                p,
                {
                    **body(h),
                    "render": {"symbol": "box"},
                    "render.half-extents": h,
                    "render.color": list(color),
                    "render.alpha": 1,
                    "render.radius": max(h),
                    "render.secondary-color": [0.8, 0.8, 0.8],
                    "render.secondary-alpha": 1,
                    "render.texture-asset": "",
                    "render.mesh-asset": "",
                    "tags": tags,
                },
            )

        # A continuous elevated street/courtyard, supported visibly by retaining
        # walls; its ramps meet real terrain at 70–110 m from the district centre.
        box(
            d["name"] + " / retained street",
            (cx, cy - 0.3, cz),
            (64, 0.3, 58),
            (0.35, 0.36, 0.34),
            1,
        )
        for side in [-1, 1]:
            for axis in [0, 1]:
                x0, z0 = cx + (64 * side if axis == 0 else 0), cz + (
                    58 * side if axis else 0
                )
                x1, z1 = cx + (105 * side if axis == 0 else 0), cz + (
                    105 * side if axis else 0
                )
                y1 = ground(x1, z1) + 0.08
                A = np.array([x0, cy, z0])
                B = np.array([x1, y1, z1])
                delta = B - A
                sc = G.Scene("Access ramp")
                sc.material("ramp", "#77776c", roughness=1)
                sc.add(
                    "ramp",
                    "ramp",
                    G.box(7, 0.25, float(np.linalg.norm(delta))),
                    G.align_y([0, 1, 0]) if False else None,
                )
                # Box local Z is the ramp direction; native quaternion matches.
                yaw = math.atan2(delta[0], delta[2])
                pitch = -math.atan2(delta[1], math.hypot(delta[0], delta[2]))
                q = [
                    math.cos(yaw / 2) * math.cos(pitch / 2),
                    math.cos(yaw / 2) * math.sin(pitch / 2),
                    math.sin(yaw / 2) * math.cos(pitch / 2),
                    -math.sin(yaw / 2) * math.sin(pitch / 2),
                ]
                r = box(
                    "Street approach",
                    (A + B) / 2,
                    [3.5, 0.125, float(np.linalg.norm(delta)) / 2],
                    (0.4, 0.39, 0.35),
                    1,
                )
                r["fields"]["rotation"] = q
        if key == "islet":
            heights = []
            for iz in range(65):
                for ix in range(65):
                    r = math.hypot(ix * 360 / 64 - 180, iz * 360 / 64 - 180)
                    heights.append(8 if r < 100 else max(-45, 8 - (r - 100) * 0.5))
            t = terrain_source("lantern_islet", 360, 360, 65, 65, heights)
            write(ROOT / "Authoring/lantern_islet.judasterrain", t)
            subprocess.run(
                [
                    str(REPO / "build/judas_terrain_author"),
                    "cook",
                    str(ROOT / "lunar_island.judasproj"),
                    "Authoring/lantern_islet.judasterrain",
                ],
                check=True,
            )
            o = add(
                "Editable Lantern Islet terrain",
                [cx, 0, cz],
                {
                    **body([180, 60, 180], shape="triangle-mesh"),
                    **mesh("unused"),
                    "render.mesh-asset": t["meshId"],
                    "render.texture-asset": t["textureId"],
                    "body.collision-asset": t["collisionId"],
                    "tags": 1,
                },
            )
        # Houses are full editable modules, furniture, opening frames and real
        # hinged door leaves, not solid generated shop façades used as interiors.
        for h in range(d["houses"]):
            hx = cx + (-32 if h % 2 == 0 else 32)
            hz = cz - 32 + (h // 2) * 29
            delta = [hx + 17, cy - 14.18, hz + 10]
            old = [o for o in objects if 3000 <= o["id"] < 3200]
            ids = {o["id"]: base + 1000 + h * 200 + o["id"] - 3000 for o in old}
            floor = (
                next(o for o in old if o["id"] == 3000)["fields"]["position"][1] + 0.1
            )
            delta[1] = cy + 0.02 - floor
            for src in old:
                o = copy.deepcopy(src)
                o["id"] = ids[src["id"]]
                o["name"] = (
                    d["name"] + f" / house {h+1} / " + src["name"].split(" / ")[-1]
                )
                o["fields"]["position"] = [
                    v + a for v, a in zip(o["fields"]["position"], delta)
                ]
                rows.append(o)
            for f in housing["fixtures"]:
                if int(f["entity"]) not in ids:
                    continue
                f = copy.deepcopy(f)
                f["entity"] = "region:" + key + ":" + str(ids[int(f["entity"])])
                f["support"] = "region:" + key + ":" + str(ids[int(f["support"])])
                f["stock"] = copy.deepcopy(
                    {**d["stock"], "planks": 6, "nails": 6}
                    if f["kind"] == "chest" and h == 0
                    else {"rations": 1} if f["kind"] == "cupboard" else {}
                )
                fixtures.append(f)
            for socket in housing["sockets"]:
                if int(socket["entity"]) not in ids:
                    continue
                socket = copy.deepcopy(socket)
                socket["entity"] = (
                    "region:" + key + ":" + str(ids[int(socket["entity"])])
                )
                socket["position"] = [v + a for v, a in zip(socket["position"], delta)]
                if socket.get("door"):
                    socket["door"] = (
                        "region:" + key + ":" + str(ids[int(socket["door"])])
                    )
                sockets.append(socket)
            for door in housing["doors"]:
                if int(door["entity"]) in ids:
                    door = copy.deepcopy(door)
                    door["entity"] = (
                        "region:" + key + ":" + str(ids[int(door["entity"])])
                    )
                    door["frame"] = "region:" + key + ":" + str(ids[int(door["frame"])])
                    doors.append(door)
        # Distinct skyline/façade dressing outside the usable houses. Coarse
        # authored collision remains solid; these models are explicitly sealed.
        if d["theme"] in ["city", "town", "industry"]:
            for j in range(10 if key == "city" else 3):
                name = (
                    "city_tower"
                    if key == "city"
                    else ["town_shop", "clinic", "market"][j % 3]
                )
                aid = ident("model/region_" + name)
                cat = json.loads((ROOT / "ArtSource/asset_catalog.json").read_text())[
                    "region_" + name
                ]
                lo, hi = cat["bounds"]
                x = cx - 100 + (j % 5) * 45
                z = cz - 95 - (j // 5) * 45
                y = ground(x, z)
                o = add(
                    "Sealed exterior / " + name,
                    [x, y, z],
                    {
                        **mesh("region_" + name),
                        **body(
                            [
                                (hi[0] - lo[0]) / 2,
                                (hi[1] - lo[1]) / 2,
                                (hi[2] - lo[2]) / 2,
                            ]
                        ),
                    },
                )
                o["fields"]["body.compound-count"] = 1
                o["fields"]["body"] = {
                    "motion": {"symbol": "static"},
                    "shape": {"symbol": "compound"},
                }
                o["fields"]["body.compound-box"] = [
                    {
                        "center": [0, (hi[1] - lo[1]) / 2, 0],
                        "halfExtents": [
                            (hi[0] - lo[0]) / 2,
                            (hi[1] - lo[1]) / 2,
                            (hi[2] - lo[2]) / 2,
                        ],
                    }
                ]
        if key == "islet":
            cat = json.loads((ROOT / "ArtSource/asset_catalog.json").read_text())[
                "region_lighthouse"
            ]
            lo, hi = cat["bounds"]
            add(
                "Lantern lighthouse / sealed tower",
                [cx + 22, cy, cz - 30],
                {
                    **mesh("region_lighthouse"),
                    **body([2, (hi[1] - lo[1]) / 2, 2], shape="compound"),
                    "body.compound-count": 1,
                    "body.compound-box": [
                        {
                            "center": [0, (hi[1] - lo[1]) / 2, 0],
                            "halfExtents": [2, (hi[1] - lo[1]) / 2, 2],
                        }
                    ],
                },
            )
        rng = random.Random(91520 + index)
        for j in range(d.get("vegetationCount", 24)):
            a = rng.uniform(0, math.tau)
            r = rng.uniform(115, 190)
            x, z = cx + math.sin(a) * r, cz + math.cos(a) * r
            y = ground(x, z) if key != "islet" else max(-45, 8 - max(0, r - 100) * 0.5)
            if y < 1 and key != "islet":
                continue
            nearest = 1e9
            for road in roads:
                for a, b in zip(road["points"], road["points"][1:]):
                    dx, dz = b[0] - a[0], b[1] - a[1]
                    t = max(
                        0,
                        min(
                            1, ((x - a[0]) * dx + (z - a[1]) * dz) / (dx * dx + dz * dz)
                        ),
                    )
                    nearest = min(
                        nearest, math.hypot(x - a[0] - t * dx, z - a[1] - t * dz)
                    )
            if nearest < 8:
                continue
            kind = "tree" if j % 3 else "ore" if j % 6 == 0 else "rock"
            height = d.get("treeHeight", 5) if kind == "tree" else 1.2
            half = [0.3, height / 2, 0.3] if kind == "tree" else [0.85, 0.6, 0.75]
            o = add(
                "Harvestable " + kind,
                [x, y + height / 2, z],
                {
                    **(mesh("rock") if kind != "tree" else {}),
                    **body(half, 100 if kind == "tree" else 160),
                    "tags": 2 if kind == "tree" else 4,
                },
            )
            if kind == "tree":
                visual = add(
                    "Tree canopy / follows physical trunk",
                    [0, -height / 2, 0],
                    {
                        **mesh("coastal_pine"),
                        "parent": o["id"],
                        "scale": [height / 9.52] * 3,
                    },
                )
            resources.append(
                {
                    "id": "region:" + key + ":" + str(o["id"]),
                    "local": str(o["id"]),
                    "kind": kind,
                    "amount": 8,
                    "height": height,
                    "position": o["fields"]["position"],
                }
            )
        # Three genuine salvage piles on the courtyard use ordinary E harvesting.
        for j in range(3):
            o = add(
                "Recoverable yard salvage",
                [cx - 9 + j * 4, cy + 0.45, cz + 24],
                {**mesh("wood_crate"), **body([0.55, 0.45, 0.5]), "tags": 4},
            )
            resources.append(
                {
                    "id": "region:" + key + ":" + str(o["id"]),
                    "local": str(o["id"]),
                    "kind": "salvage",
                    "amount": 4,
                    "height": 0,
                    "position": o["fields"]["position"],
                }
            )
        # Existing shared NPC behaviour: finite trade/quests, physical CharacterMotor
        # patrol and combat, native navigation only on the district's real surfaces.
        names = {
            "forest": "Rowan / Forester",
            "town": "Asha / Medic",
            "harbour": "Ellis / Harbourmaster",
            "city": "Nia / Radio Operator",
            "coast": "Tess / Coastguard",
            "islet": "Sol / Lighthouse Keeper",
            "industry": "Works scavenger",
        }
        count = 3 if d.get("hostile") else 1
        for j in range(count):
            npcKey = "region_" + key + "_" + str(j)
            friendly = j == 0 and key != "industry"
            template = copy.deepcopy(
                basePeople["actors"]["mara" if friendly else "bandit_guard"]
            )
            template.update(
                entity=base + 5000 + j * 10,
                name=(
                    d.get("npcName", names.get(key, d["name"] + " survivor"))
                    if j == 0
                    else d["name"] + " raider"
                ),
                friendly=friendly,
                health=85,
                speed=1.3 if friendly else 2.6,
                position=[
                    cx + (-8 if j == 0 else 12 + j * 3),
                    cy + 0.06,
                    cz + (-12 if j == 0 else 15),
                ],
                route=[
                    [cx - 8, cy + 0.06, cz - 12],
                    [cx + 7, cy + 0.06, cz - 12],
                    [cx + 7, cy + 0.06, cz + 8],
                ],
                stock=(
                    {
                        "tokens": 100,
                        "fuel_can": 4,
                        "rations": 4,
                        "bandage": 3,
                        "ammo_pistol": 20,
                    }
                    if friendly
                    else {"scrap": 3, "tokens": 8, "ammo_rifle": 10}
                ),
                prices=(
                    {
                        "wood": 2,
                        "scrap": 3,
                        "iron_ore": 4,
                        "fuel_can": 8,
                        "rations": 4,
                        "bandage": 5,
                        "ammo_pistol": 1,
                    }
                    if friendly
                    else {}
                ),
                lines=[
                    d["description"],
                    "Supplies are finite. You can turn any unoccupied house here into your own home.",
                    "Follow the surfaced roads. Ships and aircraft can reach the outlying coast.",
                ],
            )
            template.pop("quest", None)
            if friendly and d.get("quest"):
                template["quest"] = d["quest"]
            people["actors"][npcKey] = template
            src = copy.deepcopy(
                next(o for o in objects if o["id"] == (6000 if friendly else 6210))
            )
            src["id"] = template["entity"]
            src["fields"]["position"] = template["position"]
            src["fields"]["script.0.properties"] = json.dumps({"key": npcKey})
            rows.append(src)
            v = copy.deepcopy(
                next(o for o in objects if o["id"] == (6001 if friendly else 6211))
            )
            v["id"] = src["id"] + 1
            v["fields"]["parent"] = src["id"]
            v["fields"]["position"] = template["position"]
            rows.append(v)
        if key in ["harbour", "islet"]:
            # Dock descends to the real prescribed ocean; collision is a real ramp.
            dockZ = cz + 145
            dockY = 0.9
            box(
                "Working timber pier",
                [cx, dockY - 0.15, dockZ],
                [3, 0.15, 22],
                (0.45, 0.35, 0.24),
                1,
            )
            A = np.array([cx, cy, cz + 58])
            B = np.array([cx, dockY, dockZ - 22])
            delta = B - A
            ramp = box(
                "Harbour slipway",
                (A + B) / 2,
                [3, 0.15, float(np.linalg.norm(delta)) / 2],
                (0.45, 0.39, 0.3),
                1,
            )
            pitch = -math.atan2(delta[1], delta[2])
            ramp["fields"]["rotation"] = [
                math.cos(pitch / 2),
                math.sin(pitch / 2),
                0,
                0,
            ]
            vehicleKey = "region_" + key + "_boat"
            boatDef = copy.deepcopy(
                json.loads((ROOT / "Authoring/vehicles.json").read_text())["boat"]
            )
            boatDef.update(
                entity=base + 6000,
                name=d["name"] + " ocean skiff",
                region=key,
                position=[cx + 4.2, dockZ],
                height=0.05,
                stock={"fuel_can": 2, "rations": 2},
            )
            regionVehicles[vehicleKey] = boatDef
            prefab = json.loads(
                (ROOT / "Assets/prefabs/vehicle_boat.judasprefab").read_text()
            )["data"]["objects"]
            idmap = {o["id"]: base + 6000 + o["id"] - 1 for o in prefab}
            for src in prefab:
                o = copy.deepcopy(src)
                o["id"] = idmap[src["id"]]
                o["fields"]["position"] = [cx + 4.2, 0.05, dockZ]
                if "parent" in o["fields"]:
                    o["fields"]["parent"] = idmap[o["fields"]["parent"]]
                if "powertrain.graph" in o["fields"]:
                    graph = json.loads(o["fields"]["powertrain.graph"])
                    for shaft in graph["shafts"]:
                        for field in ["bearing", "target"]:
                            old = int(shaft.get(field, "0"))
                            if old in idmap:
                                shaft[field] = str(idmap[old])
                    o["fields"]["powertrain.graph"] = json.dumps(graph)
                if "scripts" in o["fields"]:
                    o["fields"]["script.0.properties"] = json.dumps({"key": vehicleKey})
                rows.append(o)
        nav = copy.deepcopy(next(o for o in objects if o["id"] == 6900))
        nav["id"] = base + 9000
        nav["fields"].update(
            position=[cx, cy + 10, cz],
            **{
                "nav.surface.asset": "",
                "nav.surface.halfExtents": "115 60 115",
                "nav.surface.cellSize": ".5",
                "nav.surface.sources": str(1 << layer),
            },
        )
        rows.append(nav)
        add(
            "Regional gameplay binding",
            [cx, cy, cz],
            {
                "scripts": 1,
                "script.0.id": 1,
                "script.0.asset": ident("script/region-content.js"),
                "script.0.enabled": True,
                "script.0.properties": json.dumps({"key": key}),
            },
        )
        mutable = {int(n["local"]) for n in resources} | {
            int(f["entity"].rsplit(":", 1)[1]) for f in fixtures if f.get("movable")
        }
        for o in rows:
            if "body" in o["fields"]:
                o["fields"]["body.collision-layer"] = 2 if o["id"] in mutable else layer
            if o["fields"].get("parent"):
                if (
                    o["id"] in [base + 5001 + j * 10 for j in range(count)]
                    or o["id"] == base + 6001
                ):
                    o["fields"]["position"] = [0, 0, 0]
            else:
                o["fields"]["position"] = [
                    v - a for v, a in zip(o["fields"]["position"], [cx, cy, cz])
                ]
        regionSettings = copy.deepcopy(settings)
        regionSettings["name"] = d["name"]
        regionSettings["world-origin"] = [ORIGIN[0] + cx, cy, ORIGIN[1] + cz]
        regionSettings["next-id"] = base + 10000
        path = ROOT / f"Scenes/region_{key}.judas"
        write(
            path,
            {
                "kind": "scene",
                "schema": 1,
                "data": {"settings": regionSettings, "objects": rows},
            },
        )
        regions[key] = {
            **d,
            "position": [cx, cy, cz],
            "nav": nav["id"],
            "fixtures": fixtures,
            "sockets": sockets,
            "doors": doors,
            "resources": resources,
            "scene": str(path.relative_to(ROOT)),
            "layer": layer,
            "objectCount": len(rows),
            "primaryChest": next(f["entity"] for f in fixtures if f["kind"] == "chest"),
            "friendly": "region_" + key + "_0" if key != "industry" else None,
        }
        world.append(
            f'region "{key}" "Scenes/region_{key}.judas" {ORIGIN[0]+cx} {cy} {ORIGIN[1]+cz} 0 0 0 1 220 500 220 1 "snapshot" 2097152 ""'
        )
        print(key, cy, len(rows))
    # Roadside wilderness is also ordinary streamed content. It carries real
    # harvest state without adding permanently simulated trees to the root scene.
    patch = 0
    for road in roads:
        for a, b in zip(road["points"], road["points"][1:]):
            dist = math.dist(a, b)
            for step in range(1, int(dist / 650) + 1):
                t = step / (int(dist / 650) + 1)
                cx = a[0] + (b[0] - a[0]) * t
                cz = a[1] + (b[1] - a[1]) * t
                if min(math.dist([cx, cz], d["center"]) for d in DEFS.values()) < 350:
                    continue
                key = "wild_" + str(patch)
                base = 80000 + patch * 1000
                patch += 1
                cy = ground(cx, cz)
                rows = []
                res = []
                rng = random.Random(91600 + patch)
                for j in range(30):
                    angle = rng.random() * math.tau
                    r = rng.uniform(25, 95)
                    x, z = cx + math.cos(angle) * r, cz + math.sin(angle) * r
                    y = ground(x, z)
                    # Leave a genuinely drivable corridor along the road centreline.
                    cross = (
                        abs((b[0] - a[0]) * (z - a[1]) - (b[1] - a[1]) * (x - a[0]))
                        / dist
                    )
                    if y < 1 or cross < 10:
                        continue
                    local = base + j * 2
                    kind = "tree" if j % 4 else "rock"
                    height = 5 if kind == "tree" else 1.2
                    fields = {
                        **body(
                            (
                                [0.25, height / 2, 0.25]
                                if kind == "tree"
                                else [0.8, 0.6, 0.7]
                            ),
                            100,
                        ),
                        "tags": 2 if kind == "tree" else 4,
                        "body.collision-layer": 2,
                    }
                    if kind != "tree":
                        fields.update(mesh("rock"))
                    rows.append(
                        obj(
                            local,
                            "Roadside " + kind,
                            [x - cx, y + height / 2 - cy, z - cz],
                            **fields,
                        )
                    )
                    if kind == "tree":
                        rows.append(
                            obj(
                                local + 1,
                                "Roadside canopy",
                                [0, -height / 2, 0],
                                parent=local,
                                **{
                                    **mesh("coastal_pine"),
                                    "scale": [height / 9.52] * 3,
                                },
                            )
                        )
                    res.append(
                        {
                            "id": "region:" + key + ":" + str(local),
                            "local": str(local),
                            "kind": kind,
                            "height": height,
                            "amount": 6,
                            "position": [x, y, z],
                        }
                    )
                rows.append(
                    obj(
                        base + 900,
                        "Roadside binding",
                        scripts=1,
                        **{
                            "script.0.id": 1,
                            "script.0.asset": ident("script/region-content.js"),
                            "script.0.enabled": True,
                            "script.0.properties": json.dumps({"key": key}),
                        },
                    )
                )
                ss = copy.deepcopy(settings)
                ss.update(
                    name="Roadside wilderness / " + str(patch),
                    **{
                        "world-origin": [ORIGIN[0] + cx, cy, ORIGIN[1] + cz],
                        "next-id": base + 1000,
                    },
                )
                path = ROOT / f"Scenes/region_{key}.judas"
                write(
                    path,
                    {
                        "kind": "scene",
                        "schema": 1,
                        "data": {"settings": ss, "objects": rows},
                    },
                )
                regions[key] = {
                    "name": "Roadside wilderness " + str(patch),
                    "hidden": True,
                    "center": [cx, cz],
                    "position": [cx, cy, cz],
                    "theme": "wilderness",
                    "description": "Harvestable roadside trees and stone.",
                    "fixtures": [],
                    "doors": [],
                    "sockets": [],
                    "resources": res,
                    "scene": str(path.relative_to(ROOT)),
                    "objectCount": len(rows),
                    "nav": None,
                }
                world.append(
                    f'region "{key}" "Scenes/region_{key}.judas" {ORIGIN[0]+cx} {cy} {ORIGIN[1]+cz} 0 0 0 1 110 150 110 0 "snapshot" 524288 ""'
                )
    people["quests"]["patrol"]["objective"]["counter"] = "campKills"
    for key, items, reward in [
        ("forestry", {"wood": 12}, {"tokens": 25, "fuel_can": 2}),
        ("clinic", {"bandage": 3}, {"tokens": 30, "ammo_pistol": 20}),
        ("harbour", {"scrap": 12}, {"tokens": 40, "fuel_can": 3}),
        ("radio", {"radio_parts": 1}, {"tokens": 80, "repair_kit": 3}),
        ("islet", {"iron_ore": 6}, {"tokens": 50, "rations": 4}),
    ]:
        people["quests"][key] = {
            "id": key,
            "name": {
                "forestry": "Timber beyond the homestead",
                "clinic": "Dressings for Moonbridge",
                "harbour": "Repair the ferry stores",
                "radio": "A voice across the water",
                "islet": "The lantern mineral",
            }[key],
            "description": "Bring "
            + ", ".join(str(n) + " " + k for k, n in items.items())
            + " to this survivor.",
            "objective": {"items": items},
            "acceptItems": {},
            "rewards": reward,
            "accepted": "I need "
            + ", ".join(str(n) + " " + k for k, n in items.items())
            + ". Search the island and return.",
            "progress": "Bring the requested supplies; check your pack before returning.",
            "completed": "Thank you. Your supplies make a real difference. Take these provisions for the journey.",
            "after": "The work is done. Travel safely.",
        }
    # S5 state remains separate; earlier saves are preserved, never forced through.
    project = json.loads((ROOT / "lunar_island.judasproj").read_text())
    p = project["data"]
    if people["regionHumanCorpse"] not in p["runtimeAssets"]:
        p["runtimeAssets"].append(people["regionHumanCorpse"])
    p["saveIdentity"] = hashlib.sha256(
        b"LunarIsland-Stage5-ConnectedRegions-v1"
    ).hexdigest()
    p["classification"]["collision"]["entries"] = (
        [r for r in p["classification"]["collision"]["entries"] if r["id"] < 2]
        + [{"id": 2, "name": "Roads / mutable props"}]
        + [
            {"id": r["layer"], "name": "District / " + r["name"]}
            for r in regions.values()
            if "layer" in r
        ]
    )
    p["classification"]["collision"]["nextId"] = (
        max(r["layer"] for r in regions.values() if "layer" in r) + 1
    )
    write(ROOT / "lunar_island.judasproj", project)
    root["data"]["settings"]["next-id"] = 10000
    write(ROOT / "Scenes/island.judas", root)
    worldPath = ROOT / "Assets/worlds/transport.judasworld"
    worldPath.write_text("\n".join(world) + "\n")
    vehicleData = json.loads((ROOT / "Authoring/vehicle_manifest.json").read_text())
    vehicleData.update(regionVehicles)
    (ROOT / "Assets/scripts/vehicle-data.js").write_text(
        "export const VEHICLES = " + json.dumps(vehicleData, indent=2) + ";\n"
    )
    for scriptName in ["exploration.js", "region-session.js", "regional-housing.js"]:
        meta(ROOT / "Assets/scripts" / scriptName, "script")
    write(ROOT / "Authoring/region_manifest.json", {"regions": regions, "roads": roads})
    (ROOT / "Assets/scripts/region-data.js").write_text(
        "export const REGIONS = " + json.dumps(regions, indent=2) + ";\n"
    )
    meta(ROOT / "Assets/scripts/region-data.js", "script")
    meta(
        ROOT / "Assets/scripts/region-content.js", "script", "script/region-content.js"
    )
    write(ROOT / "Authoring/encounter_manifest.json", people)
    (ROOT / "Assets/scripts/encounter-data.js").write_text(
        "export const WEAPONS = "
        + json.dumps(people["weapons"], indent=2)
        + ";\nexport const PEOPLE = "
        + json.dumps({k: v for k, v in people.items() if k != "weapons"}, indent=2)
        + ";\n"
    )
    items = json.loads((ROOT / "Authoring/items.json").read_text())
    items["radio_parts"] = {
        "name": "Recovered radio components",
        "stack": 4,
        "weight": 1,
        "category": "materials",
    }
    ui = json.loads((ROOT / "Assets/ui/game.judasui").read_text())
    for element in ui["data"]["elements"]:
        if element["id"] == "chapter":
            element["text"] = "LUNAR ISLAND / RECONNECT THE COAST"
    write(ROOT / "Assets/ui/game.judasui", ui)
    write(ROOT / "Authoring/items.json", items)
    from author_project import data_only

    data_only()


if __name__ == "__main__":
    ap = argparse.ArgumentParser()
    ap.add_argument(
        "--prompt3d",
        type=Path,
        default=Path("/home/conner/Documents/GitHub/ClaudesNewML-AI-Thing"),
    )
    ap.add_argument("--generate-art", action="store_true")
    a = ap.parse_args()
    if a.generate_art:
        source_art(a.prompt3d)
    main(a.prompt3d)
