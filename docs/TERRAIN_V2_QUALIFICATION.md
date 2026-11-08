# Terrain V2 — integrated Experimental qualification

Engine checkpoint: `4c71ad6ee7eb0b24a24c8194ba1bf358823945a8`.
Development branch: **Experimental**. Main, supported releases, Advanced Fluid
lock and installed consumer SDK selections remain unchanged. This is a local
source-build preview awaiting human terrain acceptance, not an Alpha publication.
Fine-grained physical/data paging remains outstanding; this document does not
claim completion of every item in the original five-stage programme.

## Implemented and available

| Area | Current implementation |
| --- | --- |
| One terrain source | Sampled heightfield, four material weights and sparse volumetric add/remove journal; source owns both ordinary terrain and excavated chunks. |
| Real excavation | Actual tunnel through a hillside across eight chunks, replacing real collision rather than hiding the intact terrain behind a visual overlay. |
| Rendering | Reusable grids, lossless height textures, camera-union/orthographic projected-error LOD, seam stitching, bounded complementary LOD transitions and unchanged-patch reuse. Scene observers have separate history. |
| Materials | Source-anchored triplanar albedo/tint, authored metre scale and PBR roughness per layer, optional shared normal detail on surfaces, cave walls and ceilings. |
| Collision/publication | Strict cooked collision; exact grid specialization for proven ordinary heightfields; private worker preparation and complete publication retain old support until success. |
| Authoring | Flat/Highlands/Island presets, 16-bit heightmaps, source selection, viewport sculpt/paint/dig/add, byte-bounded undo/redo and explicit save/update. Complete immutable brush previews are worker-prepared; cached picking matches the original triangle algorithm. |
| Runtime scripting | Generic per-owner excavation, surface raise/lower/flatten/smooth/paint, asynchronous status/backpressure and ordinary JudasJS lifecycle. Gameplay remains project-owned. |
| Native assets/export | Deterministic indexed sample pages with checksums, importer/dependencies, terrain scene component, standalone exporter and native SDK/tool integration. |
| Persistence | Base asset identity plus journal/sparse raster changes; explicit rejection of changed bases; bounded procedural snapshots; normal world and inactive-region cold saves. |
| Planetary integration | Real geographic base cutout, metre brushes, protected rim, source/provider identity, origin precision and cold regional restoration. Surrounding planet retains its existing LOD. |
| Navigation | Source-owned navigation, dirty geometry publication and compressed-tile reuse, including affected old/new bounds and raster borders. |
| Water/particles | Source-owned solid occupancy, geometry epochs/logical receivers, bounded authored particle collision coverage and persistent reliable impacts. Physical water quantity remains owned by its solver. |
| World streaming | Existing regional lifetime, private preparation, memory accounting, suspension and cold restoration. Collision is resident for the active terrain source/region. |

No original project assets are automatically replaced. The ordinary owner is a
root, unit-scale static object. Unsupported competing geometry/motion ownership
fails preflight. Native terrain cannot double as a volume sensor: use a separate
box, sphere or capsule trigger. Rejected creation preserves the existing object.

## Evidence and practical measurements

The primary current hardware is Ryzen 5 5600 / GTX 970 4 GB, NVIDIA580.178.04,
native X11, GNU15.2 Release. Native desktop4.6 and portable3.3 paths were exercised.
These are measured scopes, not an uncapped whole-game FPS promise.

| Workload | Verified result | Evidence |
| --- | --- | --- |
| 1km/1024-chunk game, 1025² samples, 1m spacing | Matched scene construction median2143.41→1035.29ms; warmed peak RSS roughly24MiB lower; steady simulation unchanged around0.37–0.39ms. | `docs/evidence/terrain-v2/structured-collision/` |
| Exact heightfield collision |64 grids/338 controls compare complete serialized collision assets byte-for-byte with the generic cooker. Irregular geometry retains the generic path. | Same directory. |
| Native two-view terrain, 513² samples, 1280x800 |56draws/28672triangles; default GPU elapsed around0.57ms, shared detail around0.65ms. New shader has about0.08ms default cost against warmed prior repeats; explicitly not a speedup. | `docs/evidence/terrain-v2/material-detail/` |
| Large editor brush hover | Previous verified median2.43ms→about0.0054ms; identical hit points. Merge/query preparation moved to worker; retained preview capacity about14.1MiB lower. | `docs/evidence/terrain-v2/editor-picking/` |
| 1km local terrain navigation edits | Earlier matched complete edit median295→76ms; only16dirty tiles rebuilt,1008reused. Whole initial setup was not improved by that change. | `docs/evidence/terrain-v2/navigation-tile-reuse/` |
| Cold native source/regional persistence | One-kilometre region, geographic patch, inactive edits and world-origin placement pass separate-process write/read controls. | `structured-collision/`, `planetary-integration/`, `save-budget/` |

The collision startup comparison uses identical source/script bytes and alternating
old/new binaries. Every changed collider keeps the same native features, normals,
adjacency, active edges, BVH/order and bounds. The original 300-step seam trajectory
has zero observed difference against monolithic collision. First-run driver/process
memory peaks are retained and are not attributed wholesale to the optimization.

