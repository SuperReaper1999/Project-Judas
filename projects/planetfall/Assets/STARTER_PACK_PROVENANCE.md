# Lastlight asset provenance

Lastlight uses a selected subset of the original 90-model starter pack retained
in this repository's ignored `LowPolyAssets/low_poly_starter/` backup. The original
pack and its editable masters are unchanged. The pack README below is retained
as source provenance; it describes the full library, not the subset in this game.

Army/zombie palette variants preserve the character geometry, skeleton and clips.
`character_fighter.glb` adds original Punch/HeavyPunch/Shove clips to that skeleton;
`fist_forearm.glb` is a new original low-poly first-person forearm/fist. Both were
made with Blender 5.2.1; their procedural source is in `Tools/build_combat_models.py`.
The current town-defence revision adds original zombie motion clips in
`zombie_animation.glb` and original launcher/rocket/grenade geometry. Editable
Blender sources are in `ArtSource/`; clip/asset manifests and generators are in
`Tools/`. They share the original character hierarchy without changing the
backup pack. Runtime use needs only the exported GLBs. No downloaded/reference
models are used.
Cloth decorations in Lastlight use static GLB variants, not simulated cloth.

---

# Low Poly Starter

90 original stylized models made in Blender 5.2.1 for building games by hand.
Teal, slate, warm wood and amber accents keep the modern and fantasy pieces
compatible. Most materials use flat colours; the six new fabric assemblies use
original woven-stripe textures embedded in their GLBs. No downloaded models or
external dependencies are required by the individual GLB exports.

![Medieval market and workshop](Previews/medieval-market-vignette.png)

## New medieval set and native cloth

The third batch adds **30 medieval props**, bringing the library to **90**:
24 market/workshop models and six frame-and-fabric assemblies. Designs are
original, inspired by generic medieval market/craft prop types in your reference.

- **New editable master:** `low_poly_starter_90.blend`. It opens on the medieval
  overview and also contains individual edit scenes, the market vignette, and
  a complete 90-model overview. The earlier 30/60 masters remain intact.
- **New Judas project:** `MedievalJudasProject/medieval_props.judasproj`. The
  gallery shows 84 static props and six full-size native cloth assemblies.
  Press Play: WASD moves the flying camera, mouse looks, Q/E lowers/raises,
  Space moves faster, **V toggles breeze**, **N selects the next fabric kit**,
  **R resets fabric**, Esc pauses. All six kits remain simulated; only the
  selected kit receives breeze so resting fabric can sleep normally.
- **Reusable live cloth:** take `MedievalJudasProject/Assets/prefabs/cloth_*.judasprefab`
  with its dependencies, or import them from this project through Judas's normal
  asset workflow. See the [cloth usage notes](MedievalJudasProject/README.md).
- **Static alternatives:** each cloth assembly also has a complete self-contained
  GLB in `Exports/`. That mesh alone is a static visual model; native simulation
  requires its `.judasdeform` resource, material, attachments and prefab.

| Medieval group | Models |
| --- | --- |
| Craft/workshop | `forge_furnace`, `anvil`, `grinding_wheel`, `smith_workbench`, `tool_rack`, `log_stack`, `charcoal_sack`, `bellows`, `water_trough`, `wheelbarrow`, `chopping_stump`, `cooking_tripod` |
| Market/cooking | `produce_crate`, `fish_drying_rack`, `merchant_scale`, `coin_display`, `food_preparation_table`, `ceramic_jug`, `woven_basket`, `grain_sack`, `hanging_lantern`, `rope_coil`, `iron_cauldron`, `spit_roast` |
| Native cloth kits | `cloth_market_canopy`, `cloth_lean_to_awning`, `cloth_banner_stand`, `cloth_laundry_line`, `cloth_table_drape`, `cloth_merchant_tent` |

Cloth is a real existing Judas deformable component, cooked by the normal native
baker. Each fabric mesh is a single textured triangle surface with shared
physical vertices, UV0 and named pin groups. The laundry has three intentional
islands, each pinned separately. Prefabs connect pins to their ordinary static
wooden frame body; the frame uses box compound proxies for contact. Fabric
materials are double-sided. Keep native cloth owner/root scale at **one**;
change source dimensions and rebake when a different physical size is needed.

This is approximate sampled-contact cloth, with no tearing or automatic
CharacterMotor blocking. Wind uses the public `entity.deformable.setMaterial`
API; no new solver or engine behavior was added. Other fabric-looking parts,
including sacks and bellows, are static meshes. Pots, troughs and cauldrons do
not automatically simulate liquids; collision/cavity authoring remains separate.

