# Terrain V2 — live engineering state (not completed qualification)

Operator approved all five implementation stages, Experimental only, no main/release changes, no automatic conversion of existing projects. Original accepted source before this work: fc4b9a0a301d8988d135badcb08a51f4635df7a7. Existing dirty water evidence PNGs and many unrelated untracked experiments are preserved. No delegated agents authorized for this task.

## Implemented so far

- `src/terrain/TerrainSource.*`: one sampled landscape with normalized material weights and bounded persistent add/remove journal.
- `TerrainMesher.cpp`: canonical shared heightfield/tetrahedral diagonal; only affected chunks become volume geometry. No forced air boundary or hidden base collision. Empty chunks are valid.
- `TerrainRuntime.*`: optional RuntimeTerrain-owned world participant; immutable worker jobs prepare dirty chunks + MK2 collision, owner-thread complete replacement; derived bodies map to terrain identity and are excluded from ordinary native-body save identity, like planet colliders.
- Existing RuntimeTerrain volume path retained. Landscape records use the same Capture/Restore/Owners/streaming participant, not a second persistence system.
- World region visibility/physics enable/filter synchronization added, NOT YET tested after latest edits.
- Renderer terrain blend uses existing shader lighting with four layer textures and streamed weight image; explicit batch boundary. Reuses material sampler units (0 and 5..8), leaves shadow/weather/light/environment units intact; no additional sampler limit requirement.
- Terrain author CLI accepts v2-validate / v2-cook / v2-import. Native source remains editable, ordinary cooked model/collision plus registered JS data module supplies existing editor preview/export path. This first cook is not yet production transaction-qualified.
- `tools/create_terrain_v2_workshop.py` creates ONLY new `projects/terrain_v2_workshop`; untouched existing projects. Explorer movement/brush/tunnel/save/pause are ordinary game JS.

## Verified interim results (must rerun exact final code)

- `build/judas_terrain_v2_tests`: 20 checks pass, actual two-chunk MK2 opening; ~11.7 ms inclusive diagnostic run, NOT matched performance claim.
- `build/judas_terrain_v2_world_tests --write /tmp/terrain-v2-cold.json`: 45 checks pass, full world save/restore, character-sized native capsule sweep through four-chunk tunnel, origin shift, destroy cleanup. Separate `--read` process passes 3 checks.
- `judas_living_world_tests /tmp/terrain-v2-legacy-regression` exited 0; /tmp/terrain-v2-legacy.log.
- Desktop actual gameplay traversed excavated hill from z=13 to z=-12.19 using real explorer/MK2 motor. `/tmp/terrain-v2-desktop-play.log`, screenshots `/tmp/terrain-v2-desktop-{intact,entrance,through}.png`. Capture at 650 still shows pending cuts; later actual traversal completed. Visuals functional but still plain/green; NOT visually qualified.

## Desktop access correction — IMPORTANT

Default restricted exec cannot access NVIDIA and reports llvmpipe. This DOES NOT mean the user's driver is broken. User explicitly corrected that mistake. Escalated desktop execution confirmed GTX 970 / driver 580.178.04 / 4096 MiB, and real game log OpenGL4.6 NVIDIA. Use require_escalated for desktop graphics launches/benchmarks; ordinary compilation/tests can remain restricted. Do not repeat claim of unavailable GPU. Only functionally qualify llvmpipe as portable fallback, not actual hardware performance.

## Current running build

`cmake --build build --target judas_terrain_v2_world_tests judas_terrain_v2_tests judas judas_editor -j 2`
exec session 83134, log /tmp/terrain-v2-current-build.log. 8 GB host, keep build concurrency 2.
Several header changes were made during earlier build; repeat an incremental build after latest source edits before claiming final exact-code results.

## Next required work (continue, do not stop at this document)

1. Finish/re-run Stage 1 exact build, source/geometry/world/cold tests; add regional private publication and collider-enable/filter regressions. Correct terrain material source-local Y/rotation texture phase before origin visual claims. Format new C++ cleanly, clear warnings. Runtime source copying was removed from mesher; it now filters edit pointers instead.
2. Usable native Terrain V2 editor authoring (current ordinary cooked scene is previewable, but native brush workspace not yet implemented). Preserve Editor V2 registry extension; no giant editor rewrite.
3. Stage 2 scalable residency/LOD and shared grid representation. Current V2 landscape runtime still retains all <=1024 chunks, NO view streaming/flat LOD. Planet V1 precision intact but NOT V2 excavation integration. Do not call these done.
4. Remaining planned authoring/material/height import, safe cooking transactions, runtime edits + nav/fluid/particles integrations, streaming/cold/save/export/SDK, representative benchmarks.
5. Existing original plan recommends heightfield base with local sparse volume overrides, early real terrain opening (now achieved), keep planetary double local frames and established native collision publication. Must not claim flat-only initial slice fulfils all five stages.

## Constraints

Main and supported releases unchanged; Alpha Advanced Fluid lock unchanged. Existing projects unmodified, no release or unlock authorized. Human gameplay/visual approval of new terrain still pending. Preserve all failed experiment evidence. Use meaningful actual game tests and representative scene performance, not just extraction assertions.

## Later Stage 1 progress (18:xx UTC)

- Actual NVIDIA desktop access verified with escalated execution. Do not repeat
  the software-driver mistake. Native editor UI checks ran on GTX970 and passed
  sculpt/undo/redo/guard/save. Initial test counted frames as checks; this has
  been corrected so the final check count represents assertions.
- First full-column geometry was replaced with sparse vertical edit bands,
  retaining untouched heightfields above/below. A 1000m elevation case passes.
