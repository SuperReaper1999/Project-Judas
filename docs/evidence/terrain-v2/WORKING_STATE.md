# Terrain V2 — current engineering handoff

Authority: the complete five-stage Terrain V2 implementation is approved on
Experimental. Continue through ordinary implementation problems. Main, supported
releases, the Alpha fluid lock, existing consumer games/SDKs and independent
research remain untouched. No delegation is authorized. No release is authorized.

## Verified live branch state

Experimental/origin: 84ae3332b8555f6f05defa4b1896727d1923d6b7.
Earlier terrain foundation: 6932fe0b744b3ca16c44384ef0fa10e0cf282185.
Main/origin: 1094d64515604fe9d03fdab479efc7f880b112a8.
The native-source integration described below is UNCOMMITTED until a later
explicit entry records its new hash. These checkpoints are not full completion.
Unrelated water screenshots and untracked structural/physics/research files were
present initially; never stage/clean/reset them. Stage terrain paths explicitly.

The desktop NVIDIA GTX970 works: 4096MiB, driver580.178.04, OpenGL4.6.
Restricted default commands see llvmpipe. Use require_escalated for native desktop
GPU tests/profiles. User explicitly corrected the unavailable-driver assumption.
Use j2 on the8GB host, one build at a time, no timing during compiler activity.

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

## Current native integration (uncommitted)

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

NEXT: final reconfigure/build all changed native targets; component35 and focused
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
volume spacing follows source heightspacing; editor currently first source under
Authoring and merged picker; undo16snapshots countonly. Fix where practical or
document honestly. Never hide deferred work behind another green demonstration.