The medieval additions contain **23,782 Blender triangles** (224–2,008 each).
Across all 90 models there are **65,520 source triangles**; Judas imports 65,516,
discarding four degenerate drying-rack fin triangles. `import-report.json`
records actual native import counts. Prior counts below describe the retained
first two batches. `previous-60-files.sha256` records all prior GLBs and both
earlier editable masters at the start of this extension.

![The 30 medieval additions](Previews/medieval-30-props.png)

![Complete 60-model collection](Previews/collection-60-overview.png)

## Retained 60-model starter set

- **Edit models:** open `low_poly_starter_60.blend` in Blender. The initial scene
  presents all 60 assets. Choose an `Edit / …` scene for a model at its original
  size and pivot, then press **Home** to frame it. Collections are marked as
  Blender assets; add this folder as an Asset Library if desired.
- **Use in a game:** take an individual file from `Exports/`. Every `.glb` is
  self-contained and includes its materials. The character also includes its
  skeleton, skin weights and animation clips.
- **Browse in Judas:** open `JudasProject/starter_pack.judasproj`. The startup
  scene is `Scenes/gallery.judas`. Select and frame a model with **F**; mouse
  wheel zooms, middle drag pans, and right drag with WASD flies around.
- **Import into your own Judas project:** use the Asset Browser's **Import &
  tools** model-import workflow. Keep the source GLB and generated model recipe
  in your project. Enable the scene's linear rendering to use imported material
  colours as authored. Visual meshes and physics collision shapes are separate;
  author/bake collision through the ordinary engine tools when needed.

The Judas gallery is for editing and inspection, with one named entity per model.
Play starts the character's Idle animation but has no game controls or authored
camera. Its display scales normalize sizes for browsing; the underlying GLBs
retain their real dimensions and useful pivots.

The original first-batch file, `low_poly_starter.blend`, is retained unchanged.
The expanded file also contains the original scenes and a separate **new 30 props**
overview, so the extension is easy to inspect without searching all 60.

## Contents

| Group | Models |
| --- | --- |
| Character | `character_adventurer` |
| Equipment | `weapon_longsword`, `weapon_short_sword`, `weapon_round_shield`, `weapon_stylized_pistol`, `weapon_compact_rifle` |
| Modular environment | `ground_tile_4m`, `sidewalk_straight_2m`, `sidewalk_corner`, `sidewalk_ramp_2m`, `road_straight_4m`, `wall_straight_2m`, `wall_corner_2m`, `doorway_frame`, `door`, `stairs_5step`, `shopfront_small`, `fence_2m` |
| Props | `wood_crate`, `metal_crate`, `barrel`, `bench`, `street_lamp`, `bollard`, `traffic_cone`, `dumpster`, `rock_cluster`, `pine_tree`, `broadleaf_tree`, `planter` |
| New street furniture | `trash_bin`, `fire_hydrant`, `parking_meter`, `bus_stop_shelter`, `traffic_light`, `street_sign`, `bicycle_rack`, `utility_box`, `road_barrier`, `manhole_cover` |
| New interiors/workshop | `table`, `chair`, `bookshelf`, `cabinet`, `bed`, `sofa`, `desk_lamp`, `computer_terminal`, `toolbox`, `wall_clock` |
| New outdoors/fantasy | `market_stall`, `well`, `wooden_cart`, `campfire`, `tent`, `treasure_chest`, `clay_pot`, `water_bucket`, `ladder`, `bridge_segment` |

![The new 30 props](Previews/new-30-props.png)

`catalog.json` gives descriptions, exact bounds, dimensions and triangle counts.
Assets range from **16 to 1,960 triangles**, with **41,738 triangles** across the
complete collection. The new 30 contribute 27,528 triangles. Models deliberately
keep separate coloured details;
individual material parts can create multiple draw calls. These are starter
assets, with no LOD chain, collision bodies, gameplay scripts or ragdoll mapping.
There is no shared texture atlas or carefully packed UV layout; unwrap/customize
the meshes in Blender if you want to replace their flat colours with textures.

## Scale and pivots

- One unit is one metre. Blender uses **Z up / -Y forward**; GLB exports use
  **Y up / +Z forward**. This authoring convention does not imply a gravity rule.
- The character's origin is at its feet. Standing height is about 1.87 m.
- Equipment uses a grip/attachment origin. Swords point up; guns point forward.
- Ground and road tops sit at source Z = 0. Sidewalk tops are 0.15 m high;
  straight sections are 2 m long. The ramp descends from +Y to -Y in Blender.
