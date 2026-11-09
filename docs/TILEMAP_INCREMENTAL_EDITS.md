# Incremental runtime tile editing — issue #33

Experimental engineering qualification, 2026-11-09. Main and the Advanced Fluids
lock are unchanged; this is not a release or human gameplay acceptance.

## Correction

The old `RuntimeWorld::EditTilemapCell` parsed the recipe, copied every live world
object, flattened and rebaked the whole map, serialized/reparsed the resulting
scene, then published a small difference. Decorative edits paid that cost too.

Runtime editing now compiles only the edited sprite and, for a solid layer, its
16×16 collision chunk. Editor baking and runtime patches use the same compiler;
there is no second collision mesher. Unchanged keys/fingerprints retain IDs and
actual collider handles. Changed definitions are validated and staged before
anything is retired or the durable recipe is published. Invalid requests, manual
target changes, user children and live joint anchors reject without replacing
existing geometry. Decorative edits never rebuild colliders.

Generated replacements now own and release their atlas demand. The independent
reference-count control failed before the ownership correction; one replacement
and forty repeated replacements now retain the same reference count. Authored
atlas demand transfers to removable ownership when its tile is first retired.
Rollback releases staged demand too.

## Matched timings

Ryzen 5 5600, Linux x86_64, GCC 15.2 Release. Independent two-frame 8×4 atlas,
20×14 ground plus 190 decoration cells (470 cells, five layers). Four warmups,
then 40 samples per row. Includes the complete native call and publication; no
compiler or GPU workloads ran during measurement. These are CPU call times,
not rendered frame times or phone measurements.

The before executable uses the exact runtime edit function from `70cb427f`,
linked against the same benchmark/common library to isolate the edit path.
An earlier untouched-parent build independently reproduced the full rebuild
cost (40.26 ms median decoration; 108.59 ms with 2,000 unrelated objects).
Whole-map compiler extraction preserves the existing authoring output.

All table entries are milliseconds: **mean / median / p95 / worst**.

| Edit | Unrelated objects | Before | After |
| --- | ---: | --- | --- |
| Decorative frame change | 0 | 40.60 / 40.51 / 41.43 / 42.09 | 3.87 / 3.81 / 4.15 / 4.49 |
| Solid frame change | 0 | 37.08 / 36.91 / 38.33 / 38.69 | 4.01 / 3.95 / 4.26 / 4.55 |
| Decorative frame change | 2,000 | 75.53 / 75.54 / 76.50 / 76.65 | 4.41 / 4.40 / 4.45 / 4.65 |
| Solid frame change | 2,000 | 76.34 / 76.13 / 77.91 / 78.23 | 4.55 / 4.55 / 4.57 / 4.57 |
| Decorative erase/place | 0 | 42.27 / 41.39 / 49.60 / 64.45 | 3.67 / 3.75 / 3.79 / 3.91 |
| Solid erase/place | 0 | 39.01 / 38.19 / 42.85 / 45.00 | 4.24 / 4.28 / 4.32 / 4.34 |
| Decorative erase/place | 2,000 | 101.31 / 101.84 / 110.16 / 110.99 | 4.01 / 4.30 / 4.33 / 4.35 |
| Solid erase/place | 2,000 | 77.27 / 77.36 / 78.39 / 78.62 | 5.43 / 5.69 / 5.75 / 5.75 |

Whole-world definition copies during all 44 calls fell from approximately
20,800 (or 108,800 with unrelated objects) to **zero**. Read-only reference
scanning still occurs. See the [machine-readable measurements](evidence/tilemap-incremental-edits.json).

## Qualification

- Native tilemap suite: **144 checks**, including real MK2 occupancy across
  six positive/negative chunk-boundary edits, whole-authoring equivalence,
  unchanged IDs/handles, idempotence, bounded resources, invalid-edit rejection,
  manual modifications, child/joint ownership and translated grids.
- Separate cold process: **14 checks**; exact physical state, changed collision,
  recipe/frame/flip/identity preservation and further edits after restoration.
- Real NVIDIA framebuffer: **19 checks**, including changed and erased runtime
  tiles with unchanged neighbour pixels.
- Editor authoring/Undo/Redo: **21 checks**; production application scene
  switch/save/load: **8 checks**.
- Neighbouring planar physics **52**, planar integration **71**, sprite integration
  **57** and modern SaveStorage **49** checks passed.
- A copied Tilemap Workshop was exported and genuinely relocated to `/tmp`.
  Its project JavaScript performed both decorative and solid tile edits, logged
  `RELOCATED_TILE_EDITS_OK`, rendered a screenshot and exited without diagnostics.
  The original project gameplay scripts were not modified.

That export test also exposed stale managed-collider fingerprints in the retained
Beacon Grove and Tilemap Workshop examples. All 22 mismatches were independently
accounted for by the removed `body.terrain`, `body.pickable` and empty
`body.fluid-cavity-count` serialization fields. Only those fingerprints were
refreshed; authored objects, geometry and gameplay remain byte-unchanged. A new
suite check reads all three shipped tilemap scenes and validates their current
managed fingerprints. No runtime compatibility exception or migration was added.

Local raw logs, rejected first attempts, original runtime source, benchmark
executables and export receipts are retained under `.cache/tilemap-edit-33/`.
Final commit and CI status accompany the handover; pending CI is not a pass.

## Reproduce

```sh
cmake --build build --target judas_tilemap_tests -j2
build/judas_tilemap_tests /tmp/judas-tile-edit-fixture
build/judas_tilemap_tests /tmp/judas-tile-edit-fixture --cold
build/judas_tilemap_tests /tmp/judas-tile-edit-benchmark --benchmark
build/judas_tilemap_tests /tmp/judas-tile-edit-topology --benchmark-topology
```

Use fresh caller-owned output directories. The cold invocation follows the first
ordinary invocation; the benchmark is explicitly opt-in, not a timing gate.

## Remaining costs and boundaries

Recipe parse/validation/encoding still scale with the bounded grid; palette and
image admission still read/decode the source image. References are scanned without
copying world definitions, and entity creation/retirement uses ordinary engine
ownership. This is not a constant-time or unlimited/infinite-grid API. Large
atlases and many edits per frame may still be expensive. No batching API or new
cache invalidation system was introduced to hide those costs.

Existing restrictions remain: scene-owned, unparented, unrotated, unit-scale roots;
no online replication or streamed/prefab-instance grid edits. No Android-device,
Windows execution or new whole-engine qualification is claimed for this change.
The [runtime API](judasjs/tilemaps-2d.md#runtime-cell-editing) is unchanged.
