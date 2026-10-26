#!/usr/bin/env python3
"""Stage 4 authoring. Replaces only 7000..7999, preserving earlier game content."""

import argparse, copy, hashlib, json, math, sys, wave, struct
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

DEFS = json.loads((ROOT / "Authoring/vehicles.json").read_text())


def art(tool):
    sys.dont_write_bytecode = True
    sys.path.insert(0, str(tool))
    import numpy as np
    from prompt3d import geometry as G, scans
    from prompt3d.pipeline import generate, build_styled
    from prompt3d.export import to_glb

    scans.GEN_DIR = str(ROOT / "ArtSource/no-texture-overrides")
    catalog = json.loads((ROOT / "ArtSource/asset_catalog.json").read_text())
    specs = {
        "car": ("a realistic green hatchback", "car", "hatchback"),
        "truck": ("a realistic red pickup truck", "car", "truck"),
        "boat": ("a realistic motor boat", "boat", "motor"),
        "plane": ("a realistic light propeller plane", "plane", "prop"),
    }
    for index, (key, (prompt, category, kind)) in enumerate(specs.items()):
        receipt = generate(
            prompt,
            91400 + index,
            str(ROOT / "ArtSource/stage4-prompt3d"),
            obj=False,
            rewrite=False,
        )
        params = copy.deepcopy(receipt["params"])
        params.update(type=kind, size=1, color=DEFS[key]["color"])
        params.setdefault("features", {})["gear"] = False
        scene = build_styled(category, params, 91400 + index, "realistic")
        if key in ["car", "truck"]:
            from prompt3d.generators.car import SPECS

            sp = SPECS[kind]
            # Remove only wheel primitives. Actual suspension/spin is M81-owned.
            for mats in scene.groups.values():
                for mat, parts in list(mats.items()):
                    keep = []
                    for part in parts:
                        low, high = part.V.min(axis=0), part.V.max(axis=0)
                        c = (low + high) / 2
                        wheel = (
                            mat in ["car_tyre", "car_chrome", "car_trim"]
                            and len(part.V) > 20
                            and high[2] - low[2] < 0.36
                            and abs(c[2]) > sp["W"] / 2 - 0.4
                            and c[1] < sp["r"] + 0.1
                            and min(
                                abs(c[0] + sp["L"] / 2 - sp["xr"]),
                                abs(c[0] + sp["L"] / 2 - sp["xf"]),
                            )
                            < 0.1
                        )
                        if not wheel:
                            keep.append(part)
                    if keep:
                        mats[mat] = keep
                    else:
                        del mats[mat]
            scene.transform_all(None, (0, -DEFS[key]["height"], 0))
            scene.transform_all(G.rot_y(math.pi / 2))
        else:
            lo, hi = scene.bounds()
            center = (lo + hi) / 2
            scene.transform_all(None, (-center[0], -center[1], -center[2]))
            scene.transform_all(G.rot_y(math.pi / 2))
            if key == "boat":
                lo, hi = scene.bounds()
                scene.transform_all(
                    G.scale(
                        1.6 / (hi[0] - lo[0]),
                        0.7 / (hi[1] - lo[1]),
                        4.4 / (hi[2] - lo[2]),
                    )
                )
            else:
                lo, hi = scene.bounds()
                scene.transform_all(
                    G.scale(
                        7 / (hi[0] - lo[0]),
                        1.8 / (hi[1] - lo[1]),
                        5.8 / (hi[2] - lo[2]),
                    )
                )
        path = ROOT / f"Assets/models/vehicle_{key}.glb"
        to_glb(scene, str(path), textures=True, flat=False)
        meta(path, "mesh", "model/vehicle_" + key)
        catalog["vehicle_" + key] = {
            "source": "Installed Prompt3D parser and procedural generator; explicit verified template selection",
            "prompt": prompt,
            "rawCategory": receipt["category"],
            "template": category,
            "params": params,
            "seed": 91400 + index,
            "path": str(path.relative_to(ROOT)),
            "sha256": hashlib.sha256(path.read_bytes()).hexdigest(),
        }

    def original(name):
        s = G.Scene(name)
        s.material("paint", "#c1a450", metallic=0.3, roughness=0.4)
        s.material("dark", "#242c2d", roughness=0.7)
        s.material("steel", "#8c9595", metallic=0.8, roughness=0.3)
        s.material("glass", "#243d43", metallic=0.25, roughness=0.18)
        return s

    def box(s, mat, p, h):
        s.add(s.name, mat, G.box(*(2 * x for x in h)), None, p)

    def tube(s, mat, a, b, r):
        s.add(s.name, mat, G.tube(a, b, r, None, 12))

    s = original("vehicle_motorcycle")
    box(s, "dark", (0, 0, 0.1), (0.18, 0.12, 0.45))
    s.add(s.name, "paint", G.sphere(1), G.scale(0.23, 0.22, 0.36), (0, 0.12, -0.15))
    box(s, "dark", (0, 0.22, 0.35), (0.19, 0.06, 0.3))
    tube(s, "steel", (-0.16, -0.45, -0.85), (-0.16, 0.35, -0.6), 0.035)
    tube(s, "steel", (0.16, -0.45, -0.85), (0.16, 0.35, -0.6), 0.035)
    tube(s, "steel", (-0.38, 0.37, -0.57), (0.38, 0.37, -0.57), 0.035)
    tube(s, "steel", (0, -0.25, 0.3), (0, -0.45, 0.85), 0.06)
    custom = {"motorcycle": s}
    s = original("vehicle_helicopter")
    s.add(s.name, "paint", G.sphere(1), G.scale(0.8, 0.65, 1.5), (0, 0, -0.2))
    s.add(s.name, "glass", G.sphere(1), G.scale(0.68, 0.48, 0.65), (0, 0.06, -1.15))
    tube(s, "paint", (0, 0.1, 0.8), (0, 0.5, 3.7), 0.16)
    box(s, "paint", (0, 0.7, 3.55), (0.05, 0.6, 0.4))
    for x in [-0.85, 0.85]:
        tube(s, "steel", (x, -1, -1.1), (x, -1, 1.2), 0.045)
        tube(s, "steel", (x, -1, -0.7), (x * 0.6, -0.3, -0.6), 0.04)
        tube(s, "steel", (x, -1, 0.8), (x * 0.6, -0.3, 0.6), 0.04)
    tube(s, "steel", (0, 0.55, 0), (0, 1.02, 0), 0.06)
    custom["helicopter"] = s
    s = original("vehicle_rotor")
    box(s, "dark", (0, 0, 0), (3.7, 0.025, 0.10))
    box(s, "dark", (0, 0, 0), (0.10, 0.025, 3.7))
    custom["rotor"] = s
    s = original("vehicle_propeller")
    box(s, "dark", (0, 0, 0), (0.06, 0.95, 0.025))
    box(s, "dark", (0, 0, 0), (0.95, 0.06, 0.025))
    custom["propeller"] = s
    s = original("vehicle_wheel")
    s.add(s.name, "dark", G.cylinder(1, 0.55, 24), G.rot_z(math.pi / 2))
    s.add(s.name, "steel", G.cylinder(0.6, 0.6, 16), G.rot_z(math.pi / 2))
    custom["wheel"] = s
    for key, s in custom.items():
        path = ROOT / f"Assets/models/vehicle_{key}.glb"
        to_glb(s, str(path), textures=True, flat=False)
        meta(path, "mesh", "model/vehicle_" + key)
        catalog["vehicle_" + key] = {
            "source": "Original authored geometry, author_stage4.py; no generator claims for unsupported motorcycle/helicopter categories",
            "path": str(path.relative_to(ROOT)),
            "sha256": hashlib.sha256(path.read_bytes()).hexdigest(),
        }
    write(ROOT / "ArtSource/asset_catalog.json", catalog)


