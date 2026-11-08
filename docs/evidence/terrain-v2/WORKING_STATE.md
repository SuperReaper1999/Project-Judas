# Terrain V2 — current engineering handoff

Authority: the complete five-stage Terrain V2 implementation is approved on
Experimental. Continue through ordinary implementation problems. Main, supported
releases, the Alpha fluid lock, existing consumer games/SDKs and independent
research remain untouched. No delegation is authorized. No release is authorized.

## Verified branch checkpoints (verify live Git before changes)

Planetary core739805be, navigation handoff6315daad, normal-project geographic
integration f4e8335b and save-budget correction2e8bebca are pushed. The SDK,
shared packaging inventory and documentation qualification below accompany this
next checkpoint. This handoff deliberately does not guess its own enclosing
commit hash. Verify live Git rather than treating a parent hash as HEAD.
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

## Current planetary integration (qualified; whole Terrain V2 NOT complete)

Source3 stores chart/rim/provider identity; native-envelope2 rejects old flat-only
readers explicitly. Ordinary Source2/native1 behavior is preserved. Meshing,
solid occupancy and world-metre brushes use the same radial map. terrain.planet
binds a root native owner to its absolute planet. Protected rims match base render
and collision grids. Regional reservations prevent overlaps while private, retain
old planetary ground before publication and restore it on hide/remove. Source
owners cannot be moved through public OR private restoration paths.

Real component40/cold9 and streaming37/cold14 pass. During integration the isolated
component validator renumbered a real external planet reference into a self-ID;
fixed without dropping endpoint validation. Cold streaming exposed an actual
absolute-descriptor translation in retained restoration; fixed. Negative control
proved private state restoration could detach a patch; now rejects. Failed runs
and setup mistakes are preserved separately from these genuine integration fixes.

Normal editor creation: choose Planetary patch in the existing landscape creator.
The existing terrain CLI gains v2-planet; v2-native handles projected previews.
Source preview and PlanetTiles mask share editor scene coverage through the small
TerrainPlanetPreview module. Source-owned navigation uses local mapped bounds.
Editor creation/excavate/publish/navigation/Play/cutout23 checks pass on actual
GTX970 OpenGL4.6 and portable3.3. Existing ordinary creation46 also passes.

Actual new projects/terrain_v2_planet_workshop is separate from existing games.
The normal relocated export sculpts/paints, excavates8chunks, walks through the
hill and cold-restores ready/navready,queue0,2surfaceDabs,z=-10.380639. Separate
live origin control and another cold process restore origin(0,1024,0), with edits
intact. This is automated engineering evidence, NOT human acceptance.

Actual-frame benchmark (180 samples each, 1280x800, GTX970, Ryzen5600, Release):
intact simulation/scripts/publication .4093ms median/.5187p95/.5944worst;
CPU render submission2.315/2.525/3.251ms; GPU elapsed1.289/1.406/1.586ms.
Excavated .8970/1.0473/1.1708ms; CPU2.327/2.620/3.014ms;
GPU1.363/1.453/1.728ms. This is one bounded game/camera workload, not a
before/after optimization claim or generic FPS. GPU elapsed includes CPU bubbles;
synchronous query timing does not represent a normally overlapping frame.
The ordinary script harness renders screenshots only; its GPU timestamps never
resolved, so it is NOT used for rendering/FPS claims. The first actual-frame
benchmark omitted AdvanceRuntimeTerrain and correctly timed out; preserved and
corrected by calling the normal outer-boundary API, not changing terrain policy.

Broader current checks: world93/300 exact seam steps, flat component35, surface59,
interaction85, preparation25, streaming69, PlanetWorld220, PlanetTerrain10074,
scene fingerprint140, generation23, export18, registration11(2existing skips),
JudasJS API contract PASS. Earlier core chart1926/GPU343each/no-cutout exact and
flat geometry416/assets32 remain relevant; no physics solver changes. Final editor
session and fresh current export runs are recorded in planetary-integration/.

Bounds: interior cube-face patches only; no overlaps or movable prefabs. Current
patch size32..512cells must satisfy existing visual/physical rim budgets. Provider
changes reject stale patches. All active native collision chunks remain resident,
max1024; mapped planet patches render full detail, surrounding planet retainsLOD.
This does not implement independent chunk physical interest or demand paging.

Geographic integration is pushed at f4e8335b392ed8c18aa3410af34b1ba5f68a8e8e.
The subsequent save-budget correction verifies the mismatch: 5,271,385byte
runtime source captured but ordinary cold restore rejected by old2MiB cap. One
shared8MiB policy now matches SaveArchive/capture/direct/world/regional paths;
30write/18coldread plus component/streaming/legacy58 controls pass. Oversize
negative preserves exact live ray/body count. Evidence save-budget/. Check live
Git for its checkpoint state, never assume a document's parent hash is current.

