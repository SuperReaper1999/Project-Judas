#!/usr/bin/env python3
"""Author Lunar Island's editable Stage 1 world. No engine modifications.

Run with --data-only after editing items/recipes/survival. Regenerating an edited
world requires --replace-world explicitly; normal asset cooking preserves sources.
"""
import argparse, hashlib, json, math, random, shutil, subprocess, wave, struct
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
REPO = ROOT.parents[1]


def ident(name):
    return hashlib.sha256(("lunar-island-v1/" + name).encode()).hexdigest()[:32]


def write(path, data):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(data, indent=2) + "\n")


def meta(path, kind, key=None):
    aid = ident(key or str(path.relative_to(ROOT)))
    Path(str(path) + ".judasmeta").write_text(
        f'JudasAssetMeta 1\nid "{aid}"\ntype {kind}\nsource "Lunar Island project authoring"\n'
    )
    return aid


def obj(i, name, p=(0, 0, 0), **f):
    return {
        "id": i,
        "name": name,
        "fields": {
            "position": list(p),
            "rotation": [1, 0, 0, 0],
            "scale": [1, 1, 1],
            **f,
        },
    }


def render_asset(aid, color=(1, 1, 1)):
    return {
        "render": {"symbol": "mesh"},
        "render.mesh-asset": aid,
        "render.texture-asset": "",
        "render.color": list(color),
        "render.alpha": 1,
        "render.half-extents": [1, 1, 1],
        "render.radius": 1,
        "render.secondary-color": [0.8, 0.8, 0.8],
        "render.secondary-alpha": 1,
    }


def mesh(name):
    return render_asset(ident("model/" + name))


def body(half, mass=1, shape="box", motion="static", **f):
    return {
        "body": {"motion": {"symbol": motion}, "shape": {"symbol": shape}},
        "body.half-extents": half,
        "body.radius": half[0],
        "body.mass": mass,
        "body.friction": 0.8,
        "body.restitution": 0.02,
        "body.pickable": True,
        "body.compound-count": 0,
        "body.fluid-cavity-count": 0,
        "body.terrain": "",
        "body.initial-velocity": [0, 0, 0],
        "body.managed": False,
        **f,
    }


def scripts(aid):
    return {
        "scripts": 1,
        "script.0.id": 1,
        "script.0.asset": aid,
        "script.0.enabled": True,
        "script.0.properties": "{}",
    }


def data_only():
    items = json.loads((ROOT / "Authoring/items.json").read_text())
    recipes = json.loads((ROOT / "Authoring/recipes.json").read_text())
    config = json.loads((ROOT / "Authoring/survival.json").read_text())
    assert len({r["id"] for r in recipes}) == len(recipes)
    for k, v in items.items():
        assert v["stack"] >= 1 and v["weight"] >= 0
    for r in recipes:
        for k, n in {**r["ingredients"], **r["outputs"]}.items():
            assert k in items and isinstance(n, int) and n > 0
    p = ROOT / "Assets/scripts/definitions.js"
    p.write_text(
        "// Generated from Authoring JSON. Rebuild with --data-only.\n"
        + "".join(
            f"export const {name} = {json.dumps(value,indent=2)};\n"
            for name, value in [
                ("ITEMS", items),
                ("RECIPES", recipes),
                ("CONFIG", config),
            ]
        )
    )
    meta(p, "script")


def terrain_source(key, width, depth, nx, nz, heights):
    return {
        "format": "JudasTerrain",
        "version": 1,
        "identity": ident(key),
        "meshId": ident(key + "/mesh"),
        "textureId": ident(key + "/texture"),
        "collisionId": ident(key + "/collision"),
        "productDirectory": "Assets/Terrain/" + key,
        "nx": nx,
        "nz": nz,
        "width": width,
        "depth": depth,
        "textureSize": 512,
        "heights": heights,
        "weights": [[1, 0, 0, 0] for _ in heights],
        "layers": [],
    }