def shaft(
    name, kind="internal", target=0, bearing=0, axis=(1, 0, 0), inertia=0.2, speed=0
):
    return dict(
        name=name,
        kind=kind,
        target=str(target),
        bearing=str(bearing),
        axis=axis,
        inertia=inertia,
        initialAngularVelocity=speed,
        viscousDrag=0.02,
    )


def source(name, index, kind="electric", torque=200, power=20000, curve=(), starter=0):
    return dict(
        name=name,
        shaft=index,
        kind=kind,
        maxTorque=torque,
        maxPower=power,
        torqueCurve=curve,
        starterTorque=starter,
    )


def brake(name, index, torque):
    return dict(name=name, shaft=index, maxTorque=torque)


def graph(d):
    root = d["entity"]
    kind = d["kind"]
    if kind in ["car", "truck"]:
        return dict(
            version=1,
            enabled=True,
            shafts=[
                shaft("crank", bearing=root, inertia=0.28, speed=100),
                shaft("gearbox", bearing=root),
                shaft("carrier", bearing=root),
                shaft("left", "wheel", root + 4),
                shaft("right", "wheel", root + 5),
            ],
            sources=[
                source(
                    "engine",
                    0,
                    "combustion",
                    200,
                    100000,
                    [
                        [0, 0],
                        [600, 0],
                        [900, 95],
                        [1800, 145],
                        [3200, 160],
                        [4800, 125],
                        [6200, 40],
                        [6800, 0],
                    ],
                    42,
                )
            ],
            links=[
                dict(
                    name="gearbox",
                    kind="gear",
                    input=0,
                    output=1,
                    outputB=0,
                    ratio=1 / 4.5,
                    maxTorque=500,
                ),
                dict(
                    name="clutch",
                    kind="clutch",
                    input=1,
                    output=2,
                    outputB=0,
                    ratio=1,
                    maxTorque=1000,
                ),
                dict(
                    name="differential",
                    kind="differential",
                    input=2,
                    output=3,
                    outputB=4,
                    ratio=1 / 3.5,
                    maxTorque=700,
                ),
            ],
            brakes=[brake("left", 3, 1100), brake("right", 4, 1100)],
        )
    if kind == "motorcycle":
        return dict(
            version=1,
            enabled=True,
            shafts=[shaft("rear", "wheel", root + 3)],
            sources=[source("engine", 0, torque=140, power=9000)],
            links=[],
            brakes=[brake("rear", 0, 400)],
        )
    heli = kind == "helicopter"
    return dict(
        version=1,
        enabled=True,
        shafts=[
            shaft(
                "rotor",
                bearing=root,
                axis=(0, 1, 0) if heli else (0, 0, -1),
                inertia=16 if heli else 0.8,
            )
        ],
        sources=[
            source(
                "engine",
                0,
                torque=1800 if heli else 700 if kind == "plane" else 180,
                power=300000 if heli else 180000 if kind == "plane" else 40000,
            )
        ],
        links=[],
        brakes=[brake("load", 0, 4000)],
    )


