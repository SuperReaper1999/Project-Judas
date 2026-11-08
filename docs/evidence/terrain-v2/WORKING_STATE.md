# Terrain V2 — live engineering handoff

This is the current handoff, not a frozen history. Historical checkpoint reports
and raw evidence remain in the neighbouring directories and Git history. Read
live Git/source before acting; obsolete next-action notes were removed here.

## Active authority and protected state

Continue the approved Terrain V2 programme on **Experimental**. Main and supported
releases remain untouched; main/origin were1094d64515604fe9d03fdab479efc7f880b112a8.
No release/tag/channel change, Advanced Fluid unlock, RPG SDK replacement, unrelated
cleanup or delegation is authorized. Preserve existing games and independent
research. Unrelated modified water PNGs and many untracked structural/physics files
were present initially; never stage, reset or remove them.

User explicitly reminded us to re-ground after compaction and use the actual
NVIDIA desktop. GTX9704GB / Ryzen5 5600 / NVIDIA580.178.04 / nativeX11.
Restricted calls see llvmpipe; actual desktop tests need escalated execution.
`JUDAS_GL_PROFILE=portable` requests the qualified3.3 fallback; default selects4.6.
One build at a time, `-j2`,8GB RAM. No performance timings while compiling/testing.

## Current committed checkpoints

Latest committed/pushed engine is **4c71ad6ee7eb0b24a24c8194ba1bf358823945a8**,
including structured collision397b2467 and sensor preflight4c71ad6e below.
Material checkpoint **b5c1bc0dc5126aa2ecc3129a702efdabe8d7466c** added:
per-layer metre tiling and PBR roughness, optional shared triplanar normal detail,
real editor authoring, native/reference persistence and normal export dependency.
Defaults preserve old source encoding. Material shader/descriptor own cohesive
headers. Native callers must relink matching SDK headers/library.

Previous0c0502b4 records exact28984 SDK qualification. Previous28984e7a isolates
Scene-observer LOD selection from game transition history. Previous4838c503 moves
complete brush preview preparation/merge to a worker and adds exact cached picking.
Previous03927f71 adds bounded complementary LOD cross-fades. All are pushed.

Native material validation:86new controls,978old height/shadow and98822transition
controls EACH modern/portable; actual editor15/creation46/planetEditor23each.
Native asset33/component38; world93/300supportedsteps with zero monolithic
trajectory difference; shader47/runtimeRenderer54. Python22build and36docs/package.
Actual relocated flat/planetary games restore8volumechunks/2surfaceDabs/navready/
emptyqueue; flat also restores one reliable particle contact/consumed droplet.
Evidence: material-detail/REVIEW.md and hashes. No operator acceptance inferred.

Material rendering is not free: same56draws/28672triangles, new defaultGPU~.57ms,
normal enabled~.65ms in the two-view1km1280x800 fixture. Warmed oldGPU~.49ms;
default shader overhead~.08ms and enabled detail another~.08ms are explicit.
CPU~.39–.40ms. Normal texture87380B, not per-chunk textures. GPUelapsed includes
submission bubbles and is not whole-game FPS. First striped demo normal was
replaced by subtler noise in disposable consumer copies only.

Remote4c71ad6e docs run42488850106SUCCESS; engine run42488850234INPROGRESS
at last observation. Verify live before claiming both configurations passed.
Earlier runs may have been superseded by new pushes; cancellation is not a failed
engine check. Main/origin1094d645 unchanged. No operator terrain acceptance.

## Structured collision checkpoint397b246759027cf14a71afccbb50241e18dbf603 (pushed)

Implemented **TerrainCollision.h/.cpp**, small internal collision/CollisionBuild.h
interface and an exact specialized grid cooker. TerrainPreparation uses it only
for ordinary nonvolumetric heightfields. It proves a strictly monotone tensor
X/Z grid and the full index pattern, preserves first-use vertex/feature ordering
and the generic arithmetic, then uses the original deterministic BVH builder.
Caves, projected planets and failed proofs retain strict CookCollision. No physics
response/CCD/integration changes or physical LOD. src/CollisionAsset.cpp only adds
the internal BVH wrapper; its existing generic cook is unchanged.

New TerrainV2CollisionTests338 controls cover64grids, including non-power-of-two
cell counts and fractional spacing: complete serialized collision assets are
BYTE IDENTICAL to the generic cook; SAT/warnings also equal. Eight negative cases
retain the old destination. World93/300exactsteps, preparation26, components38,
navReuse67, genericCollision1305 and water/particle/source-solid85 all pass.
Regional1km streaming69write/13coldread; planetary components40/9 and regions37/14
pass in separate processes. Editor15/creation46/planetEditor23 EACH modern/portable
pass after relinking. Actual relocated flat/planetary material-enabled games pass.

Matched real-game fixture:1km,1025x1025samples,1m spacing,32cellchunks=1024bodies.
Three alternating old/new runs use identical source/scripts/resolution/fixedstep.
Median scene construction2.143s ->1.035s (~51.7% faster); warmed peakRSS roughly
529000 ->505000KiB (~24MiB less). First old run586604KiB has an extra driver/process
peak: do NOT attribute its whole82MiB difference to the cook. Steady simulation
~.37–.39ms unchanged. This is terrain-chunk preparation, not the dense-body pile.
Frozen binary/raw rows/input hashes in .cache/terrain-v2/residency/;
collected evidence structured-collision/REVIEW.md. Final BVH-wrapper edit after
validation changed formatting only, with identical tokens/behaviour.