Current focused controls include: world96, component39, exact cook338, generic
collision1305, source preparation26, navigation reuse67, water/particle integration85;
regional streaming69write/13read, planetary components40/9, planetary regions37/14.
Actual editor15/creation46/geographic editor23 pass on both graphics paths. Material86,
original height/shadow978 and LOD/history98822 pass each path; shader47 and ordinary
runtime renderer54 retain their existing expectations. Positive/negative controls,
failed setup attempts, source/binary hashes and raw results remain with each slice.
The scene-sensor negative controls fail on the old code and pass after correction.

## Playable projects and human acceptance

Editable originals:

- `projects/terrain_v2_workshop/terrain_v2_workshop.judasproj`
- `projects/terrain_v2_planet_workshop/terrain_v2_planet_workshop.judasproj`

Use the matching Experimental editor or `build/judas PROJECT.judasproj`.
WASD/mouse move/look; Space jumps. T opens the actual queued tunnel. Keys1–4 select
dig/raise/lower/paint;5 changes paint layer. Left click applies the selected tool;
right click adds material. F6/F7 save/load; R resets; Esc pauses. O shifts origin
in the planetary project. Existing Saves belong to the developer and are preserved.

For material authoring use **World → Terrain tools → Surface materials**. Tile
size is measured in metres; roughness needs a PBR material on the owner. Optional
shared normal detail has its own metre scale and strength. Apply previews; **Save
and update playable terrain** publishes the asset; ordinary scene Save preserves
its placement. Unsaved/pending products block Play rather than silently using a
stale terrain. Detailed source keys and support bounds are in `docs/TERRAIN_V2.md`.

Human checklist:

1. Create a landscape in a fresh ordinary project; select it, sculpt/paint,
   undo/redo, save/update, Play/Stop and reopen.
2. In the Highlands Workshop press T, watch publication, then walk fully through
   the opening. Dig/add near a chunk boundary and verify real support/collision.
3. Change layer metre scale and optional normal detail; inspect the surface,
   cliff, cave wall and ceiling from different directions. Use PBR to inspect
   roughness. No visual acceptance is inferred from the numeric image controls.
4. Move near/far and use multiple views; check stitching and transitions. Repeat
   on a geographic patch, shift origin and check texture/geometry stability.
5. Save, quit the process completely, reopen and load. Verify excavations, paint,
   position, navigation and receiver state. Test a relocated standalone too.

Material-enabled disposable editable examples are retained under
`.cache/terrain-v2/material-detail/{flat,planet}-editable/`; originals were not
changed. Collected screenshots include above-ground and mid-tunnel views in
`docs/evidence/terrain-v2/material-detail/`.

## Reproduction and SDK

The public terrain driver runs the actual consumer game, native cook, export,
relocation, tunnel traversal and separate-process save/load. It refuses nonfresh
or inside-checkout outputs deliberately:

```sh
python3 tools/validation/qualify_terrain_v2.py --build build \
  --output /tmp/judas-terrain-flat-fresh --material-detail --particle-region
python3 tools/validation/qualify_terrain_v2.py --build build \
  --output /tmp/judas-terrain-planet-fresh --material-detail --planet
```

A package contains the same driver and learning projects; point `--build` to the
SDK directory and use its `tools/validation/qualify_terrain_v2.py` to validate using
only packaged tools. Earlier exact28984e7a and b5c1bc0d local SDKs are independently
qualified; b5c1bc0d includes materials but predates the structured cook/sensor fix.
The exact4c71ad6e package is `.cache/terrain-v2/sdk-native-preview-4c71ad6e`:
all17tool probes,842file checks,8standard consumer operations and both relocated
material-enabled games/cold saves pass. It includes all current terrain changes.
Evidence: `docs/evidence/terrain-v2/sdk-final-qualification/`.
These host builds use GLIBC2.43 and do not claim Ubuntu24.04 binary compatibility.
No installed consumer SDK was replaced. No new Windows, Android or browser
qualification is claimed; their previously accepted release evidence is unchanged.

## Remaining work and explicit limitations

- **Independent physical chunk residency and data demand paging remain open.**
  Native page IO exists, but runtime arrays/GPU images are whole-source and all
  collision chunks remain resident with the region. Current bounds are1024chunks
  per source and8owners plus regional memory budgets. Camera visibility must not
  remove support needed by physics, navigation or particle queries.
- Geographic excavation is limited to bounded static, nonoverlapping interior
  cube-face patches with a protected rim. Mapped patches render at full detail.
  No cross-cube-face excavation or moving patch owners.
- Height spacing also defines volumetric resolution. Runtime volumetric smoothing
  is unsupported; surface smoothing is supported. Empty runtime chunks are valid,
  but cooking a wholly empty compatibility preview is unsupported.
- Four albedo layers and one shared normal texture are supported; independent
  normal stacks, virtual texturing, GIS/GeoTIFF authoring and unrestricted V1
  terrain conversion are not claimed.
- Particle collision coverage remains explicit/bounded; insufficient coverage
  reports failure rather than treating unprepared terrain as empty space.
- This is not an unlimited streaming world or automatic asset migration system.
  Changed save base assets require an explicit migration. Existing projects and
  developer choices remain under their owners' control.

The next substantial architectural work is demand paging with physical interests
and query-safe lifetime ownership, not more camera-only culling. The current
region-granular implementation is useful and qualified within its stated limits;
it is not a substitute for that remaining work.
