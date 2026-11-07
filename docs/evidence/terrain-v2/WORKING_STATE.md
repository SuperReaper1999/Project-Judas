# Terrain V2 — current engineering handoff

Authority: the complete five-stage Terrain V2 implementation is approved on
Experimental. Continue through ordinary implementation problems. Main, supported
releases, the Alpha fluid lock, existing consumer games/SDKs and independent
research remain untouched. No delegation is authorized. No release is authorized.

## Verified live branch state

Experimental/origin: 58e73071d7fd1e3d0b5b7d9129d5a70905e8e40c.
Native asset/scene/persistence integration is now committed and pushed there.
Earlier terrain foundation: 6932fe0b744b3ca16c44384ef0fa10e0cf282185.
Main/origin: 1094d64515604fe9d03fdab479efc7f880b112a8.
The native-source integration below is committed at58e73071. New authoring work
is uncommitted as detailed next. These checkpoints are not full completion.
Unrelated water screenshots and untracked structural/physics/research files were
present initially; never stage/clean/reset them. Stage terrain paths explicitly.

The desktop NVIDIA GTX970 works: 4096MiB, driver580.178.04, OpenGL4.6.
Restricted default commands see llvmpipe. Use require_escalated for native desktop
GPU tests/profiles. User explicitly corrected the unavailable-driver assumption.
Use j2 on the8GB host, one build at a time, no timing during compiler activity.

## Active continuation — authoring qualified, checkpoint pending

Native checkpoint58e73071 is pushed. Authoring continuation is implemented and
qualified, but still uncommitted. Current authoring source and evidence are scoped
under stage4-authoring. Do not include unrelated water screenshots or research.

New TerrainGeneration owns deterministic flat/highlands/island creation and real
grayscale16 heightmap import. New TerrainV2Creation owns the existing native file
browser, immutable generation/cook jobs, retained recipe/original heightmap and
undoable scene insertion. Explicit selected-ground replacement preserves the old
object's gravity/scripts/children and disables only its render/collision. Inspector
and Asset Browser requests route locally; optional ComponentEditor.requestAdd
supports asset-backed workflows without frame-loop changes. Source picker now
handles multiple sources and selects their actual owners; dirty drafts are guarded.
Four-layer texture/tint authoring, geometry-free paint, byte-bounded history and
shared V1/V2 protected-kit write validation are implemented.

Final build succeeded (/tmp/terrain-v2-authoring-final-build.log), no compiler
warnings. Fresh creation46, generation23, existing editor15, geometry416, world93,
LOD339, components35, cold-read3 and modern/portable NVIDIA render978each pass.
Original authoring/pipeline and planet-source10074 also pass. Relocated export
excavates, traverses, saves/quits and restores in a separate process. This is
engineering qualification, not human acceptance. Native editor capture also completes Play/Stop with identical authored state.
Inspected landscape source controls are ahead of optional empty script starters.

Thirty isolated alternating geometry-worker pairs reproduce exact geometry. At
513samples, former per-chunk full validation median727.85ms vs batch32.82ms;
129/257 raw distributions are retained. Excludes IO, merge, BVH and GPU, notFPS.
Earlier timing overlapped other qualification, so only isolated logs are reported.
Failed fresh-project/file-picker test setup/ID oracles are retained unchanged.

Next: inspect actual editor capture; scope/commit/push authoring. Then CONTINUE
remaining approved Terrain V2, including runtime surface sculpt/paint with real
persistence, physical residency, planetary excavation and cross-system integration.
No full Terrain V2 completion claim is warranted at this authoring checkpoint.

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
