#!/usr/bin/env python3
"""Ordinary Stage 3 authoring. Reserved IDs 6000..6999; source art is read-only."""
import argparse, copy, json, math, struct, sys, shutil, hashlib, wave, random
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
    data_only,
)


def read_glb(path):
    raw = path.read_bytes()
    n = struct.unpack_from("<I", raw, 12)[0]
    return json.loads(raw[20 : 20 + n]), bytearray(raw[28 + n :])


def write_glb(path, j, b):
    j["buffers"][0]["byteLength"] = len(b)
    data = json.dumps(j, separators=(",", ":")).encode()
    data += b" " * ((-len(data)) % 4)
    b += b"\0" * ((-len(b)) % 4)
    path.write_bytes(
        struct.pack("<III", 0x46546C67, 2, 28 + len(data) + len(b))
        + struct.pack("<II", len(data), 0x4E4F534A)
        + data
        + struct.pack("<II", len(b), 0x004E4942)
        + b
    )


def animate(path, animal=False):
    j, b = read_glb(path)

    def accessor(values, kind):
        nonlocal b
        components = {"SCALAR": 1, "VEC3": 3, "VEC4": 4}[kind]
        flat = [
            v
            for row in values
            for v in (row if isinstance(row, (list, tuple)) else [row])
        ]
        b += b"\0" * ((-len(b)) % 4)
        start = len(b)
        b += struct.pack("<" + "f" * len(flat), *flat)
        vi = len(j.setdefault("bufferViews", []))
        j["bufferViews"].append(
            {"buffer": 0, "byteOffset": start, "byteLength": len(b) - start}
        )
        ai = len(j.setdefault("accessors", []))
        d = {
            "bufferView": vi,
            "componentType": 5126,
            "count": len(values),
            "type": kind,
        }
        if kind == "SCALAR":
            d.update(min=[min(flat)], max=[max(flat)])
        j["accessors"].append(d)
        return ai

    def qmul(a, c):
        x, y, z, w = a
        X, Y, Z, W = c
        return [
            w * X + x * W + y * Z - z * Y,
            w * Y - x * Z + y * W + z * X,
            w * Z + x * Y - y * X + z * W,
            w * W - x * X - y * Y - z * Z,
        ]

    def sample(ai):
        a = j["accessors"][ai]
        v = j["bufferViews"][a["bufferView"]]
        n = {"VEC3": 3, "VEC4": 4}[a["type"]]
        return list(
            struct.unpack_from(
                "<" + "f" * n, b, v.get("byteOffset", 0) + a.get("byteOffset", 0)
            )
        )

    idle = {}
    for a in j.get("animations", []):
        if a.get("name") == "Idle":
            for c in a["channels"]:
                idle[(c["target"]["node"], c["target"]["path"])] = sample(
                    a["samplers"][c["sampler"]]["output"]
                )
    times = [0, 0.18, 0.34, 0.5, 0.75]
    t = accessor(times, "SCALAR")
    anim = {"name": "Attack", "samplers": [], "channels": []}
    for i, node in enumerate(j["nodes"]):
        if not animal and i >= 17:
            continue
        if animal:
            continue
        base = idle.get((i, "rotation"), node.get("rotation", [0, 0, 0, 1]))
        angles = [0, 0, 0, 0, 0]
        if node.get("name") in ["upper_arm.R", "forearm.R"]:
            angles = [0, -0.65, -1.2, -0.3, 0]
        if node.get("name") == "chest":
            angles = [0, -0.08, -0.18, 0.08, 0]
        values = [qmul(base, [math.sin(a / 2), 0, 0, math.cos(a / 2)]) for a in angles]
        sampler = len(anim["samplers"])
        anim["samplers"].append(
            {"input": t, "output": accessor(values, "VEC4"), "interpolation": "LINEAR"}
        )
        anim["channels"].append(
            {"sampler": sampler, "target": {"node": i, "path": "rotation"}}
        )
        translation = idle.get((i, "translation"))
        if translation is not None:
            sampler = len(anim["samplers"])
            anim["samplers"].append(
                {
                    "input": t,
                    "output": accessor([translation] * len(times), "VEC3"),
                    "interpolation": "LINEAR",
                }
            )
            anim["channels"].append(
                {"sampler": sampler, "target": {"node": i, "path": "translation"}}
            )
    if not animal:
        j.setdefault("animations", []).append(anim)
    write_glb(path, j, b)


