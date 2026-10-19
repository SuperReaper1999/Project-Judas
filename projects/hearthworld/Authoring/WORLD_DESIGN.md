# Hearthworld world authoring

This is ordinary project content. `tools/CreateHearthworld.py` reproduces the
project configuration, scene, prefabs, planet and soil source. Project JavaScript
owns tools, gathering, construction, inventory, camera policy and objectives.
There is no survival-game branch in Judas C++.

## Small planet and gravity

- Radius: **120 metres**, centered at the ordinary scene origin.
- Entity `2` authors one **RadicalGravity** field, magnitude 9.81 m/s², region
  radius 1,000 metres. The motor and dynamic bodies use normal field resolution.
- The static planet render/collision mesh is entity `1`: **17,024 triangles**.
  It is authored geometry rather than an M85 adaptive planet or a heightfield.
  This modest world does not need planetary LOD or origin rebasing.
- Scene positions are ordinary simulation-frame metres. Quaternions transport
  local +Y toward each object's radial normal; neither the gameplay nor world
  authoring turns all gravity into global −Y.
- Four enterable cottages and 42 harvestable trees include a far-side cabin and
  groves. Most resource content is near the starting home; this is a compact
  sandbox rather than a uniformly populated globe.

## Excavation owns real geometry

The planet mesh deliberately omits its north-pole cap, whose boundary is the
12-metre horizontal radius ring. **The cooked planet collider omits it too.**
There is no spherical collision primitive underneath this cap.

Entity `200`, at `[0,115,0]`, owns the bounded editable soil volume:

| Property | Authored value |
|---|---|
| Source | `Imports/backyard.judasvolume` |
| Extent | 32 × 12 × 32 metres |
| Grid | 23 × 11 × 23; **5,819 samples** |
| Initial extracted triangles | **12,040** |
| Runtime triangle limit | 16,384 |
| Local cell spacing | approximately 1.455 × 1.2 × 1.455 metres |
| Permitted gameplay dig footprint | north-cap center, horizontal radius 10 m |

The initial signed field is the radius-120 sphere, intersected with a bounded
box and lower surface. The outer grid boundary stays air, as M84 requires.
`Assets/scripts/soil-source.js` embeds the same immutable source for M97 runtime
creation. Rendering, collision and optional native navigation use the normal
M97 preparation/publication path; this project does not replace those systems.

The soil extends outside the 12-metre cap to overlap the ordinary planet surface.
Gameplay restricts excavation to the central area so digging cannot expose a
second solid surface underneath a supposed hole. The finite lower soil boundary
remains real collision. Terrain away from this authored region is **not editable**.
The overlap and finite volume are explicit representation boundaries, not a claim
of arbitrary planet-wide excavation. The backyard can be excavated into a pool
shape; this project does not simulate filling it with water.

Eight short timber markers outline the usable garden area. At its home-facing
entrance, the ordinary `woodsign.glb` has raised mesh letters **BACKYARD / DIG
ZONE**. The sign does not depend on an unexposed engine text feature.

The grid suits a stylized, coarse shovel rather than centimetre-scale sculpting.
Increasing resolution is not free: both sample count and **extracted triangle
count** must stay within M97's existing limits. The first 33 × 21 × 33 candidate
fit the sample budget but produced 24,476 triangles. It was rejected during
authoring review. A 25 × 15 × 25 candidate produced 16,288, leaving too little
editing headroom. The final source leaves 26.5% initial triangle headroom.

Four native remove dabs of radius 1.7 m at local points `[0,4.4,0]`, `[0,3.3,0]`,
`[1.4,4.1,0]` and `[-1.4,4.1,0]` produced 12,148 / 12,164 / 12,184 / 12,208
triangles. These authoring checks exercise the shared brush/extraction/cook;
they do not replace runtime asynchronous-publication and collision tests.

## Cottages and interactable content

The starting home is entity `20`, at the north settlement near surface `[0,*,22]`.
Its front is local +Z, with a real 1.6-metre-wide, 2.5-metre-high doorway. Native
static box colliders follow its floor, split walls, open windows, porch and two
sloped roof panels. The whole building is not represented by one solid box.

The axe (`300`), shovel (`301`) and pickaxe (`302`) start inside, in front of the
tool rack. The home also contains a bed and bench. The player is entity `100`;
its transform is a foot pivot and its motor capsule offset is local `[0,.9,0]`.
The presentation avatar is the separate child `105`, with no second controller
or rigid body.

Standing trees are ordinary **static** 60 kg authored bodies until the project
changes their motion authority for felling. A trunk box spans the tree height;
the full generated tree mesh is a visual child centered at the physical trunk
root. Foliage is not a detailed collision volume. A felled tree rotates and falls
through ordinary dynamics. No canned fall animation is authored here. Processing
and resource quantities are game policy in JavaScript.

Mineable rocks use static conservative box collision and a grounded low-poly
visual child. Loose crates/barrels are dynamic, with ordinary mass and collision.
Only felling, loose props and constructed pieces require ongoing dynamics; the
entire forest is not continuously simulated.

## Construction prefabs

`Assets/prefabs` contains independent dynamic `log`, `floor`, `wall`, `fence`,
`pillar` and `stonebase` prefabs. Each has an authored mass and real primitive
collision. Render meshes and contact shapes are deliberately separate; fence
collision is a conservative box rather than an individual collider for every
wooden rail. Logs contain a grounded mesh child around their physical center.

`WORLD.buildHalfExtents` exposes authored placement dimensions to the project
script. Ordinary M45 fixed joints provide construction attachments. Connection
selection, material costs, destruction and support decisions belong to project
JavaScript, not these asset definitions.

`world-data.js` and `world_manifest.json` identify all authored entities and
prefabs. Presentation tools, previews, particles and audio emitters have no
rigid bodies. Hidden tool previews cannot block player movement or raycasts.

## Reproduction

From the Judas repository root:

```sh
# Recreate generated/coherently authored art first; see ../Assets/ART_PROVENANCE.md.
python3 projects/hearthworld/tools/generate_art.py
python3 tools/CreateHearthworld.py

# Regenerate scene/config/prefabs after art changes without recooking unchanged geometry.
python3 tools/CreateHearthworld.py --skip-cook

build/judas_scene_author --validate project projects/hearthworld/hearthworld.judasproj
build/judas_scene_author --validate scene projects/hearthworld/Scenes/home.judas
build/judas_volume_author validate projects/hearthworld/Imports/backyard.judasvolume
```

Regeneration overwrites the generated scene and project configuration. Preserve
hand-edited scenes under another name first. Optional `Authoring/input.json`
provides a project-owned input-map override. Gameplay modules, UI and audio are
owned separately and are not overwritten by the world generator.

Use the final game report for actual gameplay, cold-save, export and performance
results. Successful asset cooking alone does not establish those contracts or
constitute human acceptance.