# Coast silhouette inspired by the supplied map; measurements are provisional.
COAST = [
    (-0.98, -0.20),
    (-0.86, -0.53),
    (-0.48, -0.72),
    (-0.16, -0.98),
    (0.01, -0.70),
    (0.33, -0.85),
    (0.43, -0.68),
    (0.74, -0.68),
    (0.95, -0.38),
    (0.97, 0.04),
    (0.80, 0.22),
    (0.93, 0.49),
    (0.64, 0.82),
    (0.28, 0.93),
    (0.04, 0.80),
    (-0.19, 0.48),
    (-0.46, 0.69),
    (-0.69, 0.58),
    (-0.77, 0.32),
    (-0.36, 0.12),
    (-0.17, -0.03),
    (-0.30, -0.21),
    (-0.55, 0.02),
    (-0.83, 0.17),
]
ORIGIN = (2900, -1600)


def coast_distance(x, z):
    px, pz = x / 5600, z / 3400
    inside = False
    nearest = 1e9
    for i, (ax, az) in enumerate(COAST):
        bx, bz = COAST[(i + 1) % len(COAST)]
        if (az > pz) != (bz > pz) and px < (bx - ax) * (pz - az) / (bz - az) + ax:
            inside = not inside
        a, b = (ax * 5600, az * 3400), (bx * 5600, bz * 3400)
        dx, dz = b[0] - a[0], b[1] - a[1]
        t = max(0, min(1, ((x - a[0]) * dx + (z - a[1]) * dz) / (dx * dx + dz * dz)))
        nearest = min(nearest, math.hypot(x - a[0] - t * dx, z - a[1] - t * dz))
    return nearest if inside else -nearest


def island_height(x, z):
    d = coast_distance(x, z)
    if d < 0:
        return max(-90, d * 0.18)
    coast = min(1, d / 180)
    h = (
        12
        + 18 * (0.5 + 0.5 * math.sin(x * 0.0024) * math.cos(z * 0.002))
        + 205 * math.exp(-((x + 1100) ** 2 / 2700000 + (z + 600) ** 2 / 900000))
    ) * coast
    # A broad farm clearing: later stages can author fields/buildings here.
    home = math.hypot(x - ORIGIN[0], z - ORIGIN[1])
    blend = max(0, min(1, (home - 550) / 350))
    return 14 * (1 - blend) + h * blend


def interpolate(t, x, z):
    gx = max(0, min(t["nx"] - 1 - 1e-7, (x / t["width"] + 0.5) * (t["nx"] - 1)))
    gz = max(0, min(t["nz"] - 1 - 1e-7, (z / t["depth"] + 0.5) * (t["nz"] - 1)))
    ix, iz = int(gx), int(gz)
    fx, fz = gx - ix, gz - iz
    hs = t["heights"]
    n = t["nx"]
    a, b, c, d = [
        hs[k]
        for k in [
            iz * n + ix,
            iz * n + ix + 1,
            (iz + 1) * n + ix,
            (iz + 1) * n + ix + 1,
        ]
    ]
    return (
        a * (1 - fx - fz) + b * fx + c * fz
        if fx + fz <= 1
        else d * (fx + fz - 1) + c * (1 - fx) + b * (1 - fz)
    )