- Initial float band joins genuinely failed T-junction and nonmanifold native
  cooking. Retained failed source/logs in `attempts/`. Correction uses consistent
  binary64 construction/interpolation/global integer sample coordinates, then
  binary32 vertex storage, plus conforming edge subdivision. Cooker unchanged.
- Geometry 23 checks pass; world 53 checks pass (includes actual native capsule,
  ordinary full save, collider toggling, region hide/publish, origin and cleanup);
  separate cold process 3 pass. Final exact-code run pending below.
- `TerrainV2Editor.*` implements an actual module-owned source workspace, typed
  brushes, async dirty previews, undo/redo, save/update and viewport routing via
  the existing terrain tool. It doesn't touch the editor frame loop. First
  disposable-copy native UI run passed. Latest receipt/guard changes need rerun.
- Native `v2-cook` now uses existing guarded authoring transaction for source,
  model/collision, JS data module and receipt. It conforms combined edited mesh
  topology before native cooking. Latest transactional code needs qualification.
- API/type check passes after reviewed regenerated inventory; 13 Python scripting
  catalogue/editor architecture checks pass.
- Initial matched per-edit geometry+BVH prep benchmark (30 alternating pairs)
  showed full129² median40.22ms vs dirty1.68ms, full181²88.46ms vs dirty1.81ms.
  Compiler activity overlapped that timing; REPEAT in isolation before claiming
  an accepted speedup. This is preparation cost, NOT FPS or complete edit latency.
- Focused runner initially reports unavailable due stale untracked-source
  fingerprint. This is provenance, not a test assertion failure. Finish edits,
  reconfigure/build, then rerun exact gates; do not suppress freshness checks.

Current single combined build session 88276, log
`/tmp/terrain-v2-checkpoint-validation-build.log`; targets geometry/world/editor
UI/benchmark/author/editor/legacy living world/planet terrain+world/judas.
Final tiny TerrainPass optional-path optimization was changed after its build
started; rerun an incremental build before exact qualification. Keep one build
at a time. Earlier builds have finished.

Do not claim whole Terrain V2 complete: scalable flat residency/shared grids,
planetary excavation integration, proper high-bit-depth importing and new-project
creation, fluid/nav/particle qualification and final SDK/exports remain.
The project embeds source through registered generated JS for this first slice;
production paged native terrain assets are still needed for large datasets.

## Current pending regression correction

PlanetWorldTests found a real integration assumption after planet body ownership
was made visible: the motor save restored support via a single RuntimeBody(id),
but a planet owns many generated bodies and no seed body. Existing cold-save
assertion failed with "missing required support body". Do NOT remove this test
or revert the owner identity just to hide it.

Implemented `WorldTerrainQueries.cpp`: common OwnedCollisionBodies(id) and
CollisionBodyNear(id,point), with immutable native geometry nearest-point queries.
Motor restore now resolves the correct generated static support; ignored query
filters and motor ignored-body restoration expand the complete owned set.
Pending full build session 98067, log /tmp/terrain-v2-support-build.log. Needs
rerun of planet-world and all new tests. Latest authoritative gate runner
reported unavailable only because untracked-source fingerprint was stale;
reconfigure/build after source is frozen and do not weaken provenance checks.

No commits/pushes have happened yet. Keep going through qualification and make
a verified first checkpoint, then continue the approved remaining stages.

## Support correction verified

The original PlanetWorldTests now passes unchanged: 220 checks, including resumed
physical support after cold save. Terrain world 53, separate cold process 3,
legacy LivingWorld 58 also pass. The geometry suite now includes 24 deterministic
rough-surface/add/remove cases across four chunks (215 assertions total), passing
strict native cooking. First terrain native editor check passed; rerun latest
receipt and journal changes before checkpoint.

Current final incremental build session 93510, log `/tmp/terrain-v2-freeze-build.log`.
Wait for completion, run final focused gates with fresh provenance, native GPU UI,
transactional edited-terrain cooking, genuine game save/quit/load, isolated paired
benchmark and fresh screenshots. No further build should overlap timing runs.

A profiler scope now attributes GPU terrain geometry separately. Ordinary draws
skip terrain-only uniform work. Sparse-band vertices use consistent binary64
construction followed by binary32 storage; the native physics solver is unchanged.

## First checkpoint qualification (2026-11-07, 19:xx UTC)

Current implementation is still uncommitted; no main/release/fluid-lock changes.
Latest exact Stage 1 sources passed geometry215, world70 (including actual
JudasJS independent-owner backpressure and transformed generated collision),
native NVIDIA editor15, PlanetWorld220, PlanetTerrain10074, LivingWorld58,
Python35 and API consistency. Focused authoritative runner passed both new gates.
Native desktop gameplay and separate-process full save/reload passed. First
export attempt correctly rejected a stale exporter fingerprint; matching rebuilt
exporter packaged 15 assets/1scene and the fresh standalone is running relocated.
Final raw evidence and scripts are in stage1/. No compiler overlapped the final
paired benchmark.129² full geometry/BVH median37.342ms vs dirty1.564ms; actual
GTX970 main camera median0.699ms/p95.704ms for the small tunnel scene at1280x800.
Do not generalize these to large-world scaling or complete brush latency.

Next: commit ONLY terrain-owned paths (exclude unrelated water PNGs/research),
push Experimental normally, then continue Stage2. No final task-completion claim.
Stage2 design focus: shared GPU heightfield grids, crack-safe LOD over all camera
interests, separate collision residency with explicit readiness, bounded native
asset pages. Existing PlanetTiles logic is synchronous and unchanged so far.
Latest build sessions completed. Native relocated session94187 log
/tmp/terrain-v2-relocated-play.log; inspect completion before recording export pass.