def art(tool):
    sys.dont_write_bytecode = True
    sys.path.insert(0, str(tool))
    from prompt3d.pipeline import generate, build_styled
    from prompt3d.export import to_glb
    from prompt3d import geometry as G, scans
    from material_finish import finish_static

    scans.GEN_DIR = str(ROOT / "ArtSource/no-texture-overrides")
    catalog = json.loads((ROOT / "ArtSource/asset_catalog.json").read_text())
    for name, color in [
        ("npc_survivor", [0.34, 0.43, 0.32, 1]),
        ("npc_bandit", [0.32, 0.28, 0.25, 1]),
    ]:
        p = ROOT / f"Assets/models/{name}.glb"
        j, b = read_glb(ROOT / "Assets/models/character_adventurer.glb")
        j["materials"][0]["pbrMetallicRoughness"]["baseColorFactor"] = color
        j["materials"][3]["pbrMetallicRoughness"]["baseColorFactor"] = [
            0.4,
            0.35,
            0.23,
            1,
        ]
        write_glb(p, j, b)
        animate(p)
        meta(p, "mesh", "model/" + name)
        catalog[name] = {
            "source": "Original Hearthworld adventurer rig; project-authored palette and Attack clip",
            "path": str(p.relative_to(ROOT)),
            "sha256": hashlib.sha256(p.read_bytes()).hexdigest(),
        }
    receipt = generate(
        "a realistic gray wolf",
        91303,
        str(ROOT / "ArtSource/stage3-prompt3d"),
        obj=False,
        rewrite=False,
    )
    sc = build_styled(receipt["category"], receipt["params"], 91303, "realistic")
    p = ROOT / "Assets/models/island_wolf.glb"
    to_glb(sc, str(p), textures=True, contrast=1, flat=False)
    meta(p, "mesh", "model/island_wolf")
    catalog["island_wolf"] = {
        "source": "Prompt3D real trained prompt parser/procedural animal generator",
        "prompt": "a realistic gray wolf",
        "seed": 91303,
        "params": receipt["params"],
        "path": str(p.relative_to(ROOT)),
        "sha256": hashlib.sha256(p.read_bytes()).hexdigest(),
    }
    for name, original in [
        ("pistol", "weapon_stylized_pistol"),
        ("rifle", "weapon_compact_rifle"),
    ]:
        source = REPO / f"LowPolyAssets/low_poly_starter/Exports/{original}.glb"
        p = ROOT / f"Assets/models/{name}.glb"
        shutil.copy2(source, p)
        finish_static(p, G, to_glb)
        meta(p, "mesh", "model/" + name)
        catalog[name] = {
            "source": str(source.relative_to(REPO)),
            "sourceSha256": hashlib.sha256(source.read_bytes()).hexdigest(),
            "path": str(p.relative_to(ROOT)),
            "sha256": hashlib.sha256(p.read_bytes()).hexdigest(),
        }
    for name in ["shotgun", "knife", "bat"]:
        sc = G.Scene(name)
        sc.material("iron", "#454b50", roughness=0.35)
        sc.material("wood", "#7a5234", roughness=0.7)

        def box(mat, p, h):
            sc.add(name, mat, G.box(*(v * 2 for v in h)), None, p)

        if name == "shotgun":
            box("wood", (0, 0, 0.25), (0.055, 0.09, 0.22))
            box("iron", (0, 0.05, -0.12), (0.055, 0.065, 0.22))
            box("iron", (-0.032, 0.065, -0.55), (0.024, 0.025, 0.28))
            box("iron", (0.032, 0.065, -0.55), (0.024, 0.025, 0.28))
            box("wood", (0, -0.015, -0.4), (0.06, 0.04, 0.15))
            box("iron", (0, -0.09, -0.1), (0.022, 0.07, 0.045))
        elif name == "knife":
            box("wood", (0, 0, 0.06), (0.025, 0.025, 0.09))
            box("iron", (0, 0, -0.13), (0.028, 0.008, 0.11))
            box("iron", (0, 0, -0.01), (0.055, 0.018, 0.012))
        else:
            box("wood", (0, 0, 0), (0.045, 0.045, 0.36))
            box("wood", (0, 0, 0.43), (0.022, 0.022, 0.1))
        p = ROOT / f"Assets/models/{name}.glb"
        to_glb(sc, str(p), textures=True, contrast=1, flat=False)
        meta(p, "mesh", "model/" + name)
        catalog[name] = {
            "source": "Original project mesh, author_stage3.py",
            "path": str(p.relative_to(ROOT)),
            "sha256": hashlib.sha256(p.read_bytes()).hexdigest(),
        }
    write(ROOT / "ArtSource/asset_catalog.json", catalog)


