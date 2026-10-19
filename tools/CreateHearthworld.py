#!/usr/bin/env python3
"""Reproduce Hearthworld's ordinary consumer project and authored geometry.

This script writes project content, not engine behaviour. Run after
projects/hearthworld/tools/generate_art.py. Runtime rules and persistent game state live in the
project's JavaScript modules. Existing scene files are deliberately regenerated;
save editor changes under a different scene name before running it again.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import math
from pathlib import Path
import shutil
import struct
import subprocess


ROOT = Path(__file__).resolve().parents[1]
PROJECT = ROOT / "projects/hearthworld"
RADIUS = 120.0
CAP_RADIUS = 12.0
TERRAIN_POSITION = [0.0, 115.0, 0.0]


def identity(name):
    return hashlib.sha256(("hearthworld/" + name).encode()).hexdigest()[:32]


def write(path, value):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value, indent=2) + "\n")


def meta(path, asset_id, kind):
    path.with_name(path.name + ".judasmeta").write_text(
        f'JudasAssetMeta 1\nid "{asset_id}"\ntype {kind}\nsource "hearthworld-authoring"\n'
    )


def add(a, b):
    return [a[i] + b[i] for i in range(3)]


def mul(v, scalar):
    return [x * scalar for x in v]


def norm(v):
    length = math.sqrt(sum(x * x for x in v))
    return mul(v, 1.0 / length)


def rotate(q, v):
    w, x, y, z = q
    u = [x, y, z]
    cross = lambda a, b: [a[1]*b[2]-a[2]*b[1], a[2]*b[0]-a[0]*b[2], a[0]*b[1]-a[1]*b[0]]
    c = cross(u, v)
    cc = cross(u, c)
    return [v[i] + 2*(w*c[i]+cc[i]) for i in range(3)]


def radial_frame(position):
    """Quaternion transporting local +Y to the sampled radial normal."""
    up = norm(position)
    if up[1] < -.999999:
        return [0, 1, 0, 0]
    return norm_quat([1 + up[1], up[2], 0, -up[0]])


def norm_quat(q):
    length = math.sqrt(sum(x*x for x in q))
    return [x/length for x in q]


def quat_mul(a, b):
    w, x, y, z = a
    W, X, Y, Z = b
    return [w*W-x*X-y*Y-z*Z, w*X+x*W+y*Z-z*Y,
            w*Y-x*Z+y*W+z*X, w*Z+x*Y-y*X+z*W]


def surface(x, z, elevation=0):
    return mul(norm([x, math.sqrt(RADIUS*RADIUS-x*x-z*z), z]), RADIUS+elevation)


def object_(identifier, name, position=(0, 0, 0), rotation=(1, 0, 0, 0), **fields):
    return {"id": identifier, "name": name, "fields": {
        "position": list(position), "rotation": list(rotation), "scale": [1, 1, 1], **fields}}


def body(half, mass=1, motion="static", shape="box", **extra):
    return {"body": {"motion": {"symbol": motion}, "shape": {"symbol": shape}},
            "body.half-extents": list(half), "body.radius": half[0],
            "body.terrain": "",
            "body.mass": mass, "body.friction": .8, "body.restitution": .02,
            "body.initial-velocity": [0, 0, 0], "body.pickable": True,
            "body.managed": False, "body.compound-count": 0,
            "body.fluid-cavity-count": 0, **extra}


def render(asset_id, color=(1, 1, 1)):
    return {"render": {"symbol": "mesh"}, "render.mesh-asset": asset_id,
            "render.texture-asset": "", "render.color": list(color), "render.alpha": 1,
            "render.half-extents": [1, 1, 1], "render.radius": 1,
            "render.secondary-color": [.8, .8, .8], "render.secondary-alpha": 1}


def settings(name):
    return {"name": name, "world-origin": [0, 0, 0],
            "sun-direction": [.3, .8, .45], "sun-color": [1, .96, .86],
            "ambient": [.31, .36, .4], "fluid-scale": 1,
            "linear-rendering": True,
            "background-color": [.20, .36, .48], "exposure": 1,
            "fluid-update-rate-hz": 30, "fluid-hydrostatic-drag-rate": 2,
            "fidelity-policy": {"mode": {"symbol": "none"}}, "next-id": 10000}


def glb(path, positions, normals, colors):
    """Small dependency-free GLB writer for the authored planet surface."""
    data = bytearray()
    views, accessors = [], []
    for values, count in [(positions, 3), (normals, 3), (colors, 4)]:
        offset = len(data)
        data.extend(struct.pack("<" + "f"*len(values), *values))
        views.append({"buffer": 0, "byteOffset": offset,
                      "byteLength": len(data)-offset, "target": 34962})
        accessor = {"bufferView": len(views)-1, "componentType": 5126,
                    "count": len(values)//count, "type": "VEC"+str(count)}
        if count == 3 and len(accessors) == 0:
            accessor["min"] = [min(values[k::3]) for k in range(3)]
            accessor["max"] = [max(values[k::3]) for k in range(3)]
        accessors.append(accessor)
    document = {"asset": {"version": "2.0", "generator": "Hearthworld authored planet"},
                "buffers": [{"byteLength": len(data)}], "bufferViews": views,
                "accessors": accessors, "materials": [{"name": "Meadow and exposed earth",
                    "pbrMetallicRoughness": {"baseColorFactor": [1, 1, 1, 1],
                    "metallicFactor": 0, "roughnessFactor": 1}}],
                "meshes": [{"primitives": [{"attributes": {"POSITION": 0,
                    "NORMAL": 1, "COLOR_0": 2}, "material": 0}]}],
                "nodes": [{"mesh": 0}], "scenes": [{"nodes": [0]}], "scene": 0}
    encoded = json.dumps(document, separators=(",", ":")).encode()
    encoded += b" "*((-len(encoded)) % 4)
    data += b"\0"*((-len(data)) % 4)
    binary = struct.pack("<4sII", b"glTF", 2, 12+8+len(encoded)+8+len(data))
    binary += struct.pack("<I4s", len(encoded), b"JSON") + encoded
    binary += struct.pack("<I4s", len(data), b"BIN\0") + data
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(binary)


def planet_mesh(path):
    # The north cap is deliberately absent from BOTH geometry and collision.
    # A separate bounded M97 volume owns that real excavatable surface.
    start = math.asin(CAP_RADIUS/RADIUS)
    angles = [start]
    while angles[-1] < math.pi-.03:
        angles.append(min(math.pi-.02, angles[-1]+(.025 if angles[-1] < .6 else .055)))
        if angles[-1] == math.pi-.02:
            break
    angles.append(math.pi)
    count = 128
    rings = [[(RADIUS*math.sin(a)*math.cos(j*2*math.pi/count),
               RADIUS*math.cos(a), RADIUS*math.sin(a)*math.sin(j*2*math.pi/count))
              for j in range(count)] for a in angles]
    positions, normals, colors = [], [], []
    for i in range(len(rings)-1):
        for j in range(count):
            k = (j+1) % count
            for triangle in [(rings[i][j], rings[i+1][j], rings[i+1][k]),
                             (rings[i][j], rings[i+1][k], rings[i][k])]:
                a, b, c = triangle
                ab, ac = [b[k]-a[k] for k in range(3)], [c[k]-a[k] for k in range(3)]
                face = [ab[1]*ac[2]-ab[2]*ac[1], ab[2]*ac[0]-ab[0]*ac[2], ab[0]*ac[1]-ab[1]*ac[0]]
                if sum(x*x for x in face) < 1e-12:
                    continue
                if sum(face[k]*a[k] for k in range(3)) < 0:
                    triangle = (a, c, b)
                center = [sum(p[k] for p in triangle)/3 for k in range(3)]
                tint = .93+.07*math.sin(center[0]*.13+math.sin(center[2]*.1))
                grass = [.29, .43, .22]
                if center[1] < 12 and math.sin(center[0]*.035+center[2]*.04) > .6:
                    grass = [.4, .42, .28]
                for point in triangle:
                    positions.extend(point)
                    normals.extend(norm(point))
                    colors.extend([*(x*tint for x in grass), 1])
    glb(path, positions, normals, colors)
    return len(positions)//9


def volume_source(resolution=(23, 11, 23)):
    extent = [32, 12, 32]
    # M97 has BOTH sample and extracted-triangle budgets. Keeping the closed
    # source below 16,384 triangles leaves room for subsequent excavation.
    resolution = list(resolution)
    density = []
    for z in range(resolution[2]):
        for y in range(resolution[1]):
            for x in range(resolution[0]):
                p = [x*extent[0]/(resolution[0]-1)-extent[0]/2,
                     y*extent[1]/(resolution[1]-1)-extent[1]/2,
                     z*extent[2]/(resolution[2]-1)-extent[2]/2]
                absolute = add(TERRAIN_POSITION, p)
                radial = math.sqrt(sum(v*v for v in absolute))-RADIUS
                # Finite air boundary and a real lower solid surface. There is
                # no hidden sphere collider underneath the excavation core.
                value = max(radial, abs(p[0])-15.4, abs(p[2])-15.4, -p[1]-5.4)
                if x in (0, resolution[0]-1) or y in (0, resolution[1]-1) or z in (0, resolution[2]-1):
                    value = max(value, .1)
                density.append(value)
    return {"format": "JudasVolumeTerrain", "version": 1,
            "resolution": resolution, "extent": extent,
            "identity": identity("backyard-source"),
            "meshId": identity("backyard-mesh"),
            "collisionId": identity("backyard-collision"),
            "productDirectory": "Assets/Volumes/backyard", "density": density}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--volume-author", type=Path, default=ROOT/"build/judas_volume_author")
    parser.add_argument("--collision-cook", type=Path, default=ROOT/"build/judas_collision_cook")
    parser.add_argument("--skip-cook", action="store_true")
    args = parser.parse_args()
    for folder in ("Scenes", "Imports", "Assets/models", "Assets/collision", "Assets/scripts",
                   "Assets/prefabs", "Assets/fonts", "Authoring"):
        (PROJECT/folder).mkdir(parents=True, exist_ok=True)
    catalog_path = PROJECT/"Assets/art_sources/asset_catalog.json"
    catalog = json.loads(catalog_path.read_text()) if catalog_path.exists() else {}
    # The asset author supplies these stable names and IDs in the final catalog.
    models = catalog.get("models", catalog)
    aliases = {"pine": "tree_pine", "birch": "tree_birch", "rock": "rock_cluster",
               "crate": "wood_crate", "garden_sign": "woodsign"}
    def model(name):
        name = aliases.get(name, name)
        item = models.get(name, {})
        return item.get("id", identity("model/"+name)) if isinstance(item, dict) else item
    for name, item in models.items():
        if isinstance(item, dict) and "path" in item:
            meta(PROJECT/item["path"], model(name), "mesh")
    tags = ["tree", "rock", "tool", "wood", "stone", "buildable", "carryable", "ground"]
    tag = lambda name: 1 << tags.index(name)
    prefab_names = ["log", "floor", "wall", "fence", "pillar", "stonebase"]
    prefab_ids = {name: identity("prefab/"+name) for name in prefab_names}
    project = {"kind": "project", "schema": 1, "data": {
        "name": "Hearthworld", "legacyGameplay": False,
        "saveIdentity": hashlib.sha256(b"hearthworld-save-v1").hexdigest(),
        "iconAsset": "", "worldManifest": "", "exportScenes": [], "excludeScenes": [],
        "startupScene": "Scenes/home.judas", "assetsDir": "Assets", "scenesDir": "Scenes",
        "savesDir": "Saves", "exportAssetPolicy": "closure", "runtimeAssets": list(prefab_ids.values()) +
            ["f1000000000000000000000000000002"] + [f"f10300000000000000000000000000{i:02x}" for i in range(1, 8)],
        "input": [], "classification": {
            "tags": {"nextId": len(tags), "entries": [{"id": i, "name": t} for i, t in enumerate(tags)]},
            "collision": {"nextId": 1, "entries": [{"id": 0, "name": "Default"}]},
            "render": {"nextId": 1, "entries": [{"id": 0, "name": "Default"}]}},
        "navigation": {"areas": {"nextId": 1, "entries": [{"id": 0, "name": "Default"}]},
                       "nextProfile": 1, "profiles": [{"id": 0, "name": "Default",
                       "radius": .3, "height": 1.8, "slope": 50, "climb": .35}]},
        "localization": {"defaultLocale": "en", "fonts": [], "locales": {}}, "audio": {}}}
    def binding(control, scale=1, deadzone=0):
        return {"control": control, "scale": scale, "deadzone": deadzone}
    def action(name, controls, axis=False):
        return {"name": name, "axis": axis, "bindings": [binding(*c) if isinstance(c, tuple) else binding(c) for c in controls]}
    project["data"]["input"] = [
        action("move_x", [("key:A", -1), "key:D", ("stick:LeftX", 1, .15)], True),
        action("move_y", ["key:W", ("key:S", -1), ("stick:LeftY", -1, .15)], True),
        action("look_x", ["mouse:dx"], True), action("look_y", ["mouse:dy"], True),
        action("look_stick_x", [("stick:RightX", 1, .15)], True),
        action("look_stick_y", [("stick:RightY", 1, .15)], True),
        action("sprint", ["key:Left Shift"]), action("jump", ["key:Space", "pad:South"]),
        action("interact", ["key:E", "pad:West"]), action("primary", ["mouse:Left"]),
        action("grab", ["mouse:Right"]), action("equip_gravity", ["key:1"]),
        action("equip_axe", ["key:2"]), action("equip_pickaxe", ["key:3"]),
        action("equip_shovel", ["key:4"]), action("equip_build", ["key:5"]),
        action("build_rotate", ["key:R"]), action("build_next", ["key:Q"]),
        action("inventory", ["key:Tab"]), action("pause", ["key:Escape", "pad:Start"]),
        action("save", ["key:F5"]), action("load", ["key:F9"]),
        action("camera_toggle", ["key:V"]), action("scroll", ["mouse:wheelY"], True),
        action("ui_click", ["mouse:Left"]), action("ui_accept", ["key:Return", "pad:South"]),
        action("ui_activate", ["key:Return", "pad:South"]), action("ui_back", ["key:Escape", "pad:East"]),
        action("ui_next", ["key:Tab"]), action("ui_up", ["key:Up", "pad:DpadUp"]),
        action("ui_down", ["key:Down", "pad:DpadDown"]),
        action("ui_left", ["key:Left", "pad:DpadLeft"]), action("ui_right", ["key:Right", "pad:DpadRight"])]
    # Optional authored rebinding overrides the generated defaults.
    input_path = PROJECT/"Authoring/input.json"
    if input_path.exists():
        project["data"]["input"] = json.loads(input_path.read_text())
    write(PROJECT/"hearthworld.judasproj", project)
    for name in ("DejaVuSans.ttf", "DejaVuSans.ttf.judasmeta", "LICENSE.txt"):
        shutil.copyfile(ROOT/"projects/living_world_lab/Assets/fonts"/name, PROJECT/"Assets/fonts"/name)

    planet_path = PROJECT/"Assets/models/planet.glb"
    planet_triangles = planet_mesh(planet_path)
    planet_id, planet_collision = identity("planet-mesh"), identity("planet-collision")
    meta(planet_path, planet_id, "mesh")
    collision_path = PROJECT/"Assets/collision/planet.judascollision"
    source = volume_source()
    source_path = PROJECT/"Imports/backyard.judasvolume"
    source_path.write_text(json.dumps(source, separators=(",", ":"))+"\n")
    if not args.skip_cook:
        subprocess.run([str(args.collision_cook), "mesh", str(planet_path), str(collision_path),
                        planet_id, "0"], check=True)
        meta(collision_path, planet_collision, "collision")
        subprocess.run([str(args.volume_author), "cook", str(PROJECT/"hearthworld.judasproj"),
                        "Imports/backyard.judasvolume"], check=True)

    rows = [object_(1, "Hearth / authored planetary collision with excavatable cap", **{
        # The ordinary renderer uses source materials and instance tint; glTF
        # COLOR_0 is not a supported material input. Author the meadow tint here.
        **render(planet_id, [.32, .50, .23]), **body([RADIUS]*3, shape="triangle-mesh"),
        "body.collision-asset": planet_collision, "tags": tag("ground")}),
        object_(2, "Ordinary RadicalGravity field", **{
            "gravity": {"kind": {"symbol": "radial"}, "magnitude": 9.81},
            "gravity.region": {"shape": {"symbol": "sphere"}, "radius": 1000}}),
        object_(200, "Backyard / editable bounded M97 soil", TERRAIN_POSITION, **{
            **render(source["meshId"], [.36, .45, .24]),
            **body([16, 6, 16], shape="triangle-mesh"),
            "body.collision-asset": source["collisionId"], "tags": tag("ground")})]
    start = surface(0, 28, .05)
    rows.append(object_(100, "You / persistent Hearthworld player", start, radial_frame(start), **{
        "motor.enabled": True, "motor.radius": .3, "motor.halfHeight": .6,
        "motor.offset": [0, .9, 0], "motor.stepHeight": .42,
        "motor.supportDistance": .2, "motor.skin": .02, "motor.maxSlopeDegrees": 52,
        "motor.gravityScale": 1, "motor.reorientationDegreesPerSecond": 180,
        "motor.interactionMass": 80, "motor.maxPushImpulse": 20,
        "motor.collisionLayer": 0, "motor.collisionMask": 18446744073709551615,
        "motor.requiredTags": 0, "motor.excludedTags": 0,
        "scripts": 1, "script.0.id": 1,
        "script.0.asset": "f1000000000000000000000000000001",
        "script.0.enabled": True, "script.0.properties": "{}"}))
    rows.append(object_(101, "Compatibility initial view / JS owns all gameplay cameras", start,
                        radial_frame(start), **{"player-start": {"symbol": "first-person"}, "player-start.yaw": 0}))

    for index in range(8):
        angle = 2*math.pi*index/8
        base = surface(math.sin(angle)*10.5, math.cos(angle)*10.5, .03)
        q = radial_frame(base)
        center = add(base, rotate(q, [0, .5, 0]))
        marker = object_(220+index, "Backyard dig-area boundary marker / decoration", center, q,
                         **render(model("pillar")))
        marker["fields"]["scale"] = [.7, .4, .7]
        rows.append(marker)
    if "woodsign" in models:
        base = surface(0, 12.5, .03)
        rows.append(object_(228, "Backyard dig-zone sign / authored mesh lettering", base,
                            radial_frame(base), **render(model("garden_sign"))))

    home_records = []
    home_sites = [surface(0, 22, .12), surface(-23, 30, .12), surface(29, 14, .12),
                  mul(norm([1, -.15, -.45]), RADIUS+.12)]
    home_names = ["Your starting home", "Old orchard cottage", "Quarry cottage", "Far-side explorer's cabin"]
    for home_index, base in enumerate(home_sites):
        identifier = 20+home_index*20
        q = radial_frame(base)
        rows.append(object_(identifier, home_names[home_index],
                            base, q, **render(model("home"))))
        boxes = [("Floor", [0, .05, 0], [4, .1, 3]),
                 ("Back wall", [0, 1.55, -2.9], [4, 1.4, .12]),
                 ("Left wall", [-3.9, 1.55, 0], [.12, 1.4, 3]),
                 ("Right wall", [3.9, 1.55, 0], [.12, 1.4, 3]),
                 ("Front left / real doorway", [-2.4, 1.55, 2.9], [1.6, 1.4, .12]),
                 ("Front right / real doorway", [2.4, 1.55, 2.9], [1.6, 1.4, .12]),
                 ("Door lintel", [0, 2.8, 2.9], [.8, .25, .12]),
                 ("Roof interior", [0, 3.15, 0], [3.9, .14, 2.9])]
        if "home" in models and "colliders" in models["home"]:
            boxes = [("Home collision / real door and windows", b["position"], b["halfExtents"])
                     for b in models["home"]["colliders"]]
        for part, (name, offset, half) in enumerate(boxes):
            rows.append(object_(identifier+1+part, name, add(base, rotate(q, offset)), q,
                                **body(half)))
        for part, panel in enumerate(models.get("home", {}).get("roofColliders", [])):
            angle = panel["rotationZRadians"]
            local = [math.cos(angle/2), 0, 0, math.sin(angle/2)]
            rows.append(object_(identifier+15+part, "Sloped roof / physical collision",
                                add(base, rotate(q, panel["position"])), quat_mul(q, local),
                                **body(panel["halfExtents"])))
        home_records.append({"id": str(identifier), "position": base, "rotation": q})

    tools = []
    home = home_records[0]
    for index, name in enumerate(["axe", "shovel", "pickaxe"]):
        point = add(home["position"], rotate(home["rotation"], [-1.2+index*1.2, 1.2, -2.12]))
        identifier = 300+index
        rows.append(object_(identifier, name.title()+" / collect with interact", point, home["rotation"],
                            **{**render(model(name)), **body([.27, .7, .12], mass=2), "tags": tag("tool")}))
        tools.append({"id": str(identifier), "kind": name, "position": point})
    for index, (name, offset, half) in enumerate([
            ("bed", [-2.5, .16, -.5], [.6, .5, 1.1]),
            ("bench", [2.5, .16, -1.4], [.8, .5, .34]),
            ("tool_rack", [0, .16, -2.65], [.65, .8, .3])]):
        identifier = 320+index
        base = add(home["position"], rotate(home["rotation"], offset))
        center = add(base, rotate(home["rotation"], [0, half[1], 0]))
        rows.append(object_(identifier, "Home "+name+" collision", center, home["rotation"], **body(half)))
        rows.append(object_(330+index, "Home "+name, [0, -half[1], 0],
                            **{"parent": identifier, **render(model(name))}))

    trees = []
    tree_positions = [(-9, 22), (-15, 15), (-20, 5), (-16, -12), (-4, -22),
                      (10, -18), (20, -9), (29, 1), (38, 19), (12, 39),
                      (-6, 41), (-34, 16), (-39, -6), (-29, -24), (-12, -40),
                      (14, -38), (35, -24), (48, -4), (49, 31), (33, 45),
                      (-20, 53), (-42, 39), (-56, 9), (-51, -30), (-24, -57),
                      (10, -62), (49, -43), (65, 13), (56, 52), (-52, 62)]
    # Far-side groves make the whole small planet traversable and worth seeing;
    # every tree still starts static, with identical ordinary harvesting rules.
    tree_sites = [surface(x, z, .02) for x, z in tree_positions] + [mul(norm(v), RADIUS+.02) for v in
        [(1, .25, .1), (1, .1, -.2), (.8, -.2, -.55), (.6, -.4, -.7),
         (.15, -.8, -.5), (-.1, -1, .2), (-.4, -.7, .6), (-.8, -.3, .4),
         (-1, .1, 0), (-.8, .25, -.5), (-.2, .1, -1), (.4, .05, 1)]]
    for index, base in enumerate(tree_sites):
        identifier = 500+index*2
        q = radial_frame(base)
        kind = "birch" if index % 2 else "pine"
        tree_height = models.get(aliases[kind], {}).get("bounds", [[0, 0, 0], [0, 7, 0]])[1][1]
        center = add(base, rotate(q, [0, tree_height/2, 0]))
        rows.append(object_(identifier, kind.title()+" / static until felled", center, q, **{
            **body([.28, tree_height/2, .28], mass=60),
            "tags": tag("tree") | tag("carryable")}))
        rows.append(object_(identifier+1, kind.title()+" visual / follows physical trunk", [0, -tree_height/2, 0],
                            **{"parent": identifier, **render(model(kind))}))
        trees.append({"id": str(identifier), "visual": str(identifier+1), "kind": kind,
                      "position": center, "base": base, "rotation": q,
                      "halfExtents": [.28, tree_height/2, .28], "mass": 60})

    rocks = []
    for index, (x, z) in enumerate([(8, 25), (13, 27), (12, 20), (39, 9),
                                    (-31, 20), (-6, -34), (30, -48), (-52, -15)]):
        identifier = 700+index
        base = surface(x, z, .02)
        q = radial_frame(base)
        center = add(base, rotate(q, [0, .54, 0]))
        rows.append(object_(identifier, "Mineable low-poly stone", center, q, **{
            **body([1.4, .54, 1.03], mass=40), "tags": tag("rock")}))
        visual = object_(800+index, "Mineable stone visual", [0, -.54, 0],
                         **{"parent": identifier, **render(model("rock"))})
        visual["fields"]["scale"] = [2, 2, 2]
        rows.append(visual)
        rocks.append({"id": str(identifier), "position": center, "rotation": q})

    props = []
    for index, (name, x, z, half, mass) in enumerate([
            ("crate", 3, 29, [.5, .5, .5], 10), ("crate", -3, 29, [.5, .5, .5], 10),
            ("barrel", 5.5, 25, [.4, .55, .4], 15)]):
        identifier = 900+index
        base = surface(x, z, .03)
        q = radial_frame(base)
        # Meshes are grounded; root-body center has an offset visual child.
        center = add(base, rotate(q, [0, half[1], 0]))
        rows.append(object_(identifier, "Loose "+name+" / gravity gun", center, q, **{
            **body(half, mass, "dynamic"), "tags": tag("carryable")}))
        rows.append(object_(950+index, name.title()+" visual", [0, -half[1], 0],
                            **{"parent": identifier, **render(model(name))}))
        props.append({"id": str(identifier), "position": center, "rotation": q, "mass": mass})

    # Presentation-only tools are controlled by the project's camera module.
    viewmodels = {}
    for index, name in enumerate(["gravity_gun", "axe", "pickaxe", "shovel", "hammer"]):
        identifier = 400+index
        rows.append(object_(identifier, "First-person "+name, [0, -150, 0], **render(model(name))))
        viewmodels[name] = str(identifier)
    avatar = None
    if "character_adventurer" in models:
        avatar = "105"
        rows.append(object_(105, "Optional third-person adventurer / presentation only", [0, 0, 0], **{
            "parent": 100,
            **render(model("character_adventurer")), "animation.enabled": True,
            "animation.play-on-start": False, "animation.loop": True,
            "animation.clip": "Idle", "animation.speed": 1, "animation.time": 0}))
    previews = {}
    for index, name in enumerate(["floor", "wall", "fence", "pillar", "stonebase"]):
        identifier = 410+index
        fields = render(model(name), [.35, .95, .82])
        fields["render.alpha"] = .48
        rows.append(object_(identifier, "Construction preview / no physics "+name, [0, -150, 0], **fields))
        previews[name] = str(identifier)
    sounds = {}
    for index, name in enumerate(["chop", "dig", "build", "grab", "launch", "pickup", "stone"]):
        identifier = 450+index
        rows.append(object_(identifier, "Ordinary spatial sound / "+name, **{
            "audio-emitter": [], "audio.asset": f"f10300000000000000000000000000{index+1:02x}",
            "audio.enabled": True, "audio.play-on-start": False, "audio.loop": False,
            "audio.spatial": True, "audio.volume": .55, "audio.pitch": 1,
            "audio.reference-distance": 2, "audio.maximum-distance": 35, "audio.rolloff": 1,
            "audio.attenuation": {"symbol": "inverse"}}))
        sounds[name] = str(identifier)
    rows.append(object_(398, "Normal active-camera audio listener", **{
        "audio-listener": [], "listener.enabled": True, "listener.follow-view": True}))
    rows.append(object_(399, "Bounded harvest and building dust", **{
        "particle-emitter": [], "particle.enabled": True, "particle.loop": False,
        "particle.local-space": False, "particle.gravity": False,
        "particle.rate": 0, "particle.lifetime": .6, "particle.size": .075,
        "particle.end-size": .015, "particle.burst": 0, "particle.capacity": 96,
        "particle.spread": [.1, .1, .1], "particle.velocity": [0, 1, 0],
        "particle.variation": [1, 1, 1], "particle.acceleration": [0, -1, 0],
        "particle.texture": "", "particle.color": [.6, .47, .25], "particle.alpha": .8,
        "particle.end-color": [.5, .4, .3], "particle.end-alpha": 0, "particle.seed": 9071}))

    prefab_specs = {"log": ("log", [.23, 1.1, .23], 7, "wood", [0, -1.1, 0]),
                    "floor": ("floor", [1.2, .08, 1.2], 10, "buildable", None),
                    "wall": ("wall", [1.2, 1.25, .16], 15, "buildable", None),
                    "fence": ("fence", [1.2, .8, .12], 6, "buildable", None),
                    "pillar": ("pillar", [.14, 1.25, .14], 8, "buildable", None),
                    "stonebase": ("stonebase", [1.2, .25, 1.2], 50, "buildable", None)}
    for name, (asset, half, mass, tag_name, visual_offset) in prefab_specs.items():
        fields = {**body(half, mass, "dynamic"), "tags": tag(tag_name) | tag("carryable")}
        members = [object_(1, "Hearthworld "+name, **fields)]
        if visual_offset:
            members.append(object_(2, "Grounded log visual", visual_offset,
                                   **{"parent": 1, **render(model(asset))}))
        else:
            members[0]["fields"].update(render(model(asset)))
        path = PROJECT/"Assets/prefabs"/(name+".judasprefab")
        write(path, {"kind": "prefab", "schema": 1,
                     "data": {"settings": settings("Reusable "+name), "objects": members}})
        meta(path, prefab_ids[name], "prefab")

    write(PROJECT/"Scenes/home.judas", {"kind": "scene", "schema": 1,
          "data": {"settings": settings("Hearthworld / a little world worth keeping"), "objects": rows}})
    manifest = {"player": "100", "terrain": "200", "planet": "1", "radius": RADIUS,
                "digRadius": 10, "colliderHoleRadius": CAP_RADIUS,
                "terrainPosition": TERRAIN_POSITION, "terrainSamples": len(source["density"]),
                "terrainResolution": source["resolution"], "planetTriangles": planet_triangles,
                "homes": home_records, "trees": trees, "rocks": rocks, "tools": tools,
                "props": props, "prefabs": prefab_ids, "viewmodels": viewmodels,
                "dust": "399", "avatar": avatar, "previews": previews, "sounds": sounds,
                "buildHalfExtents": {name: spec[1] for name, spec in prefab_specs.items()}}
    write(PROJECT/"Authoring/world_manifest.json", manifest)
    module = PROJECT/"Assets/scripts/world-data.js"
    module.write_text("// Authored IDs and asset references; gameplay lives in ordinary JS.\n"+
                      "export const WORLD = "+json.dumps(manifest, separators=(",", ":"))+";\n")
    meta(module, identity("world-data"), "script")
    embedded = PROJECT/"Assets/scripts/soil-source.js"
    embedded.write_text("// Same immutable source as Imports/backyard.judasvolume.\n"+
                        "export const source = "+json.dumps(json.dumps(source, separators=(",", ":")))+";\n")
    meta(embedded, identity("soil-source"), "script")
    print("Authored", PROJECT/"hearthworld.judasproj", "/", len(rows), "objects")


if __name__ == "__main__":
    main()