The first large fixture rounded weights independently and correctly failed the
unchanged1e-4 sum check. Corrected integer weight units sum to10000. Failed generator,
input hash and log remain. Do not relax that validator or repeat the failed setup.

All native build/test/benchmark/SDK processes have EXITED. Collision optimization
and its evidence are committed/pushed. Sensor correction4c71ad6e is also pushed. Do not stage
the unrelated water screenshots or untracked research. A genuine terrain sensor defect was reproduced and corrected separately: native
placement and runtime creation accepted a sensor seed but generated blocking
triangle surfaces that MK2 cannot mark as sensors. New tests FAIL on397b2467 and
pass with explicit preflight rejection. Ordinary sensor seeds remain intact.
Native component39/world96 (all old controls retained), planet40/9, creation46
EACH modern/portable and a real relocated/cold material+particle game pass.
Evidence sensor-contract/. Those four source/test files and evidence are checkpointed separately at4c71ad6e. No unsupported sensor implementation was invented.
Final matching17-tool SDK4c71ad6e is QUALIFIED below. No RPG SDK replacement.


## Packaged SDKs and editable projects

Final exact **4c71ad6e** SDK .cache/terrain-v2/sdk-native-preview-4c71ad6e QUALIFIED:
17tool probes/842files,8standard operations, both SDK-only material-enabled games
with relocated exports/cold saves and particle interaction. It includes the
structured cook and sensor correction. Evidence sdk-final-qualification/.
Previous exactb5c1bc0d/28984 SDKs remain, but do not include later source changes.
All are hostGNU15.2/GLIBC2.43 local builds, not Ubuntu24.04 distribution or fresh
Windows/Android/browser certification. SourceDirty=true and18preexisting untracked
source inputs are recorded honestly. The RPG still selects local9f022cf1.

The consolidated implementation/benchmarks/support bounds/human checklist are in
**docs/TERRAIN_V2_QUALIFICATION.md**. This handoff and final SDK evidence accompany a docs-only descendant checkpoint;
verify its enclosing HEAD live. All functional work is committed/pushed. Main, fluid lock,
installed games and unrelated modified/untracked work remain untouched.

Editable owned projects:
- projects/terrain_v2_workshop/terrain_v2_workshop.judasproj
- projects/terrain_v2_planet_workshop/terrain_v2_planet_workshop.judasproj

WASD/mouse/Space; Tprogressive tunnel;1/2/3/4dig/raise/lower/paint;5layer;
clickbrush/rightadd; F6/F7save/load; Rreset; Escpause; planetOorigin shift.
World→Terrain tools owns source creation, selection and brushes/material controls.
Scene Save differs from Save and update playable terrain; stale products blockPlay.

Do NOT overwrite old authored native files/saves automatically. Disposable material
copies are retained in .cache/terrain-v2/material-detail/{flat,planet}-editable.
Latest outside-checkout cases are /tmp/terrain-cook-{flat,planet}-game-20261108.
The driver renames sources to **parked editable** to prove runtime relocation;
copy from that directory, not from a nonexistent `editable`. Saves stay user-owned.

## Remaining approved boundaries / not complete

The complete five-stage Terrain V2 programme is not yet finished. All physical
chunks remain resident with their native source region; arrays and GPU images
remain whole-source. Native pages/checksums and region memory budgets are real,
but independent per-chunk physical interest and data demand paging remain open.
Do not evict by camera visibility: physics queries/particles/navigation can need
unseen terrain. Current 1024chunks/source,8owners and normal regional budgets stay.

Mapped planetary patches are static, interior cube-face bounded, nonoverlapping
and full render detail. No cross-cube-face excavation, moving patch owners or
runtime volumetric smoothing; surface smoothing works. Four albedo layers plus
shared detail normal are real; virtual textures/per-layer normal stacks are not.
Preserve these support limits honestly while continuing work.

Successful foundations: exact cross-chunk tunnels, canonical float64 construction,
shared GPU height grids/camera-union LOD/stitching, native Terrain assets/export,
asset-reference sparse saves, editor presets/16bitheightmaps/undo, shared source-solid
water occupancy, logical particle receivers/local collision coverage, incremental
source-owned navigation, planetary cutouts/precision/origin shifts and regional
cold persistence. Prior evidence directories remain authoritative for each slice.

## Reliable commands and common traps

- SDK verification is positional: `python3 tools/package_sdk.py verify SDKDIR`.
- SDK staging: --bin-dir build --build-info build/judas-build-info-Release.json
  --output FRESH_SDK --probe-tools. Rebuild all17tools after an engine checkpoint.
- SDK consumer: scripts/qualify_sdk.py --sdk SDKDIR --work /tmp/FRESH.
- Packaged terrain driver: SDKDIR/tools/validation/qualify_terrain_v2.py --build SDKDIR
  --output /tmp/FRESH [--material-detail] [--planet OR --particle-region]. Output
  must be fresh/outside checkout. Never weaken this guard.
- CMake new targets require configuration before requesting them with Makefiles.
- Runtime renderer target is judas_runtime_render_tests, not runtime_renderer_tests.
- Build organization Python module is tests.test_cmake_test_targets.
- Check sources for actual CLI/target names before guessing.
- Raw logs may contain whitespace; do not alter evidence to satisfy diff checks.
- Keep persistent logs in.cache and copy evidence before reboot; /tmp can disappear.
- Do not start a duplicate build if a session ID expires; inspect logs/processes.