def validate_definitions(defs):
    items = json.loads((ROOT / "Authoring/items.json").read_text())

    # Fail authoring before replacing any scene objects or generated assets.
    def contents(values):
        for item, amount in values.items():
            assert item in items and type(amount) is int and amount > 0, (
                "invalid item quantity",
                item,
                amount,
            )

    ids = set()
    for key, actor in defs["actors"].items():
        ident = actor["entity"]
        assert (
            type(ident) is int
            and 6000 <= ident < 6400
            and ident % 10 == 0
            and ident not in ids
        ), ("NPC entity range/identity", key)
        ids.add(ident)
        assert actor["route"] and len(actor["position"]) == 2
        for pair in [actor["position"], *actor["route"]]:
            assert len(pair) == 2 and all(math.isfinite(x) for x in pair), (
                "invalid route",
                key,
            )
        assert (
            actor["health"] > 0
            and actor["speed"] > 0
            and actor["sight"] >= 0
            and actor["damage"] >= 0
        )
        contents(actor.get("stock", {}))
        contents(actor.get("prices", {}))
        assert not actor.get("quest") or actor["quest"] in defs["quests"]
    for key, weapon in defs["weapons"].items():
        assert key == "hands" or key in items
        assert all(
            math.isfinite(weapon[k]) and weapon[k] > 0
            for k in ["damage", "range", "interval"]
        )
        if weapon.get("ammo"):
            assert (
                weapon["ammo"] in items
                and type(weapon["capacity"]) is int
                and weapon["capacity"] > 0
                and weapon["reload"] > 0
            )
    for key, quest in defs["quests"].items():
        assert key == quest["id"]
        contents(quest["acceptItems"])
        contents(quest["rewards"])
        objective = quest["objective"]
        assert bool(objective.get("counter")) != bool(objective.get("items"))
        if objective.get("counter"):
            assert (
                objective["counter"] in ["kills", "wolves"]
                and type(objective["count"]) is int
                and objective["count"] > 0
            )
        else:
            contents(objective["items"])


