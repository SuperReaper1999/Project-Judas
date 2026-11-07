# Terrain V2 — current engineering handoff

Authority: the complete five-stage Terrain V2 implementation is approved on
Experimental. Continue through ordinary implementation problems. Main, supported
releases, the Alpha fluid lock, existing consumer games/SDKs and independent
research remain untouched. No delegation is authorized. No release is authorized.

## Verified branch checkpoints (verify live Git before changes)

Experimental/origin last checked: 5a7cf7df (query full hash live).
Main/origin unchanged:1094d64515604fe9d03fdab479efc7f880b112a8.
Native asset checkpoint58e73071; authoring b63d707e; runtime sculpt/paint/save
c5fde88d; source-owned solid/fluid/particle integration a64c454a are PUSHED.
Earlier geometry6932fe0b and shared-grid84ae3332 are also pushed.
These are intermediate checkpoints, not full Terrain V2 completion.
Unrelated modified water screenshots and untracked structural/physics/research
files were present initially; never stage/clean/reset them.

NVIDIA GTX970 works on desktop:4096MiB, driver580.178.04, OpenGL4.6.
Restricted calls see llvmpipe. Use require_escalated for native GPU validation.
Use j2, ONE build at a time, no timing during builds. Default restricted process
lists do not see other exec sessions; escalated ps can inspect actual own builds.
Never start a second build before the first session actually exits. If source
changes during a multi-target build, freeze and do a final incremental build.
Exporter/runtime provenance mismatch correctly rejects mixed intermediate binaries.

## Active continuation — actual regional streaming stalls

Native regional handoff + selective reference restoration and payload accounting
are now qualified and checkpoint-ready. Frozen build88111 has EXITED. No build
or test is running. Evidence is regional-streaming/after/. Extraction/baseline
is5a7cf7df; query live Git for the functional checkpoint after commit.

PreparedWorldRegion owns move-consumed source/mesh/BVH/LOD products. Regional
staging installs them without IO/extraction/cooking on the frame thread. Static
staging rollback now retires identity seeds even if no EntityRecord was created.
Preparation25 (including real pending/live budget failures), surface59, native
streaming69/cold13, world93/300-step exact trajectory, component35, interactions85,
existing streaming85, LivingWorldStreaming35 and planetary origin streaming20 PASS.
Existing consumer streamed game cold write11/read19 PASS (copied project). A second
LivingWorldStreaming run rejected its already-existing default fixture; rerun with
its explicit fresh-path argument passed35 unchanged controls. Preserve that log.

Frozen matched1km513²/1024collider run: first activation475ms, revisits471–516ms;
cumulative largest streaming unit3.428ms vsbaseline516.636ms. Existing terrain,
object counts, sculpt/journal data and cold state are preserved. Initial source
still prepares in the background; no FPS or hard2ms guarantee is claimed. Resident
payload77,006,492B now accountssource/mesh/BVH/LOD, updating after edits. PeakRSS
is separate (see time logs), not a heap/GPU cap; no memory speedup claim.

Selective restore checks native header identity, reuses verified live samples,
rereads original pages only for reverted overrides, applies all saved overrides,
and cooks only changed-height/journal dependencies. Paint-only restore preserves
collider IDs. Corrupt required pages reject before publication. No second base
array cache; initial cold source reads still verify every page.

Desktop NVIDIA4.6 editor authoring, undo and PlayStop authored identity PASS.
Real relocated Workshop sculpt/paint/tunnel/traversal+separate-processcold PASS,
including inspected tunnel screenshot. InstalledSDKs/userprojects untouched.
Native128MiB budget is enough for the measured1km source. Default pending32/live64
MiB will correctly reject such a source; split regions or choose appropriate
budgets. Values are payload admission estimates, not hard allocation caps.

NEXT continue actual navigation integration (baker currently sees identityBox),
physical-interest/residency limits and planetary excavation/provider masking.
Further work still includes actual navigation (baker sees placeholderBox), physical
interest/residency beyond whole regions, and actual planetary excavation/provider
masking. Do not claim complete Terrain V2 from more isolated passes.

Last solid integration qualification:85 actual source/water/particle controls,
world93, surface44, component35; unchanged water28/runtime41/script5 and particles
world113/collision37. Source density correctly gives192m³ free capacity vs old shell
replay240m³;64m³ water conserved through dig/flow/save/origin/radial frame. Paint
retains occupancy epoch; incarnation tokens prevent revision reuse after re-create.
Evidence stage5-solid-integration/. Tiny2048cell edit geometry prep14.35ms, steady
median.0158ms; pressure/transport excluded, not a generalFPS claim. Journal scans
are still linear in edit count, so large edited fluid regions need profiling.

## Completed committed foundations

Sampled heightfields plus sparse edit bands share one source; a real tunnel
crosses8chunks. Binary64 construction and consistent face diagonals fix preserved
T-junction/nonmanifold attempts. Native collision cooking remains strict.