def author():
    scene = json.loads((ROOT / "Scenes/island.judas").read_text())
    scene["data"]["objects"] = [
        o for o in scene["data"]["objects"] if not 7000 <= o["id"] < 8000
    ]
    rows = scene["data"]["objects"]
    terrain = json.loads((ROOT / "Authoring/homestead.judasterrain").read_text())
    ground = lambda x, z: interpolate(terrain, x, z)
    runway = max(ground(x, z) for x in [-7, 0, 7] for z in range(60, 221, 5)) + 0.15
    meta(ROOT / "Assets/scripts/vehicle.js", "script", "script/vehicle.js")
    audioPath = ROOT / "Assets/audio/vehicle_engine.wav"
    rate = 22050
    samples = [
        int(
            5000
            * (
                math.sin(2 * math.pi * 70 * k / rate)
                + 0.3 * math.sin(2 * math.pi * 140 * k / rate)
                + 0.15 * math.sin(2 * math.pi * 280 * k / rate)
            )
        )
        for k in range(rate // 2)
    ]
    with wave.open(str(audioPath), "wb") as wav:
        wav.setnchannels(1)
        wav.setsampwidth(2)
        wav.setframerate(rate)
        wav.writeframes(struct.pack("<" + "h" * len(samples), *samples))
    sound = meta(audioPath, "audio")
    audioFields = {
        k: copy.deepcopy(v)
        for k, v in next(o for o in rows if o["id"] == 961)["fields"].items()
        if k == "audio-emitter" or k.startswith("audio.")
    }
    audioFields.update(
        {
            "audio.asset": sound,
            "audio.loop": True,
            "audio.play-on-start": False,
            "audio.volume": 0.2,
            "audio.maximum-distance": 120,
        }
    )
    payload = copy.deepcopy(DEFS)
    for key, d in payload.items():
        i = d["entity"]
        kind = d["kind"]
        x, z = d["position"]
        y = (
            d["height"]
            if kind == "boat"
            else runway + d["height"] if kind == "plane" else ground(x, z) + d["height"]
        )
        d["position"] = [x, y, z]
        d["wheels"] = []
        d["visual"] = str(i + 1)
        f = {
            **body(
                d["half"], d["mass"], motion="static" if kind == "depot" else "dynamic"
            ),
            "tags": 1024,
            "scripts": 1,
            "script.0.id": 1,
            "script.0.asset": ident("script/vehicle.js"),
            "script.0.enabled": True,
            "script.0.properties": json.dumps({"key": key}),
        }
        if kind != "depot":
            f["powertrain.graph"] = json.dumps(graph(d))
            f.update(copy.deepcopy(audioFields))
        if kind in ["car", "truck"]:
            f["body"] = {
                "motion": {"symbol": "dynamic"},
                "shape": {"symbol": "compound"},
            }
            f["body.compound-count"] = 2
            f["body.compound-box"] = [
                {"center": [0, -0.23, 0], "halfExtents": d["half"]},
                {
                    "center": [
                        0,
                        0.4 if kind == "truck" else 0.3,
                        -0.45 if kind == "truck" else 0.45,
                    ],
                    "halfExtents": [
                        d["half"][0] * 0.8,
                        0.4 if kind == "truck" else 0.3,
                        1 if kind == "truck" else 1.4,
                    ],
                },
            ]
        if kind == "plane":
            f["body"] = {
                "motion": {"symbol": "dynamic"},
                "shape": {"symbol": "compound"},
            }
            f["body.compound-count"] = 2
            f["body.compound-box"] = [
                {"center": [0, 0, 0], "halfExtents": d["half"]},
                {"center": [0, 0.08, 0], "halfExtents": [3.35, 0.08, 0.55]},
            ]
        if kind == "helicopter":
            f["body"] = {
                "motion": {"symbol": "dynamic"},
                "shape": {"symbol": "compound"},
            }
            f["body.compound-count"] = 4
            f["body.compound-box"] = [
                {"center": [0, 0, -0.2], "halfExtents": d["half"]},
                {"center": [0, 0.3, 2.2], "halfExtents": [0.14, 0.14, 1.5]},
            ] + [
                {"center": [x, -0.95, 0], "halfExtents": [0.06, 0.06, 1.3]}
                for x in [-0.85, 0.85]
            ]
        rows.append(obj(i, d["name"], d["position"], **f))
        rows.append(
            obj(
                i + 1,
                d["name"] + " / authored body",
                [0, 0, 0],
                parent=i,
                tags=4096,
                **mesh("wood_crate" if kind == "depot" else "vehicle_" + kind),
            )
        )
        mounts = []
        if kind in ["car", "truck"]:
            mounts = [
                [x, -0.25 if kind == "car" else -0.32, z]
                for z in [d["front"], d["rear"]]
                for x in [-d["track"], d["track"]]
            ]
        if kind == "motorcycle":
            mounts = [[0, -0.2, d["front"]], [0, -0.2, d["rear"]]]
        if kind == "plane":
            mounts = [
                [0, -0.2, d["front"]],
                [-d["track"], -0.2, d["rear"]],
                [d["track"], -0.2, d["rear"]],
            ]
        for n, m in enumerate(mounts):
            wid = i + 2 + n
            d["wheels"].append(str(wid))
            fields = {"parent": i}
            settings = dict(
                enabled="true",
                chassis=str(i),
                mount=" ".join(map(str, m)),
                orientation="1 0 0 0",
                radius=str(d["radius"]),
                minLength=".08",
                restLength=str(d["height"] + m[1] - d["radius"]),
                maxLength=str(d["height"] + m[1] - d["radius"] + 0.2),
                springStiffness=str(d["mass"] * 9.81 / len(mounts) / 0.12),
                damping=str(d["mass"] / len(mounts) * 8),
                spinInertia="1.2",
                friction="1.05",
                rollingResistance=".015",
                collisionLayer="0",
                collisionMask=str(2**64 - 1),
                requiredTags="0",
                excludedTags="0",
            )
            settings.update(enabled=True, chassis=i, mount=m, orientation=[1, 0, 0, 0])
            for k in [
                "radius",
                "minLength",
                "restLength",
                "maxLength",
                "springStiffness",
                "damping",
                "spinInertia",
                "friction",
                "rollingResistance",
            ]:
                settings[k] = float(settings[k])
            for k in [
                "collisionLayer",
                "collisionMask",
                "requiredTags",
                "excludedTags",
            ]:
                settings[k] = int(settings[k])
            fields.update({"wheel." + k: v for k, v in settings.items()})
            center = [m[0], m[1] - (d["height"] + m[1] - d["radius"]), m[2]]
            rows.append(obj(wid, d["name"] + " / wheel", center, **fields))
            rows.append(
                obj(
                    i + 30 + n,
                    d["name"] + " / tyre visual",
                    [0, 0, 0],
                    parent=wid,
                    **mesh("vehicle_wheel"),
                    scale=[d["radius"]] * 3,
                )
            )
        if kind in ["helicopter", "plane"]:
            rotor = i + 12
            d["rotor"] = str(rotor)
            rows.append(
                obj(
                    rotor,
                    d["name"] + " / rotating rotor",
                    [0, 1.02, 0] if kind == "helicopter" else [0, 0, -3],
                    parent=i,
                    tags=2048,
                    **mesh(
                        "vehicle_rotor" if kind == "helicopter" else "vehicle_propeller"
                    ),
                )
            )
    # A small service strip and spring launch are Stage 4 infrastructure, not town expansion.
    for i, name, p, h, color in [
        (
            7700,
            "Homestead grass airstrip",
            [0, runway - 0.12, 140],
            [7, 0.12, 80],
            [0.31, 0.34, 0.22],
        ),
        (7701, "Spring launch", [90, 10.2, -90], [2, 0.15, 5], [0.44, 0.32, 0.2]),
    ]:
        rows.append(
            obj(
                i,
                name,
                p,
                **{
                    **body(h),
                    **mesh("build_wall"),
                    "render": {"symbol": "box"},
                    "render.mesh-asset": "",
                    "render.half-extents": h,
                    "render.color": color,
                    "tags": 1,
                },
            )
        )
    approach = ground(0, 40)
    angle = -math.atan2(runway - approach, 20)
    half = [7, 0.1, 10]
    ramp = obj(
        7702,
        "Airstrip approach ramp",
        [0, (runway + approach) / 2 - 0.1, 50],
        **{
            **body(half),
            **mesh("build_wall"),
            "render": {"symbol": "box"},
            "render.mesh-asset": "",
            "render.half-extents": half,
            "render.color": [0.31, 0.34, 0.22],
            "rotation": [math.cos(angle / 2), math.sin(angle / 2), 0, 0],
            "tags": 1,
        },
    )
    rows.append(ramp)
    for n in range(9):
        rows.append(
            obj(
                7710 + n,
                "Runway center mark",
                [0, runway + 0.005, 70 + n * 17],
                **{
                    **mesh("build_wall"),
                    "render": {"symbol": "box"},
                    "render.mesh-asset": "",
                    "render.half-extents": [0.08, 0.01, 2],
                    "render.color": [0.83, 0.82, 0.65],
                },
            )
        )
    project = json.loads((ROOT / "lunar_island.judasproj").read_text())
    p = project["data"]
    p["saveIdentity"] = hashlib.sha256(b"LunarIsland-Stage4-Transport-v1").hexdigest()
    if not any(t["id"] == 10 for t in p["classification"]["tags"]["entries"]):
        p["classification"]["tags"]["entries"].append({"id": 10, "name": "vehicle"})
    p["classification"]["tags"]["nextId"] = 11
    scene["data"]["settings"]["next-id"] = 8000
    # Regional assembly is genuinely authored regional content, not an adopted
    # root masquerading as a suspendable recipe. Driven instances transfer to root.
    regional_ids = set()
    for key, d in payload.items():
        if d.get("region"):
            regional_ids.update(
                o["id"] for o in rows if d["entity"] <= o["id"] < d["entity"] + 50
            )
    region = copy.deepcopy(scene)
    region["data"]["objects"] = [o for o in rows if o["id"] in regional_ids]
    region["data"]["settings"]["name"] = "Birchside transport garage"
    region["data"]["settings"]["world-origin"] = [0, 0, 0]
    scene["data"]["objects"] = [o for o in rows if o["id"] not in regional_ids]
    write(ROOT / "Scenes/transport_garage.judas", region)
    worldpath = ROOT / "Assets/worlds/transport.judasworld"
    worldpath.parent.mkdir(parents=True, exist_ok=True)
    worldpath.write_text(
        'JudasWorld 1\nbudget 2 16 2 16777216 67108864 16777216 1073741824\nregion "transport_garage" "Scenes/transport_garage.judas" 2900 0 -1600 0 0 0 1 120 40 120 1 "snapshot" 524288 ""\n'
    )
    p["worldManifest"] = meta(worldpath, "world", "world/transport")
    for tag, name in [(11, "vehicle_rotor"), (12, "vehicle_visual")]:
        if not any(t["id"] == tag for t in p["classification"]["tags"]["entries"]):
            p["classification"]["tags"]["entries"].append({"id": tag, "name": name})
    p["classification"]["tags"]["nextId"] = 13
    # Each complete assembly is also an ordinary reusable prefab.
    for key, d in payload.items():
        assembly = copy.deepcopy(
            [o for o in rows if d["entity"] <= o["id"] < d["entity"] + 50]
        )
        mapping = {o["id"]: n + 1 for n, o in enumerate(assembly)}
        for o in assembly:
            old = o["id"]
            o["id"] = mapping[old]
            f = o["fields"]
            if "parent" not in f:
                f["position"] = [a - b for a, b in zip(f["position"], d["position"])]
            if "parent" in f:
                f["parent"] = mapping[f["parent"]]
            if "wheel.chassis" in f:
                f["wheel.chassis"] = mapping[f["wheel.chassis"]]
            if "powertrain.graph" in f:
                graphdata = json.loads(f["powertrain.graph"])
                for shaftdata in graphdata["shafts"]:
                    for field in ["target", "bearing"]:
                        value = int(shaftdata[field])
                        shaftdata[field] = str(mapping[value]) if value else "0"
                f["powertrain.graph"] = json.dumps(graphdata)
        prefabpath = ROOT / f"Assets/prefabs/vehicle_{key}.judasprefab"
        write(
            prefabpath,
            {
                "kind": "prefab",
                "schema": 1,
                "data": {
                    "settings": copy.deepcopy(region["data"]["settings"]),
                    "objects": assembly,
                },
            },
        )
        prefabid = meta(prefabpath, "prefab", "prefab/vehicle_" + key)
        if prefabid not in p["runtimeAssets"]:
            p["runtimeAssets"].append(prefabid)
    write(ROOT / "Scenes/island.judas", scene)
    write(ROOT / "lunar_island.judasproj", project)
    for name in [
        "vehicle-rules",
        "vehicle-session",
        "vehicle",
        "transport",
        "vehicle-ui",
    ]:
        meta(ROOT / f"Assets/scripts/{name}.js", "script", "script/" + name + ".js")
    path = ROOT / "Assets/scripts/vehicle-data.js"
    path.write_text(
        "// Generated from Authoring/vehicles.json\nexport const VEHICLES = "
        + json.dumps(payload, indent=2)
        + ";\n"
    )
    meta(path, "script", "script/vehicle-data.js")
    write(ROOT / "Authoring/vehicle_manifest.json", payload)
    data_only()
    print("Authored", len(payload), "vehicle/service entities; runway height", runway)


if __name__ == "__main__":
    p = argparse.ArgumentParser()
    p.add_argument("--prompt3d", type=Path)
    p.add_argument("--art-only", action="store_true")
    a = p.parse_args()
    if a.prompt3d:
        art(a.prompt3d)
    if not a.art_only:
        author()