- Walls use an edge origin for snapping. The door's origin is its hinge axis;
  the mesh extends along +X from it. Door and frame are separate assets.
- Ordinary props use a ground-level origin. Trees, rocks and foliage are
  faceted geometry, with opaque materials rather than transparent cards.
- Desktop props sit at source Z = 0 for placing on furniture. The wall clock
  instead uses a centred mounting pivot, with its back at source Y = 0.
- The bridge is a 4 m modular segment; its source ends are at Y = -2 and +2.
  Consult individual catalog descriptions for other placement conventions.

The new props are visual models. Doors/lids, clock hands, signal lights, lamp
heads and cart wheels are static mesh pieces without animation or gameplay.
The tent has an open entrance; vessels have modeled interiors. A bucket does
not automatically become a simulated liquid container: author its body/cavity
through Judas's normal tools if needed. The bucket and well are empty. Campfire
flames are static geometry. Shelter panels use opaque stylized material.

## Character and animation

![Adventurer](Previews/character.png)

The adventurer has **17 deform bones**, a named hierarchy and a segmented,
rigid-weighted low-poly mesh. Limbs rotate as solid pieces around joints; this is
a simple prototype rig rather than a smoothly deforming anatomical character.
It includes four in-place clips at 30 fps:

| Clip | Duration | Use |
| --- | --- | --- |
| `Idle` | 2.0 s | Looping subtle breathing |
| `Walk` | 1.0 s | Looping opposing arm/leg motion |
| `Run` | 0.8 s | Looping stronger motion |
| `Jump` | 1.0 s | One-shot visual pose; scripts/motor supply actual movement |

Clips start at time zero, with no root-motion travel. Use the existing animation
API to select, play, crossfade and mask them. No IK setup or controller is
included. Judas imports two additional hierarchy nodes besides the 17 deform
bones; `import-report.json` records the actual imported joint keys and clips.
In Blender, choose `Edit / character_adventurer`, select `Adventurer_Rig`, and
use the Dope Sheet's **Action Editor** to select a clip. Its source frame range
is 1–61 for Idle, 1–31 for Walk/Jump, and 1–25 for Run.

![Sampled character poses](Previews/character-motion.png)

## Editable source and reuse

All model geometry, materials, stripe textures and clips were created for this
pack. You can use and edit them in your games. The medieval set takes generic
prop-category inspiration from your reference; it does not reproduce that pack's
models, logo or image. No image generation or downloaded models were used.
Blender itself and the Judas engine are not bundled in the asset archive.

The `tools/` scripts preserve the procedural modeling source. From the Judas
repository root, rebuild the extension from the retained first-batch master with:

```sh
/home/conner/Programs/blender-5.2.1-linux-x64/blender \
  --background --factory-startup -noaudio --threads 2 \
  --python asset_packs/low_poly_starter/tools/extend_pack.py
python3 asset_packs/low_poly_starter/tools/create_judas_gallery.py
```

The extension preserves the first 30 GLBs and original .blend, writes only new
GLBs, and saves a separate 60-asset master. It exits its owned background process
after export/render/save finish, avoiding this host's PulseAudio shutdown hang.
`build_pack.py` remains the original first-batch generator; running that script
with `--exit-after-build` recreates the first batch from scratch and overwrites
its source. The gallery generator reads existing Judas templates, so run it
from this repository rather than an extracted standalone pack.

All 60 final GLBs were opened by Judas's native model importer. The recorded
inspection in `import-report.json` also confirms embedded dependencies and the
four character clips. It is an import report, not a claim of human gameplay
validation. Engine source and existing projects were not modified for this pack.
`original-files.sha256` records the first batch at the start of the extension;
those model exports and the original editable master remain byte-for-byte intact.

### Rebuild the medieval extension

From the Judas repository root:

```sh
/home/conner/Programs/blender-5.2.1-linux-x64/blender \
  --background --factory-startup -noaudio --threads 2 \
  --python asset_packs/low_poly_starter/tools/extend_medieval_pack.py
python3 asset_packs/low_poly_starter/tools/bake_medieval_cloth.py
python3 asset_packs/low_poly_starter/tools/create_medieval_project.py
```

The extension reads the preserved 60-model master, exports only the new 30 and
saves a separate 90-model master. `Cloth/cloth_manifest.json` records original
source files, world-basis pin selectors and collision proxies;
`Cloth/baked_manifest.json` records native node counts, pin groups and source
hashes. Changing a fabric GLB requires rebaking. The tiny pin-map tool uses the
existing native asset fields; it does not provide another physics implementation.
Native baking/project generation require an existing Judas checkout and baker.