def ui_document(font):
    es = []
    ink = [0.88, 0.91, 0.84, 1]
    dark = [0.025, 0.055, 0.06, 0.97]
    accent = [0.53, 0.72, 0.62, 1]

    def e(
        i,
        text="",
        kind=2,
        parent="canvas",
        pos=(0, 0),
        size=(300, 30),
        anchor=(0, 0),
        align=(0, 0),
        bg=(0, 0, 0, 0),
        color=ink,
        visible=True,
        fontSize=18,
        wrap=False,
    ):
        d = dict(
            id=i,
            parent=parent,
            kind=kind,
            text=text,
            texture="",
            font=font,
            textKey="",
            textLogicalAlign=-1,
            mirrorRow=False,
            focusColor=[0.98, 0.78, 0.43, 1],
            placeholder="",
            accessibleLabel=text,
            description="",
            readOnly=False,
            maxBytes=4096,
            visible=visible,
            enabled=True,
            clip=False,
            wrap=wrap,
            fit=True,
            spacing=8,
            fontSize=fontSize,
            value=0,
            minimum=0,
            maximum=1,
            flow=0,
            direction=0,
            anchorMin=list(anchor),
            anchorMax=list(anchor),
            offset=list(pos),
            size=list(size),
            relativeSize=[0, 0],
            align=list(align),
            textAlign=[0, 0.5],
            margin=[0, 0, 0, 0],
            padding=[10, 6, 10, 6],
            background=list(bg),
            color=list(color),
            rowHeight=36,
        )
        es.append(d)
        return d

    e("canvas", kind=0, parent="", size=(0, 0))["anchorMax"] = [1, 1]
    e("brand", "LUNAR ISLAND", pos=(28, 20), size=(380, 38), fontSize=28)
    e(
        "chapter",
        "FOUNDATION / HOMESTEAD COAST",
        pos=(30, 59),
        size=(400, 24),
        fontSize=13,
        color=accent,
    )
    e(
        "vitals",
        pos=(-28, 22),
        anchor=(1, 0),
        align=(1, 0),
        size=(480, 70),
        bg=dark,
        wrap=True,
        fontSize=17,
    )
    e("journal", pos=(28, 100), size=(490, 66), bg=dark, wrap=True, fontSize=17)
    e("reticle", "·", anchor=(0.5, 0.5), align=(0.5, 0.5), size=(24, 30), fontSize=30)
    e(
        "context",
        anchor=(0.5, 0.5),
        align=(0.5, 0),
        pos=(-240, 28),
        size=(480, 55),
        wrap=True,
        bg=(0.025, 0.055, 0.06, 0.6),
    )
    e(
        "notice",
        anchor=(0.5, 1),
        align=(0.5, 1),
        pos=(0, -140),
        size=(950, 45),
        color=[0.99, 0.82, 0.53, 1],
        wrap=True,
    )
    e(
        "hotbar",
        anchor=(0.5, 1),
        align=(0.5, 1),
        pos=(0, -70),
        size=(1000, 60),
        bg=dark,
        wrap=True,
        fontSize=17,
    )
    e(
        "hint",
        "WASD move · Mouse look · E gather / interact · Click tool · Tab backpack & crafting · Esc pause",
        anchor=(0.5, 1),
        align=(0.5, 1),
        pos=(0, -18),
        size=(1180, 32),
        fontSize=14,
    )
    e("overlay", kind=1, size=(0, 0), bg=[0.01, 0.025, 0.03, 0.9], visible=False)[
        "anchorMax"
    ] = [1, 1]
    e(
        "panel",
        kind=1,
        parent="overlay",
        anchor=(0.5, 0.5),
        align=(0.5, 0.5),
        size=(1100, 640),
        bg=dark,
    )
    e("title", "FIELD PACK", parent="panel", pos=(24, 16), size=(850, 44), fontSize=29)
    for i, label, x in [
        ("pack", "Backpack", 24),
        ("craft", "Crafting", 190),
        ("map", "Field map", 356),
        ("help", "Controls", 522),
    ]:
        e("tab_" + i, label, 4, "panel", (x, 72), (154, 40), bg=[0.13, 0.25, 0.25, 1])
    e(
        "close",
        "Return to island",
        4,
        "panel",
        (870, 72),
        (200, 40),
        bg=[0.20, 0.38, 0.31, 1],
    )
    e(
        "entries",
        kind=9,
        parent="panel",
        pos=(24, 132),
        size=(520, 330),
        bg=[0.05, 0.10, 0.11, 1],
    )
    e("detail", parent="panel", pos=(568, 132), size=(500, 252), wrap=True, fontSize=18)
    e(
        "primary",
        "Use / equip",
        4,
        "panel",
        (568, 405),
        (235, 46),
        bg=[0.20, 0.38, 0.31, 1],
    )
    e("drop", "Drop one", 4, "panel", (823, 405), (235, 46), bg=[0.22, 0.20, 0.18, 1])
    e(
        "menu_status",
        parent="panel",
        pos=(24, 485),
        size=(1030, 52),
        wrap=True,
        color=accent,
    )
    for i, label, x in [
        ("save", "Save world · F6", 24),
        ("load", "Load save · F9", 290),
        ("quit", "Save and quit", 556),
    ]:
        e(i, label, 4, "panel", (x, 565), (244, 45), bg=[0.13, 0.25, 0.25, 1])
    e(
        "sensitivity",
        kind=5,
        parent="panel",
        pos=(24, 435),
        size=(480, 36),
        visible=False,
    )["value"] = 0.5
    e(
        "map_image",
        kind=3,
        parent="panel",
        pos=(24, 128),
        size=(620, 414),
        visible=False,
    )["texture"] = ident("map-reference")
    return {
        "kind": "ui",
        "schema": 1,
        "data": {
            "reference": [1280, 720],
            "visible": True,
            "enabled": True,
            "modal": False,
            "elements": es,
        },
    }


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--data-only", action="store_true")
    ap.add_argument("--replace-world", action="store_true")
    args = ap.parse_args()
    data_only()
    if args.data_only:
        return
    scenePath = ROOT / "Scenes/island.judas"
    if scenePath.exists() and not args.replace_world:
        raise SystemExit(
            "Scene exists. Use --data-only or explicitly --replace-world; keep hand edits in separate scenes."
        )
    catalog = json.loads((ROOT / "ArtSource/asset_catalog.json").read_text())
    for name, d in catalog.items():
        meta(ROOT / d["path"], "mesh", "model/" + name)
    for p in (ROOT / "Assets/scripts").glob("*.js"):
        meta(p, "script")
    for name in ["DejaVuSans.ttf", "LICENSE.txt"]:
        shutil.copyfile(
            REPO / "projects/hearthworld/Assets/fonts" / name,
            ROOT / "Assets/fonts" / name,
        )
    font = meta(ROOT / "Assets/fonts/DejaVuSans.ttf", "font")
    ref = Path("/home/conner/Documents/ChatGPT Image 25 Oct 2026, 22_13_24.png")
    shutil.copyfile(ref, ROOT / "ArtSource/Lunar-Island-reference.png")
    shutil.copyfile(ref, ROOT / "Assets/textures/field-map.png")
    meta(ROOT / "Assets/textures/field-map.png", "texture", "map-reference")
    uid = meta(ROOT / "Assets/ui/game.judasui", "ui")
    write(ROOT / "Assets/ui/game.judasui", ui_document(font))
    project = json.loads(
        (REPO / "projects/hearthworld/hearthworld.judasproj").read_text()
    )
    d = project["data"]
    d.update(
        name="Lunar Island",
        saveIdentity=hashlib.sha256(b"LunarIslandCampaign-Stage1-v1").hexdigest(),
        startupScene="Scenes/island.judas",
        runtimeAssets=[ident("loot-prefab")],
        advancedFluidDynamics=False,
        basicWater=True,
        waterSwimming=True,
    )
    d["classification"]["tags"] = {
        "nextId": 6,
        "entries": [
            {"id": i, "name": n}
            for i, n in enumerate(
                ["ground", "tree", "resource", "loot", "freshwater", "campfire"]
            )
        ],
    }
    bindings = {
        "move_x": [("key:A", -1), ("key:D", 1), ("stick:LeftX", 1)],
        "move_y": [("key:W", 1), ("key:S", -1), ("stick:LeftY", -1)],
        "look_x": [("mouse:dx", 1)],
        "look_y": [("mouse:dy", 1)],
        "look_stick_x": [("stick:RightX", 1)],
        "look_stick_y": [("stick:RightY", 1)],
    }
    actions = {
        **{k: {"axis": True, "keys": v} for k, v in bindings.items()},
        **{
            k: {"axis": False, "keys": [(v, 1)]}
            for k, v in {
                "jump": "key:Space",
                "sprint": "key:Left Shift",
                "dive": "key:Left Ctrl",
                "interact": "key:E",
                "primary": "mouse:Left",
                "inventory": "key:Tab",
                "pause": "key:Escape",
                "save": "key:F6",
                "load": "key:F9",
                "camera_toggle": "key:V",
                "map": "key:M",
                **{f"hotbar_{i}": f"key:{i}" for i in range(1, 7)},
                "ui_click": "mouse:Left",
                "ui_activate": "key:Return",
                "ui_back": "key:Escape",
                "ui_up": "key:Up",
                "ui_down": "key:Down",
                "ui_left": "key:Left",
                "ui_right": "key:Right",
                "ui_next": "key:Tab",
            }.items()
        },
    }
    d["input"] = [
        {
            "name": k,
            "axis": v["axis"],
            "bindings": [
                {
                    "control": c,
                    "scale": s,
                    "deadzone": 0.15 if c.startswith("stick:") else 0,
                }
                for c, s in v["keys"]
            ],
        }
        for k, v in actions.items()
    ]
    write(ROOT / "lunar_island.judasproj", project)
    coarse = []
    for n, cx in enumerate([-2800, 2800]):
        heights = []
        for iz in range(129):
            z = iz * 6800 / 128 - 3400
            for ix in range(129):
                x = cx + ix * 5600 / 128 - 2800
                h = island_height(x, z)
                if abs(x - ORIGIN[0]) < 340 and abs(z - ORIGIN[1]) < 340:
                    h -= 20
                heights.append(h)
        t = terrain_source("coast_" + str(n), 5600, 6800, 129, 129, heights)
        coarse.append(t)

    def baseHeight(x, z):
        return interpolate(coarse[0 if x < 0 else 1], x - (-2800 if x < 0 else 2800), z)

    def localH(x, z):
        globalX, globalZ = x + ORIGIN[0], z + ORIGIN[1]
        h = island_height(globalX, globalZ) - 6 * math.exp(
            -((x - 90) ** 2 + (z + 100) ** 2) / 1800
        )
        clearing = max(0, min(1, (math.hypot(x, z) - 30) / 75))
        pond = 1 - math.exp(-((x - 90) ** 2 + (z + 100) ** 2) / 2500)
        h += (
            (
                2.5 * math.sin(x * 0.028) * math.cos(z * 0.025)
                + 0.5 * math.sin((x + z) * 0.065)
            )
            * clearing
            * pond
        )
        edge = min(320 - abs(x), 320 - abs(z))
        a = max(0, min(1, edge / 55))
        return baseHeight(globalX, globalZ) * (1 - a) + h * a

    detail = terrain_source(
        "homestead",
        640,
        640,
        129,
        129,
        [localH(ix * 5 - 320, iz * 5 - 320) for iz in range(129) for ix in range(129)],
    )
    detail["textureSize"] = 1024

    def ground(x, z):
        return (
            interpolate(detail, x, z)
            if abs(x) < 320 and abs(z) < 320
            else baseHeight(x + ORIGIN[0], z + ORIGIN[1])
        )

    # Small original tiled textures and editable native paint layers. Geometry
    # remains unchanged; shoreline and footpath are ordinary terrain weights.
    palette = ["grass", "sand", "rock", "earth"]
    for name in palette:
        path = ROOT / f"Assets/textures/{name}.png"
        if not path.exists():
            raise RuntimeError(
                "Run generate_assets.py before authoring terrain materials"
            )
        meta(path, "texture", "ground/" + name)
    for tile, center in zip([*coarse, detail], [(-2800, 0), (2800, 0), ORIGIN]):
        tile["layers"] = [
            {"key": name, "texture": ident("ground/" + name), "tileMetres": 12}
            for name in palette
        ]
        for z in range(129):
            for x in range(129):
                k = z * 129 + x
                h = tile["heights"][k]
                gx = center[0] + x * tile["width"] / 128 - tile["width"] / 2
                gz = center[1] + z * tile["depth"] / 128 - tile["depth"] / 2
                sand = max(0, min(1, (3 - h) / 3))
                rock = max(0, min(0.85, (h - 38) / 70))
                earth = 0
                if tile is detail:
                    lx, lz = gx - ORIGIN[0], gz - ORIGIN[1]
                    earth = (
                        max(0, 1 - abs(lz - (12 + 5 * math.sin(lx * 0.025))) / 5) * 0.75
                    )
                other = min(1, sand + rock + earth)
                total = sand + rock + earth
                tile["weights"][k] = [
                    1 - other,
                    *(
                        [
                            sand * other / total,
                            rock * other / total,
                            earth * other / total,
                        ]
                        if total
                        else [0, 0, 0]
                    ),
                ]
    for name in ["grass_normal", "grass_orm"]:
        meta(ROOT / f"Assets/textures/{name}.png", "texture", "ground/" + name)
    mat = ROOT / "Assets/materials/homestead.judasmat"
    mat.parent.mkdir(exist_ok=True)
    mat.write_text(
        "JudasMaterial 1\nmodel 1\nbase 1 1 1 1\nmetallic 0\nroughness 1\nnormal .7\n"
        + f'map 0 "{detail["textureId"]}" 10497 10497 9987 9729\n'
        + f'map 1 "{ident("ground/grass_orm")}" 10497 10497 9987 9729\n'
        + f'map 2 "{ident("ground/grass_normal")}" 10497 10497 9987 9729\n'
        + f'map 3 "{ident("ground/grass_orm")}" 10497 10497 9987 9729\n'
        + "".join(f"mapUV {i} 0 320 320 0 0 0\n" for i in [1, 2, 3])
    )
    material = meta(mat, "material", "ground/homestead-material")
    rows = []
    for i, (t, center) in enumerate(
        zip(
            [*coarse, detail],
            [
                (-2800 - ORIGIN[0], 0, -ORIGIN[1]),
                (2800 - ORIGIN[0], 0, -ORIGIN[1]),
                (0, 0, 0),
            ],
        )
    ):
        write(
            ROOT / f"Authoring/{['coast_0','coast_1','homestead'][i]}.judasterrain", t
        )
        rows.append(
            obj(
                1 + i,
                "Editable terrain / "
                + ["west island", "east island", "homestead detail"][i],
                center,
                **{
                    **render_asset(t["meshId"], [1, 1, 1]),
                    "render.texture-asset": t["textureId"],
                    **body(
                        [t["width"] / 2, 300, t["depth"] / 2],
                        shape="triangle-mesh",
                        **{"body.collision-asset": t["collisionId"]},
                    ),
                    "tags": 1,
                },
            )
        )
    rows[2]["fields"]["render.materials"] = f'1 "{material}" 0'
    rows.append(
        obj(
            4,
            "Uniform island gravity",
            **{
                "gravity": {"kind": {"symbol": "uniform"}, "magnitude": 9.81},
                "gravity.region": {
                    "shape": {"symbol": "box"},
                    "halfExtents": [15000, 3000, 15000],
                },
            },
        )
    )
    for i, name, pos, half, color in [
        (
            5,
            "Ocean / basic prescribed water",
            [-ORIGIN[0], 0, -ORIGIN[1]],
            [6200, 200, 4000],
            [0.018, 0.23, 0.29],
        ),
        (
            6,
            "Homestead spring pool / swim here",
            [90, 10, -100],
            [65, 12, 65],
            [0.025, 0.33, 0.34],
        ),
    ]:
        rows.append(
            obj(
                i,
                name,
                pos,
                **{
                    "water.basic": json.dumps(
                        {
                            "version": 1,
                            "enabled": True,
                            "halfExtents": half,
                            "color": color,
                            "flow": [0.08, 0, 0],
                            "waveAmplitude": 0.10 if i == 5 else 0.025,
                            "wavelength": 5,
                            "waveSpeed": 1.1,
                            "opacity": 0.83,
                            "density": 1000,
                        }
                    )
                },
            )
        )
    spawn = [0, ground(0, 28) + 0.03, 28]
    rows.append(
        obj(
            100,
            "Lunar Island player / project JS",
            spawn,
            **{
                "motor.enabled": True,
                "motor.radius": 0.3,
                "motor.halfHeight": 0.6,
                "motor.offset": [0, 0.9, 0],
                "motor.stepHeight": 0.45,
                "motor.supportDistance": 0.2,
                "motor.skin": 0.02,
                "motor.maxSlopeDegrees": 50,
                "motor.gravityScale": 1,
                "motor.reorientationDegreesPerSecond": 160,
                "motor.interactionMass": 80,
                "motor.maxPushImpulse": 20,
                "motor.collisionLayer": 0,
                "motor.collisionMask": 18446744073709551615,
                "motor.requiredTags": 0,
                "motor.excludedTags": 0,
                "ui.asset": uid,
                "ui.name": "lunar",
                "ui.enabled": True,
                **scripts(ident("Assets/scripts/player.js")),
            },
        )
    )
    rows.append(
        obj(
            101,
            "Authored initial view / JS owns active camera",
            spawn,
            **{"player-start": {"symbol": "first-person"}, "player-start.yaw": 0},
        )
    )
    rows.append(
        obj(
            102,
            "Third-person presentation avatar",
            (0, 0, 0),
            **{
                "parent": 100,
                **mesh("character_adventurer"),
                "render-visible": False,
                "animation.enabled": True,
                "animation.play-on-start": False,
                "animation.loop": True,
                "animation.clip": "Idle",
                "animation.speed": 1,
                "animation.time": 0,
            },
        )
    )
    resources = []
    rng = random.Random(85321)

    def resource(i, kind, x, z, model, scale=1, amount=1):
        y = ground(x, z)
        s = [scale] * 3
        if kind == "berries":
            model = "berry_bush"
        fields = {**mesh(model), "tags": 4, "scale": s}
        if kind == "tree":
            bounds = catalog[model]["bounds"]
            h = (bounds[1][1] - bounds[0][1]) * scale
            rows.append(
                obj(
                    i,
                    model + " / harvestable physical trunk",
                    [x, y + h / 2, z],
                    **{**body([0.23, h / 2, 0.23], mass=70), "tags": 2},
                )
            )
            rows.append(
                obj(
                    i + 1,
                    model + " visual",
                    [0, -h / 2 - bounds[0][1] * scale, 0],
                    **{"parent": i, **mesh(model), "scale": s},
                )
            )
        else:
            radius = (
                0.22 if kind == "branch" else 0.28 if kind == "loose_stone" else 0.65
            )
            rows.append(
                obj(
                    i,
                    "Gather / " + kind,
                    [x, y + radius * 0.45, z],
                    **{**fields, **body([radius] * 3, shape="sphere")},
                )
            )
        resources.append(
            {
                "id": str(i),
                "kind": kind,
                "position": [x, y, z],
                "amount": amount,
                "height": h if kind == "tree" else 0,
            }
        )

    # A compact first-session loop, then a broader forest trail.
    for i, (x, z) in enumerate(
        [(-3, 22), (2, 20), (5, 22), (-6, 19), (9, 19), (12, 15), (17, 18), (20, 22)]
    ):
        resource(200 + i * 2, "branch", x, z, "log", 0.12, 2)
    for i, (x, z) in enumerate(
        [(-4, 15), (4, 14), (9, 13), (14, 17), (18, 16), (23, 12)]
    ):
        resource(250 + i * 2, "loose_stone", x, z, "rock", 0.22, 2)
    for i, (x, z) in enumerate(
        [(-5, 24), (-7, 21), (-8, 17), (7, 17), (10, 21), (8, 24)]
    ):
        resource(
            280 + i * 2, "berries" if i % 2 == 0 else "fiber", x, z, "fern", 0.65, 4
        )
    for i in range(35):
        x, z = (rng.uniform(-85, 130), rng.uniform(-70, 110))
        resource(
            300 + i * 2, "berries" if i % 3 == 0 else "fiber", x, z, "fern", 0.65, 4
        )
    for i in range(110):
        angle = rng.uniform(0, math.tau)
        radius = rng.uniform(30, 290)
        x, z = math.cos(angle) * radius, math.sin(angle) * radius
        if math.hypot(x - 90, z + 100) < 65:
            continue
        resource(
            500 + i * 2,
            "tree",
            x,
            z,
            ["coastal_pine", "silver_birch", "island_oak"][i % 3],
            rng.uniform(0.8, 1.15),
        )
    for i in range(5):
        resource(390 + i * 2, "salvage", -12 + i * 3, 5, "barrel", 0.3, 2)
    for i in range(45):
        x, z = rng.uniform(-105, -22), rng.uniform(-100, -20)
        resource(
            1100 + i * 2,
            "tree",
            x,
            z,
            ["coastal_pine", "silver_birch", "island_oak"][i % 3],
            rng.uniform(0.7, 1.0),
        )
    for i in range(16):
        resource(
            850 + i * 2,
            "ore" if i % 3 == 0 else "rock",
            rng.uniform(-95, 160),
            rng.uniform(-50, 100),
            "rock",
            rng.uniform(0.8, 1.25),
            6,
        )
    camp = [6, ground(6, 9), 9]
    spring = [18, ground(18, 1), 1]
    rows.append(obj(900, "Homestead fire / cooking station", camp, **mesh("campfire")))
    rows.append(
        obj(
            902,
            "Freshwater well / refill flasks",
            spring,
            **{**mesh("well"), **body([0.85, 1.1, 0.85]), "tags": 16},
        )
    )
    rows.append(
        obj(
            903,
            "Trailhead sign / stage one camp",
            [-5, ground(-5, 8), 8],
            **mesh("woodsign"),
        )
    )
    rows.append(
        obj(904, "Camp chopping block", [2, ground(2, 7), 7], **mesh("chopping_stump"))
    )
    for i, name in enumerate(["axe", "pickaxe"]):
        rows.append(obj(930 + i, "Viewmodel / " + name, [0, -100, 0], **mesh(name)))
    rows.append(
        obj(
            950,
            "Active view audio listener",
            **{
                "audio-listener": [],
                "listener.enabled": True,
                "listener.follow-view": True,
            },
        )
    )
    soundIds = {}
    for i, name in enumerate(["gather", "chop", "craft", "consume", "step"]):
        p = ROOT / f"Assets/audio/{name}.wav"
        r = random.Random(762 + i)
        with wave.open(str(p), "wb") as w:
            w.setnchannels(1)
            w.setsampwidth(2)
            w.setframerate(22050)
            samples = []
            for n in range(5500 if name == "craft" else 2700):
                t = n / 22050
                v = (
                    math.sin(t * (950 if name == "craft" else 160) * math.tau) * 0.5
                    + r.uniform(-1, 1) * 0.5
                ) * math.exp(-t * 28)
                samples.append(int(v * 10000))
            w.writeframes(struct.pack("<" + "h" * len(samples), *samples))
        aid = meta(p, "audio")
        soundIds[name] = str(960 + i)
        rows.append(
            obj(
                960 + i,
                "Original effect / " + name,
                **{
                    "audio-emitter": [],
                    "audio.asset": aid,
                    "audio.enabled": True,
                    "audio.play-on-start": False,
                    "audio.loop": False,
                    "audio.spatial": True,
                    "audio.volume": 0.4,
                    "audio.pitch": 1,
                    "audio.rolloff": 1,
                    "audio.attenuation": {"symbol": "inverse"},
                    "audio.reference-distance": 2,
                    "audio.maximum-distance": 30,
                },
            )
        )
    loot = obj(
        1,
        "Dropped item bundle",
        **{
            **mesh("wood_crate"),
            "scale": [0.25, 0.25, 0.25],
            **body([0.22, 0.22, 0.22], motion="dynamic"),
            "tags": 8,
        },
    )
    settings = {
        "name": "Lunar Island / Homestead Coast",
        "world-origin": [ORIGIN[0], 0, ORIGIN[1]],
        "sun-direction": [0.45, 0.72, 0.3],
        "sun-color": [1, 0.94, 0.79],
        "ambient": [0.22, 0.25, 0.28],
        "background-color": [0.36, 0.56, 0.65],
        "linear-rendering": True,
        "exposure": 1,
        "next-id": 2000,
        "fluid-scale": 1,
        "fidelity-policy": {"mode": {"symbol": "none"}},
    }
    write(
        ROOT / "Assets/prefabs/loot.judasprefab",
        {
            "kind": "prefab",
            "schema": 1,
            "data": {
                "settings": {
                    **settings,
                    "name": "Dropped supplies",
                    "world-origin": [0, 0, 0],
                    "next-id": 2,
                },
                "objects": [loot],
            },
        },
    )
    meta(ROOT / "Assets/prefabs/loot.judasprefab", "prefab", "loot-prefab")
    write(
        scenePath,
        {"kind": "scene", "schema": 1, "data": {"settings": settings, "objects": rows}},
    )
    world = {
        "player": "100",
        "avatar": "102",
        "spawn": spawn,
        "resources": resources,
        "campfire": camp,
        "spring": spring,
        "well": "902",
        "lootPrefab": ident("loot-prefab"),
        "sounds": soundIds,
        "viewmodels": {"stone_axe": "930", "stone_pickaxe": "931"},
        "map": {
            "width": 11200,
            "depth": 6800,
            "origin": ORIGIN,
            "developedRegion": "Homestead Coast; other regions are geography only in Stage 1",
        },
    }
    write(ROOT / "Authoring/world_manifest.json", world)
    p = ROOT / "Assets/scripts/world-data.js"
    p.write_text("export const WORLD = " + json.dumps(world, indent=2) + ";\n")
    meta(p, "script")
    for name in ["coast_0", "coast_1", "homestead"]:
        subprocess.run(
            [
                str(REPO / "build/judas_terrain_author"),
                "cook",
                str(ROOT / "lunar_island.judasproj"),
                f"Authoring/{name}.judasterrain",
            ],
            check=True,
        )
    print(
        "Authored",
        len(rows),
        "entities; resource nodes",
        len(resources),
        "; island bounds 11.2 x 6.8 km (not a verified 64 km2 land area).",
    )


if __name__ == "__main__":
    main()