Reusable render grids, byte-exact height textures, camera-union/orthographic LOD,
coarse-neighbour edge stitching and native-resolution excavation seams work.
Modern4.6 and portable3.3 framebuffer/shadow tests pass978checks each: maximum
height difference7.19726e-6m; shadow depths match independent CPU geometry.
Motor seam travel matches monolithic collision exactly over300supported steps.
Matched two-camera1km render results/evidence remain in stage2/. GPU timestamp
scope includes submission bubbles; do not call it pure GPU arithmetic speedup.

## Native integration committed at58e73071

TerrainAsset.*: deterministic indexed64x64 sample pages with checksums, exact
binary32 heights/fourweights, bounded header/index-only and independent page
reads. Runtime still materializes ALL source arrays. Not demand paging yet.
AssetType::Terrain/.judasterrain is integrated with importer, dependency graph,
scene key terrain.asset, source fingerprints and exporter. No game JS bootstrap.
Root unit-scale static Box seed provides identity/filter/material; actual chunks
replace support. Competing body/cavity/geometry owners explicitly rejected.

Asset-backed saves use JTRREF2 base-ID/fingerprint plus metadata/editjournal.
Changed base requires explicit migration. Procedural sources save complete native
snapshots within8MiB existing string budget. OldJSON snapshots remain readable.
Restore retains old support until publication and retires obsolete chunks on a
smaller replacement. The1024resident-chunk budget now also applies to Restore.

TerrainRuntime.cpp coordinates physical jobs/publication; TerrainPresentation.cpp
owns GPU/views; TerrainPersistence.cpp owns snapshots. Same class/lifetime/order,
cohesive implementations, no duplicate subsystem. Logical material controls now
propagate to all derived colliders and replacement chunks. Regional stage/private
placement/publication/remove uses common owner placement and cleanup.

Source.Affected now bounded-range scans with exact original halo predicate;
mesher uses direct Touches. An exhaustive control caught tangent rounding at
seed161; preserved failure and unchanged oracle. Conservative extra candidate
then exact predicate fixed all416geometry checks.

v2-native publishes source/adaptive editor preview/texture/receipt; preview is NOT
physicalLOD. v2-pack writes native only. Legacy v2-cook remains available. Receipts
now distinguish native collision ownership from preview and checksum products.
Owned Workshop generator/scene use native asset. Existing Saves never deleted.

## Qualification state and immediate actions

Before the latest budget/material/receipt changes: asset32, component29 (then
extended to35), geometry416, world93, separate-process cold3, LOD339,
modern/portable render978each, native editor15, original PlanetWorld220,
PlanetTerrain10074, LivingWorld58, scene fingerprint140 and export18passed.
The actual relocated game excavated, traversed, saved, quit and restored in a
separate process. tools/validation/qualify_terrain_v2.py reproduces that path,
checks actual script restore state and native cook receipts; no private injection.
It also captures mid-tunnel and restored screenshots; inspect them.

A1km native fixture (513x513,2m spacing) cooked successfully without a monolithic
collision bake and exported/ran after genuine relocation from a foreign cwd.
The warm-file benchmark initially measured ~503ms median scene setup,27–30ms
complete local excavation,594-byte edit snapshot and148420KiB peakprocessRSS.
Rerun latest binary once to include material propagation; do not claim a speedup.
Raw /tmp/terrain-v2-native-km.jsonl and /tmp/terrain-v2-native-km-time.txt.
Large cook/export/play logs /tmp/terrain-v2-native-large-*.log. Profile has no
resolved GPU timestamps: DO NOT derive GPU timings from that particular capture.
No false passing GPU measurement. Existing stage2 native GPU benchmark remains.

COMPLETED for58e73071: final reconfigure/build all changed native targets; component35 and focused
native geometry/world/LOD/assets/components provenance runner; cold3;
modern/portableGPU+editor15; relocated game qualification with latest receipt;
original planet/lifecycle controls. No new functional edits unless failure reveals
real defect. Preserve failure logs. Collect evidence under stage3-native/, scoped
commit and push Experimental. Then CONTINUE remaining approved work below.

## Remaining approved work — not completed by native storage

1. Physics-interest residency/budgets: all collision chunks still resident,1024
perterrain. Native files are paged but runtime arrays/GPUimages still whole-source.
Separate physical interest from camera visibility, preserve support/publication.
2. Fresh source/project creation, highbitdepth heightmaps/presets, multisource
selection, bytebounded undo, runtime surface sculpt/paint+persistence. Base-ref
saves must gain raster deltas or clear base markers when samples change.
3. Planetary excavation/provider override: preserve PlanetTiles precision/LOD and
actually remove base render+collision. A cave prop over intact planet is invalid.
4. Navigation actual terrain geometry/dirty publication, fluid solid occupancy,
particle interactions/stable receiving coordinates, real regional travel cases.
5. Broader materials/visual quality, temporalLOD transition, final consumer copies,
freshSDK/export and platform coverage. Do NOT replace installedRPG SDK or assets.

Known bounds: V1 import squarepitch/16cell/4mtiling; whole-empty ordinary editor
model unsupported (runtime emptychunks valid); runtime V2 smoothvolume unsupported;
volume spacing follows source heightspacing; editor uses a multi-source picker and byte-bounded history; large merged mesh
picking still needs scaling work. Fix where practical or document honestly. Never hide deferred work behind another green demonstration.