Save-budget correction is pushed at2e8bebcadd3ad43e0b781be45f0c77d8ea3fb88e.
Fresh local SDK .cache/terrain-v2/sdk-native-preview-2e8bebca-final verifies17tools,
8standard editor/project/tool/moved-export operations, and SDK-contained flat and
planetary native-cook/sculpt/paint/tunnel/cold-reload exports. It is host-native,
GLIBC2.43, NOT Ubuntu24.04-qualified or published. SourceDirty and18untracked
provenance remain honest; don't erase research to make them disappear. Installed
RPG SDK remains local9f022cf1. Only our redundant first staging copy was removed.

SDK/source inventory qualification exposed4omitted learning-project assertions:
old Particles/Graffiti and new terrain samples. package_source now derives the
learning roots from the SDK inventory, using the requested checkout's COMMITTED
inventory for archives. Source-only disposable-Git and unrelated-game/save
negative controls pass. Native SDK now includes both Workshops and its own terrain
qualification driver. Package/CI-route/registration36pass(2existing skips).
Git blob-reader pipes now close explicitly, removing observedResourceWarnings.
Evidence sdk-qualification/. The SDK/shared-inventory work is qualified for this
checkpoint; verify live Git for commit state. No build or test was running when
the final qualification logs were collected.

NEXT substantive boundaries: independent per-chunk physical interest/data paging
and broader material/large-source interaction coverage remain open, not magically
solved by the integrated Workshop. Current supported residency unit is a native
source/region, max1024colliders per source; no demand-paging or whole-programme
completion claim. Retain query correctness rather than evicting unseen support.
Do not replace installed RPG SDK or existing game assets.

Runtime memory correction qualified after8ca4ee17: ordinary published chunks now
release CPU draw streams unused by shared-grid rendering, while retaining the
exact cooked collision, navigation inputs, generated triangle metric and unique
cave/planetary meshes. Matched1km initial payload76.13->53.27MB, excavated
77.02->54.25MB; full-nav80.10->57.32MB. No peak-RSS or speedup claim. Original
negative12checks becomes26pass; 300physical seam steps still exact; ordinary and
planetary/regional/large-cold/realGPU/relocated-game controls pass. Evidence
runtime-memory/. Pushed asd22512b8. Verify live Git rather than assuming HEAD.

Incremental navigation is now implemented and qualified: native ownership derives
BOTH old/new changed bounds; the small NavigationTileReuse module includes full
filter borders, retains unchanged/empty layers and falls back when bake settings
or profile changes. SAME kilometre source compares byte-identically with a full
bake:16tiles rebuilt/1008reused.67new controls plus frozen raster59/navigation92,
native nav73, preparation26, terrain/world/source/streaming/water/particle and
cold-save controls pass. Actual editor23eachGL4.6/3.3 and both relocated games
pass; API contract/registration pass. All17tools build.10matched runs per side:
complete excavation median295.345->75.691ms, sampled worst303.981->82.351ms.
No FPS/startup/memory claim; full geometry bins and private Detour cache still
prepare. Evidence navigation-tile-reuse/. Verify Git for checkpoint state.

## Completed native navigation cache handoff

Functional cache handoff is implemented on top of1b593d8f. The SAME Detour
Surface builds privately through NavigationSystem::PrepareSurface on the existing
preparation worker, then move-consumes once at normal publication. No second
navigation system or public JS change. Native navigation73/world93/handoff25/nav92
PASS, including actual worker thread, identity mismatch, double consume, partial
cancellation, invalid layer, private staging and live state retention.

Matched warm-file full1km source1024colliders/full1024m nav1m cells:10runs each,
order before/after then after/before in5-run groups. Publication median26.3803ms
→.4948035ms, sampled worst28.1212→.615662ms. Complete edit297.43→298.04ms:
work moved off owner thread, NOT eliminated. Initial publication28.19→2.23ms.
PeakRSS230880–231276KiB before,231512–231980after; no memory reduction claim.
Evidence navigation-cache/. Frozen build83156 EXITED; no build/test running.
Cold73write/7read, components35,surface59,interactions85,streaming69,raster59,
saveExclusions77,export18,existing actual navgame26 PASS. Desktop NVIDIA editor
authoring/undo/PlayStop authoredIDENTICAL and relocatedWorkshop PASS; restored
navready,8volumechunks,2surfacedabs,queue0,z=-10.6087. Screenshots inspected.
Committed/pushed as6315daad; source/evidence review completed.

Then continue physical residency/planetary work; fullTerrainV2 is incomplete.
Current bounded native solid/nav pipeline does NOT make planetary excavation.

## Completed regional streaming and navigation foundations

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

Source-owned native navigation is qualified and checkpoint-ready after4905918a.
No build/test is running. Evidence: navigation/. Query actual Git for checkpoint.
Native TerrainNavigation.* prepares from shared cooked chunks; existing NavSurface
settings (empty asset) configure an optional live source-owned surface. Scene,
region worker, cold prepare, exporter, CLI/dependency tooling and Inspector support
it. Generic bakes now collect native geometry rather than identityBox.