def author():
    scene = json.loads((ROOT / "Scenes/island.judas").read_text())
    objects = scene["data"]["objects"]
    scene["data"]["objects"] = [o for o in objects if not 6000 <= o["id"] < 7000]
    objects = scene["data"]["objects"]
    terrain = (
        json.loads((ROOT / "Authoring/homestead.judasterrain").read_text())
        if (ROOT / "Authoring/homestead.judasterrain").exists()
        else None
    )

    # Use the same exact authored ground evaluator as Stage 2.
    def ground(x, z):
        return interpolate(terrain, x, z)

    defs = json.loads((ROOT / "Authoring/encounters.json").read_text())
    validate_definitions(defs)
    weapons = defs["weapons"]
    people = defs["actors"]
    script = meta(ROOT / "Assets/scripts/person.js", "script", "script/person.js")
    for key, d in people.items():
        x, z = d["position"]
        d["position"] = [x, ground(x, z) + 0.04, z]
        d["route"] = [[x, ground(x, z) + 0.04, z] for x, z in d["route"]]
        i = d["entity"]
        fields = {
            k: v
            for k, v in next(o for o in objects if o["id"] == 100)["fields"].items()
            if k.startswith("motor.")
        }
        fields.update(
            {
                "tags": 128 + (256 if d["friendly"] else 512),
                "scripts": 1,
                "script.0.id": 1,
                "script.0.asset": script,
                "script.0.enabled": True,
                "script.0.properties": json.dumps({"key": key}),
            }
        )
        if d.get("animal"):
            fields.update(
                {
                    "motor.radius": 0.28,
                    "motor.halfHeight": 0.2,
                    "motor.offset": [0, 0.48, 0],
                }
            )
        for k, v in {
            "enabled": "1",
            "avoidance": "1",
            "profile": "0",
            "areas": str((1 << 64) - 1),
            "speed": str(d["speed"]),
            "arrival": ".7",
            "cornerDistance": ".4",
            "repathSeconds": ".6",
            "costs": "0 1",
        }.items():
            fields["nav.agent." + k] = v
        objects.append(obj(i, d["name"], d["position"], **fields))
        model = (
            "island_wolf"
            if d.get("animal")
            else "npc_survivor" if d["friendly"] else "npc_bandit"
        )
        visual = {"parent": i, **mesh(model)}
        if not d.get("animal"):
            visual.update(
                {
                    "animation.enabled": True,
                    "animation.play-on-start": False,
                    "animation.loop": True,
                    "animation.clip": "Idle",
                    "animation.speed": 1,
                    "animation.time": 0,
                }
            )
        objects.append(obj(i + 1, d["name"] + " / visual", d["position"], **visual))
        if not d["friendly"] and not d.get("animal"):
            objects.append(
                obj(
                    i + 2,
                    d["name"] + " / held weapon",
                    parent=i,
                    **mesh("rifle" if d.get("ranged") else "bat"),
                )
            )
    scene["data"]["settings"]["next-id"] = 7000
    objects.append(
        obj(
            6900,
            "Homestead / navigation",
            [0, 25, 15],
            **{
                "nav.surface.enabled": "1",
                "nav.surface.asset": "",
                "nav.surface.halfExtents": "110 35 110",
                "nav.surface.cellSize": ".35",
                "nav.surface.cellHeight": ".15",
                "nav.surface.tileSize": "64",
                "nav.surface.profile": "0",
                "nav.surface.sources": str((1 << 64) - 1),
                "nav.surface.includeDynamic": "0",
                "nav.surface.minRegion": "2",
                "nav.surface.simplification": "1.3",
            },
        )
    )
    # Corpse prefabs have normal native collision/mass and no game script.
    prefabs = {}
    for name, model, half in [
        ("human_corpse", "npc_bandit", [0.28, 0.8, 0.22]),
        ("wolf_corpse", "island_wolf", [0.6, 0.3, 0.25]),
    ]:
        y = -half[1]
        data = {
            "kind": "prefab",
            "schema": 1,
            "data": {
                "settings": copy.deepcopy(
                    json.loads(
                        (ROOT / "Assets/prefabs/build_chest.judasprefab").read_text()
                    )["data"]["settings"]
                ),
                "objects": [
                    obj(
                        1,
                        "Recoverable remains",
                        **body(
                            half, 70 if name == "human_corpse" else 30, motion="dynamic"
                        ),
                        tags=8,
                    ),
                    obj(2, "Remains visual", (0, y, 0), parent=1, **mesh(model)),
                ],
            },
        }
        p = ROOT / f"Assets/prefabs/{name}.judasprefab"
        write(p, data)
        prefabs[name] = meta(p, "prefab", "prefab/" + name)
    # Bounded native particle emitter; reused for visible physical impact feedback.
    objects.append(
        obj(
            6910,
            "Weapon impact sparks",
            **{
                "particle-emitter": [],
                "particle.enabled": True,
                "particle.loop": False,
                "particle.local-space": False,
                "particle.gravity": False,
                "particle.rate": 0,
                "particle.lifetime": 0.2,
                "particle.size": 0.045,
                "particle.end-size": 0.01,
                "particle.burst": 0,
                "particle.capacity": 128,
                "particle.spread": [0.02, 0.02, 0.02],
                "particle.velocity": [0, 0.7, 0],
                "particle.variation": [1, 1, 1],
                "particle.acceleration": [0, -4, 0],
                "particle.texture": "",
                "particle.color": [1, 0.7, 0.25],
                "particle.alpha": 1,
                "particle.end-color": [1, 0.2, 0],
                "particle.end-alpha": 0,
                "particle.seed": 303,
            },
        )
    )
    soundIds = {}
    for index, name in enumerate(["gunshot", "reload", "hurt"]):
        path = ROOT / f"Assets/audio/{name}.wav"
        rng = random.Random(91300 + index)
        rate = 22050
        count = int(rate * (0.22 if name != "reload" else 0.38))
        samples = []
        for k in range(count):
            t = k / rate
            if name == "gunshot":
                value = (
                    rng.uniform(-1, 1) * 0.7 + math.sin(2 * math.pi * 80 * t) * 0.3
                ) * math.exp(-35 * t)
            elif name == "reload":
                value = (
                    rng.uniform(-1, 1)
                    * sum(math.exp(-abs(t - c) * 230) for c in [0.02, 0.17, 0.31])
                    * 0.4
                )
            else:
                value = (
                    (math.sin(2 * math.pi * 115 * t) * 0.7 + rng.uniform(-1, 1) * 0.3)
                    * math.exp(-23 * t)
                    * 0.5
                )
            samples.append(int(max(-1, min(1, value)) * 15000))
        with wave.open(str(path), "wb") as f:
            f.setnchannels(1)
            f.setsampwidth(2)
            f.setframerate(rate)
            f.writeframes(struct.pack("<" + "h" * len(samples), *samples))
        aid = meta(path, "audio")
        soundIds[name] = str(6940 + index)
        template = copy.deepcopy(next(o for o in objects if o["id"] == 961))
        template["id"] = 6940 + index
        template["name"] = "Original effect / " + name
        template["fields"]["audio.asset"] = aid
        template["fields"]["audio.volume"] = 0.5
        objects.append(template)
    for i, (x, z) in enumerate([(75, 66), (81, 66)]):
        objects.append(
            obj(
                6400 + i,
                "Scavenger camp timber cover",
                (x, ground(x, z) + 0.55, z),
                **{
                    **mesh("build_wall"),
                    **body([1.5, 0.55, 0.12]),
                    "scale": [1, 0.4, 1],
                    "tags": 64,
                },
            )
        )
    vm = {}
    for n, name in enumerate(["pistol", "shotgun", "rifle", "knife", "bat"]):
        i = 6920 + n
        objects.append(
            obj(
                i,
                name + " / held presentation",
                **mesh(name),
                **{"render-visible": False},
            )
        )
        vm[name] = str(i)
    project = json.loads((ROOT / "lunar_island.judasproj").read_text())
    p = project["data"]
    p["saveIdentity"] = hashlib.sha256(b"LunarIsland-Stage3-Encounters-v1").hexdigest()
    for k, n in [(7, "person"), (8, "friendly"), (9, "hostile")]:
        if not any(t["id"] == k for t in p["classification"]["tags"]["entries"]):
            p["classification"]["tags"]["entries"].append({"id": k, "name": n})
    p["classification"]["tags"]["nextId"] = 10
    if not any(e["id"] == 1 for e in p["classification"]["collision"]["entries"]):
        p["classification"]["collision"]["entries"].append(
            {"id": 1, "name": "Far island shell"}
        )
    p["classification"]["collision"]["nextId"] = 2
    for o in objects:
        if o["id"] in [1, 2]:
            o["fields"]["body.collision-layer"] = 1
        if o["id"] == 6900:
            o["fields"]["nav.surface.sources"] = "1"
    for a in prefabs.values():
        if a not in p["runtimeAssets"]:
            p["runtimeAssets"].append(a)
    for action, control in [("reload", "key:R"), ("aim", "mouse:Right")]:
        if not any(e["name"] == action for e in p["input"]):
            p["input"].append(
                {
                    "name": action,
                    "axis": False,
                    "bindings": [{"control": control, "scale": 1, "deadzone": 0}],
                }
            )
    data_only()
    write(ROOT / "Scenes/island.judas", scene)
    write(ROOT / "lunar_island.judasproj", project)
    payload = {
        "actors": people,
        "quests": defs.get("quests", {}),
        "humanCorpse": prefabs["human_corpse"],
        "wolfCorpse": prefabs["wolf_corpse"],
        "impact": "6910",
        "viewmodels": vm,
        "sounds": soundIds,
    }
    path = ROOT / "Assets/scripts/encounter-data.js"
    path.write_text(
        "// Generated from Authoring/encounters.json.\nexport const WEAPONS = "
        + json.dumps(weapons, indent=2)
        + ";\nexport const PEOPLE = "
        + json.dumps(payload, indent=2)
        + ";\n"
    )
    meta(path, "script", "script/encounter-data.js")
    for name in ["people-session", "encounter-rules", "combat", "social-ui"]:
        meta(ROOT / f"Assets/scripts/{name}.js", "script", "script/" + name + ".js")
    write(ROOT / "Authoring/encounter_manifest.json", payload)
    ui = json.loads((ROOT / "Assets/ui/game.judasui").read_text())
    for e in ui["data"]["elements"]:
        if e["id"] == "menu_status":
            e.update(offset=[24, 505], size=[1030, 55], fontSize=17)
    write(ROOT / "Assets/ui/game.judasui", ui)
    print("Stage 3:", len(people), "ordinary NPCs,", len(weapons), "weapon profiles")


if __name__ == "__main__":
    ap = argparse.ArgumentParser()
    ap.add_argument("--prompt3d", type=Path)
    ap.add_argument("--art-only", action="store_true")
    a = ap.parse_args()
    if a.prompt3d:
        art(a.prompt3d)
    if not a.art_only:
        author()
