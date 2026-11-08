# Terrain V2 — final engineering qualification and human handover

Engine and SDK source: **`23dbefdec8aa5f5ac5a466af85032186357db700`**, Experimental.
The main paging implementation is `376b0a8a5d77dc44c9331c153bf894d46751b91e`;
970c8296 corrects local-edit GPU invalidation discovered during final measurement.
The following a9a98c96 checkpoint corrects private-reader cleanup order; 23dbefde
strengthens the coverage oracle and explicitly verifies graphics-profile execution.
This is an unsupported local Alpha V2 source-build preview, not a publication.
**Human acceptance remains pending.** Main and supported releases are unchanged,
as are the Advanced Fluids lock and the RPG project's installed SDK selection.

**Required source CI completed successfully:** [run 42507574497](https://github.com/SuperReaper1999/Project-Judas/actions/runs/42507574497).
Experimental passed **199/199** registered gates. Alpha passed **187** with **12
predeclared locked-feature cases not exercised**, zero failures; those cases pass
on Experimental. Skipped tests are not counted as passes. Both configurations
also passed clean Ubuntu 24.04 SDK inspection (GLIBC ceiling2.39), 17 tool probes
and 844-file verification. Machine-readable channel/SDK receipts are retained.

## Completed scope

| Area | Available behaviour |
| --- | --- |
| Authoring | Flat, Highlands, Island, 16-bit heightmaps and bounded planetary patches; viewport sculpt/paint/dig/add, source selection, undo/redo and explicit Save and update. Play refuses unsaved or pending source products. |
| Geometry | One source owns the heightfield and sparse excavation journal. A real hillside tunnel crosses chunk boundaries in both rendering and physical collision. |
| Presentation | Shared grids, lossless selected heights/normals, independent native material pages, multi-camera projected-error LOD, seam stitching and complementary transitions. Four source-anchored albedo layers, metre scale, per-layer roughness and shared normal detail also work on cave walls/ceilings. |
| CPU source residency | Immutable 64×64 native sample pages under an 8 MiB per-owner cache. Runtime sources retain no full height/weight arrays. Sparse edits/journals are durable before publication. |
| Collision residency | A 64 MiB per-owner cache retains exact prepared geometry. Small body identities/bounds stay live; physical queries acquire leases. Expensive cave/curved products reload from checked private storage instead of being recooked. |
| GPU residency | A separate 32 MiB per-owner cache is shared across views. Visual eviction cannot remove physics support. Local excavation replaces affected meshes while preserving other uploads; sample changes invalidate their rectangle and dependency halo. |
| Publication | Worker preparation, old support retained until complete replacement, consistent collision/navigation publication and explicit backpressure/failure. |
| Persistence/streaming | Native base identity plus sparse raster/journal state; edited inactive regions, cold processes, source replacement rejection and world-origin placement. Caches reconstruct from authoritative saved data; private readers close before their owned directory is deleted. |
| Integration | Character/wheel/sleeping-body support, global queries, source-owned navigation with dirty-tile reuse, particle snapshots/reliable receivers, fluid solid occupancy and planetary cutouts. Simulation demand is independent of camera visibility. |
| Tools/exports | Deterministic native assets, project/editor integration, 17-tool matching local SDK, SDK-only projects and genuinely relocated flat/planet games. |

Architecture, quotas and error policy are in [Terrain residency](TERRAIN_V2_RESIDENCY.md).
Exact authoring/source/JudasJS contracts remain in [Terrain V2](TERRAIN_V2.md) and
[the scripting reference](judasjs/terrain.md). No existing project assets migrate
or replace themselves automatically.

## Qualification evidence

Fresh local builds use GNU 15.2 Release on Ryzen 5 5600 / GTX 970 4 GB,
NVIDIA 580.178.04, native X11. Both modern OpenGL 4.6 and portable 3.3 were tested
with the explicit `--graphics-profile` gate option. Seven created contexts per
group are checked in the receipts/logs; the portable exported game also uses3.3.
Machine-readable results and compressed raw measurements are retained in
[`residency-final`](https://github.com/SuperReaper1999/Project-Judas/tree/Experimental/docs/evidence/terrain-v2/residency-final).
The source checkout also contains that directory locally; large evidence archives
are not duplicated into the SDK documentation bundle.

| Final local scope | Result |
| --- | --- |
| Affected terrain/navigation/shader gates | **31/31 per graphics profile**. |
| Source paging | 3,556 checks: exact encoding/editing, eviction, all authored chunk sizes, cache concurrency and sparse change bounds. |
| Collision paging | 400,803 checks: exact features/faces/normals/adjacency/BVH, lease lifetime, repeated eviction and checked spill recovery/failure. |
| Render paging | 177 checks per profile, including exact near colour/depth, shadow depth, material detail on coarse/odd grids, shared textures and local invalidation. Forced 32 KiB cache: 41 evictions, peak 32,368 bytes. |
| Independent height/shadow | 978 checks per profile; earlier unpaged references retain their original tolerances. |
| Sustained physical support | 9,293 checks per profile: three passes over all 1,024 chunks while a character walks, wheels drive/support a chassis, and a body sleeps, wakes and settles. |
| Regional cold process | 13 checks after a separate sustained write process; excavation, sculpt and exact continuation retained. |
| Other integration | World 96 checks with the 300-step reference trajectory unchanged; water/particle solid integration 85; navigation reuse 67; ordinary and planetary streaming/authoring/persistence gates also pass. |
| Actual terrain editor | 15 controls per profile: open/select, sculpt, dirty-Play guard, undo/redo, save/update and disk persistence. Creation and planetary authoring are included in the 31-gate runs. |
| Matching SDK | 17/17 identity probes, 844 file checks, 8 standard SDK consumer operations. Both material-enabled terrain exports pass native cooking, dependency checks, relocation, actual tunnel traversal and cold saves; the flat export also retains a reliable consumed particle impact. |

Two initial combined runs failed the unchanged transition oracle's first coverage
comparison while reading the hidden-window default framebuffer. Immediate, paired
and repeated standalone controls did not reproduce its exact trigger. The final
oracle uses real renderer-owned colour/depth attachments, independent of desktop
pixel ownership and drawable preparation, retaining **all** original per-pixel,
draw-order, shadow and distinct-detail expectations/tolerances. Ten alternating
profile controls and both final groups pass 98,839 checks with zero coverage-depth
error. This is a corrected test target contract, not a claimed engine shader fix;
the precise original native trigger remains unconfirmed. Original failures,
source and binary hashes are preserved.

Qualification also found that the wrapper filtered inherited `JUDAS_GL_PROFILE`.
Earlier group runs labelled portable actually selected automatic4.6; that label
is explicitly superseded in the archive. The new explicit option is recorded and
verified against actual contexts. Nineteen wrapper negative/positive controls pass,
including real child-process profile forwarding while unrelated game/save overrides
remain stripped. Direct older3.3 tests are still valid. Reviewed shader identities
changed only after independent colour/depth/shadow comparisons passed.

Earlier scoped results in the other evidence directories remain valid for their
named checkpoints. The former 4c71ad6e qualification report and SDK predate paging;
the old claim that active collision/source arrays must all stay resident is now
superseded. They are historical references, not the current SDK or an open work item.

## Measured costs and limits

The one-kilometre fixture has 1025² native samples, 1 m spacing and 1,024 chunks.
It uses the actual Workshop scripts/renderer and a real queued cross-chunk tunnel.
Measurements ran without concurrent local compilation. GPU timestamps include
ordinary render submission bubbles; these are diagnostic costs, **not FPS promises**.

| Measurement | Result |
| --- | --- |
| Repeated source + collision traversal | 24,576 rendered frames, three complete laps (409.6 simulated seconds). Frame median **1.374 ms**, p95 **2.465 ms**, worst **16.574 ms**. |
| Independent physical query during travel | Median **0.0029 ms**, p95 **1.023 ms**, worst **3.249 ms**. The preceding measured cave-recook path reached **27.600 ms**; checked prepared products remove that repeated cook. |
| Accounted CPU terrain payload | Maximum **77,399,240 bytes (73.81 MiB)**. Every lap ends at exactly **77,291,844 bytes**; no lap-to-lap accumulation. Sample cache stays below 8 MiB and collision below 64 MiB. |
| GPU terrain pages | Maximum **6,359,920 bytes (6.07 MiB)** in the long run. Default 32 MiB working set fits this fixture; separate forced-eviction controls test the eviction path. These are terrain-owned resources, not total driver VRAM. |
| Actual regional support fixture | Each lap ends at **77,164,264 bytes**. Five edited unload/reload visits retain exact changes and refresh regional accounting. Asynchronous load is **1.316–1.536 s**; largest observed integration unit **3.88 ms**. The scheduling target is not a hard frame deadline. |
| Local excavation frames | Before selective invalidation: p95 **78.26–79.52 ms**, worst **85.11 ms**. Final three runs: p95 **2.309–2.656 ms**, worst **3.625–4.118 ms**. Final draws/triangles/payload are identical: 37 / 129,740 / 77,051,648 bytes. |
| Setup and process memory | Final three scene builds **1.317–1.348 s**; short-run peak RSS **307,940–309,276 KiB**, long-run **378,380 KiB**. Prior retained whole-resident structured-cook builds were about **1.035 s** and **504,652–504,936 KiB**. Paging trades roughly 0.30 s of initial setup for lower residency; this is not a startup speedup. |

A separate memory-only short run sampled the NVIDIA driver: the terrain process
peaked at **37 MiB GPU allocation**; device-wide use moved from284 to322MiB,
including desktop applications. This polling run is excluded from timing results.
It is not the long-run terrain-page counter or a GPU memory certification.

RSS includes the game, assets, driver and transient preparation. An earlier first measured
paged run peaked at 366,916 KiB, so the warmed short-run reduction must not be
presented as a clean-machine peak guarantee. Owner accounting excludes external
consumer copies, navigation metadata, driver allocations and bounded worker scratch.
The LOD worker permits up to 16 MiB of temporary height-only data. Eight owners
bound the default cache sum to 576 MiB CPU and 256 MiB GPU; they allocate on demand.

Prepared cave/chart storage has a 32 MiB per-file and 512 MiB per-owner disk limit.
Pinned consumers can exhaust a budget; the engine reports this rather than evicting
live geometry. Missing/corrupt required data reports failure instead of returning
empty terrain. Private spill is not an automatic game save. A deliberately tiny
128 KiB sample-cache experiment thrashed; its failure/cost remains in the evidence.

## Matching SDK and launch instructions

Final SDK: `.cache/terrain-v2/sdk-native-preview-23dbefde` (repository-relative).
Its engine binaries report the exact 23dbefde source revision and Experimental
channel. `source_dirty` truthfully records preserved unrelated PNGs and local
terrain documentation changes; compiled engine/tool inputs match the commit. Untracked research
is observed separately and is not compiled into this package. See `provenance.json`.
The final documentation refresh does not change compiled engine/tool sources.

The local SDK is a **glibc 2.43 host build**. It is qualified on this Linux PC and
does not claim Ubuntu 24.04 binary compatibility. CI separately builds/inspects the
SDK on Ubuntu 24.04. No new Windows, Android or browser terrain certification is
claimed. No consumer SDK selection, including the RPG project, has been changed.

Run from any working directory:

```sh
/home/conner/Documents/JudasTerrainV2Acceptance-23dbefde/launch-editor.sh flat
/home/conner/Documents/JudasTerrainV2Acceptance-23dbefde/launch-editor.sh planet
```

These open the packaged editor with fresh, material-enabled editable review copies
under that dedicated folder. It also contains tested relocated standalone games
under `flat/relocated game/` and `planet/relocated game/`. The launcher accepts
`--play` after the project name to run a review copy directly. Original projects
remain at `projects/terrain_v2_workshop/` and `projects/terrain_v2_planet_workshop/`.

## Ordered human acceptance checklist

1. **Create:** In a new throwaway project use World → Terrain tools → Create
   Terrain V2 landscape. Try Flat/Highlands/Island, select the owner and source,
   then Save and update and save the scene.
2. **Sculpt and paint:** Raise/lower, flatten, smooth and paint. Check brush feel,
   undo/redo, source switching and close-up versus distant appearance. Deliberately
   try Play with a dirty draft; save/update, then test Play/Stop and reopen.
3. **Tunnel:** Open the flat review copy. In Play press T and wait for Ready. Walk
   completely through both ends. Dig/add across a chunk boundary and verify real
   collision/support. Keys 1–4 select dig/raise/lower/paint; 5 changes paint layer.
4. **Materials and views:** Change metre tiling, layer colours/roughness and normal
   detail. Inspect surface, slope, wall and ceiling. Move near/far and compare scene
   observer and game cameras; look for holes, seam pops or disappearing paint.
5. **Gameplay:** Walk/jump over edited support. Check props, particle collision and
   source-owned navigation around the excavation. The automated support fixture
   covers wheels and sleeping bodies; these are not extra hardcoded game systems.
6. **Planet:** Open the planetary review copy, excavate and traverse. Press O to
   shift the origin; inspect support, materials and the protected outer rim.
7. **Streaming:** Leave and return to modified regions in your intended game.
   Watch `entity.terrain.state` cache bytes/pages/evictions. Camera visibility must
   not remove required physical support. The large automated fixture is reproducible
   with the commands in the evidence README.
8. **Persistence/export:** F6 save, exit the process completely, reopen and F7 load.
   Verify excavation, paint, position and particle state. Repeat using a relocated
   standalone launched from another directory. Only then decide human acceptance.

WASD/mouse move/look; Space jumps; left click applies the selected tool; right
click adds material; R resets; Esc pauses. Runtime edits persist in saves, not in
the authored source unless deliberately committed through the editor workflow.

## Declared support boundaries, not unfinished acceptance work

- Root, unit-scale static owners; no competing geometry owner or terrain triangle
  volume sensor. Invalid combinations fail preflight without replacing the object.
- At most eight owners, 1,024 chunks per owner, 65,536 persistent native raster
  overrides and the existing 8 MiB continuation limit. Split large editable worlds
  into sensible regions. This is bounded paging, not unlimited-world authoring.
- Planetary excavation uses nonoverlapping interior cube-face patches and a
  protected rim. Mapped patches draw native detail; surrounding planet LOD remains.
  Cross-cube-face excavation and moving terrain owners are unsupported.
- Volumetric resolution follows authored height spacing. Runtime volume smoothing
  is unsupported; surface smoothing works. Independent normal stacks, virtual
  texturing and GIS authoring are optional future work, not completed features.
- Particle coverage is explicitly bounded. Fluid tests exercise source occupancy;
  this work does not unlock Advanced Fluids or certify new fluid product behaviour.
- Changed base assets require explicit save migration. There is no silent geometry
  replacement or automatic conversion of every V1 project.
- The original default-framebuffer transition observation has no confirmed native
  trigger. The final owned-target oracle and real profile checks pass without
  relaxed expectations. No repeatable Terrain V2 gameplay defect remains
  established by this pass; new platform qualification remains separate.

**Endpoint:** automated engineering qualification followed by the operator's
hands-on review. No main promotion, publication or human acceptance is implied.