Native64/cold7, world93/300exactseam, components35/surface59/handoff25/interactions85,
nav92/actualgame26, saveExclusions77/export18, authoring46, GPU978modern+978portable
PASS. Real relocated Workshop with navigation also passes. A genuine project bug
found by it is repaired: queued brushes now persist, dequeue only after acceptance,
and resume after cold save. Saved8pending/busy, restored7after accepted in-flight
dab, completed0, traversed and cold-restoredz=-10.61. Original geometry/position
acceptance is retained, with explicit observed queue completion before traversal.

Workload1km513²/1024colliders, OPTIONAL128m navcoverage0.5m (NOTfullkm routing):
5warm-file off/on diagnostic medians: complete sculpt22.90/40.74ms, excavation
24.60/46.39ms, paint3.57/3.64ms, edited publication.196/.952ms. No compilers during
measurement; grouped5samples aren't percentile/speedup/FPS evidence. Full-region
nav rasterization still scans geometry per tile and needs profiling before
claiming scalable kilometre navigation. Don't confuse128m coverage with1km source.

Whole-source removal publishes empty nav. Agent replans, paint reuse, rigid pose,
origin, disable/reenable, prepared settings mismatch and physical/cave-floor
layering are tested. Static independent navbakes explicitly block native runtime
geometry changes unless source layer excluded; props use obstacles. Native
preparation16Mcolumns/existing4096tiles/Recast13bit height limits reject rather
than dropping coverage. Height samples should be landscape-relative, with
placement via transform. Current native coverage rebuilds whole; no incremental
tile preparation yet. Generic inspector static bake freshness also recollects
geometry per frame; avoid claiming that rare path optimized without profiling.

Navigation scaling after34b6ab5b is qualified and checkpoint-ready. No build/test
running. Evidence navigation-raster/. DT_POLYREF64 and exact triangle bins fix
full-km startup capacity and repeated tile raster rejection. Existing compressed
nav resources remain readable (actual SpringRange26). All native consumers must
rebuild together: internal Detour ABI changed, not save/resource formats.

Index59/full-reference17/native64+cold7/world93+exact300steps/components35/surface59/
handoff25/interactions85/nav92/actualgame26/saveExclusions77/export18 and genuinely
relocatedWorkshop PASS. Five alternating full-km524288triangle bake pairs: median
2277.29→239.448ms, every975compressed layer bytes EXACT. Before runtime with same
full bounds failed native cache init (975layers*4reserves,2048polys,only9saltbits).
Before executable/source retained at .cache/terrain-v2/navigation-raster-reference.
No speedup from the failed runtime is claimed. First large oracle fixture wrongly
kept small vertical bounds; corrected to the kilometre -256..256, failure preserved.

Full1km navigation/1m cells now actually runs1,024physical chunks: median startup
704.7ms, complete sculpt292.79ms, excavation296.74ms, publication26.23ms. That last
owner-thread cache construction is a real remaining hitch. Partial128m numbers
above aren't full-km routing claims. Coverage is configured, not a guarantee that
all physical points pass Recast slope/climb/voxel constraints. Five samples are
engineering diagnostics, not broad percentile distributions.

That26ms cache construction is addressed by the current handoff above. The
independent raster/64bit checkpoint is committed/pushed at1b593d8f; its measured
cost remains historical baseline evidence. Physical interest/planetary work remains.

Physical-interest/residency limits, planetary excavation/provider masking and
final game/SDK/visual qualification remain; TerrainV2 is NOT complete. Main,
fluid lock and existing installedSDKs/projects/research untouched.

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
2. COMPLETED: fresh source/project creation,16bitheightmaps/presets,multisource
selection,bytebounded undo,runtime surface sculpt/paint and sparse raster saves.
3. COMPLETED bounded planetary excavation/provider override: actual base render
and collision removal, precision/LOD, editor workflow, origin shifts and cold
regional persistence. Interior face patches only, not whole-planet volume paging.
4. COMPLETED bounded controls: source-owned navigation+atomic dirty publication,
source-solid water occupancy,particle interactions/logical source IDs,real regional
travel and cold persistence. Remaining large-source particle capture budgets and
whole-coverage navigation rebuild limits are explicit above.
5. COMPLETED fresh native local SDK/export and editable flat/planetary consumer
copies. Broader materials/visual quality, temporalLOD transition and additional
platform coverage remain open. Do NOT replace installedRPG SDK or assets.

Known bounds: V1 import squarepitch/16cell/4mtiling; whole-empty ordinary editor
model unsupported (runtime emptychunks valid); runtime V2 smoothvolume unsupported;
volume spacing follows source heightspacing; editor uses a multi-source picker and byte-bounded history; large merged mesh
picking still needs scaling work. Fix where practical or document honestly. Never hide deferred work behind another green demonstration.
