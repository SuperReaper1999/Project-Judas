# Terrain V2 — live implementation state

## Authority and protected work

Operator approved all five Terrain V2 stages on Experimental. Continue routine
engineering independently; stop only for a genuine major blocker/product decision.
Main, supported releases, Advanced Fluid lock, existing projects and independent
research remain unchanged. No subagent delegation is authorized by this task.
Current accepted starting source was fc4b9a0a301d8988d135badcb08a51f4635df7a7.
Unrelated dirty water screenshots and many untracked historical experiments were
present initially; preserve them and stage only explicit terrain-owned paths.

## Committed checkpoint

6932fe0b744b3ca16c44384ef0fa10e0cf282185 is committed and pushed to Experimental.
It is the FIRST integrated slice, not completed Terrain V2. Main/origin main stay
1094d64515604fe9d03fdab479efc7f880b112a8. No release/unlock is authorized.

Stage1 includes sampled heightfields + sparse edit bands, genuine tunnel across
chunks, native collision cooking/publication, actual world persistence and
origin/region handling, module-owned editor brushes/undo/save, four material layers,
transactional authoring tools and owned projects/terrain_v2_workshop. Its evidence
is under stage1/.215 geometry checks,70 world/script checks,15 native editor,
220 original planet-world,10074 planet-terrain,58 legacy LivingWorld,35 Python,
API consistency, genuine desktop gameplay/save/quit/load and relocated standalone
passed. Raw failure experiments remain in attempts/.

IMPORTANT: default restricted commands see llvmpipe, but the DESKTOP HAS NVIDIA.
Use require_escalated for native GPU checks/benchmarks. Verified GTX970/4096MiB,
driver580.178.04, OpenGL4.6. User corrected the mistaken unavailable-driver claim.
Do not repeat that mistake. Build j2 on8GB RAM; one build at a time; no compiler
jobs during performance timing. Avoid ps args (it can expose large proxy text).

## Current uncommitted continuation

Stage2 rendering foundation implemented:
- terrain/TerrainLod.* immutable geometric-error hierarchy; multi-view/orthographic
  selection, rectangular exact coverage, finest-resolution volume seam ring,
  edge interpolation against coarser neighbours; no skirts closing cave openings.
- Renderer shared grid per resolution + lossless binary32 height samples in
  portable RGBA8 storage; identical shared GLSL deformation in colour/shadow.
  Ordinary draws reset terrain state; existing material slots/renderer reused.
- WorldPresentation prepares one camera-interest union before any pass. Physical
  collision is NOT retired by visual LOD.
- Batch extraction validates an immutable source once. Runtime preparation and
  author cooking consume the batch. Single-chunk API retains full validation.
- Collision fingerprints now hash the actual dirty chunk positions/topology;
  local edits no longer stringify/hash the entire source. Counts and signed-zero
  normalization are explicit. Cold-save qualification must be rerun after this.
- Source pages exceeding actual device texture limits are explicitly rejected
  before GPU allocation. Native paging is still remaining work, not implemented.

New tests: TerrainV2LodTests339 passed before final tiny changes, native GPU
TerrainV2RenderTests653 passed on modern4.6 and portable3.3. Actual GPU depths
agree with CPU reconstruction within7.2e-6m in identity/rotated/translated frames.
Initial ordinary-draw test failed because it read before normal batch flush;
corrected oracle flushes batch, production code unchanged for that correction.

Matched native benchmark (same1km terrain,2cameras,30 alternating pairs):
Far full-mesh CPU median11.632ms/p9511.702 vs shared-grid.0633/.0675ms;
near full5.858/6.347 vs shared.3636/.3705ms. Far selected4 patches, near43.
Geometry+height payload22,863,872 ->1,075,004 bytes (common material images excluded).
GPU elapsed scopes include CPU submission bubbles; do NOT present them as pure
GPU arithmetic timings. The small tunnel scene is GPU-neutral (~.70ms camera),
not a speedup. No RAM/physics speedup claimed from render-grid changes.
Raw logs /tmp/terrain-v2-render-{far,near}-prequal.log. No builds overlapped them.

## Current next actions

Build session48374 /tmp/terrain-v2-stage2-exact-build.log may still be running.
A final tiny GeometryFingerprint count edit occurred after its engine compile;
RECONFIGURE and incremental rebuild once before exact qualification.
New SeamTravel regression in TerrainV2WorldTests compares native motor crossing
20m of chunk seams/corners to one monolithic collider. Not run yet. Do NOT
weaken it if it reveals a real defect; investigate.

Next finish Stage2 rendering checkpoint qualification: new geometry/world/LOD
focused gates with fresh source provenance, genuine cold process again, native
render tests modern/portable, editor tests, author cook, actual desktop tunnel.
Preserve failed test-harness log /tmp/terrain-v2-render-native.log as an oracle
failure (normal draw was queued), not an engine defect. Record exact code/binary
hashes. Then scoped commit/push Experimental and CONTINUE approved remaining work.
No final task-completion claim after this checkpoint.

## Major remaining approved work (do not forget)

Current source is bounded flat terrain with <=1024 ALL-RESIDENT collision chunks.
Shared visual grids/LOD now exist, but there is NO native paged terrain asset,
separate physics/residency budgets or physical interest streaming yet. Runtime
source still arrives as JSON through a generated JS data module. This is the
next storage/residency issue; do not call it production large-world support.

PlanetTiles precision/LOD remains V1 and unchanged except generated-body queries.
Planetary excavation is NOT integrated (CreateLandscape rejects planet owners).
Need coherent patch/provider ownership, actual planetary tunnel/collision tests,
not a cave prop hiding an intact planet collider.

Fresh V2 source/project creation and high-bit-depth import UI, procedural landscape
presets, multi-source authoring, practical terrain PBR, runtime sculpt/paint APIs,
painting-only cook optimization and smooth LOD transitions remain. Current editor
discovers first source under Authoring, and picks with a merged preview mesh.

Navigation dirty publication, particle/fluid source-coordinate/solid-occupancy
integration, real regional unload/reload, native asset dependency/export receipts,
consumer-game copies and fresh SDK/export qualification remain. Do not overwrite
RPG installed SDK or existing game sources during this task.

Open known first-slice bounds: whole-empty ordinary model cook unsupported;
legacy V1 import only square pitch/16-cell alignment/4m tiling; runtime landscape
smooth brush unsupported (editor surface smoothing exists); volume sampling
currently follows height spacing. Undo snapshot memory currently count-bounded16.
These need correction or honest final boundaries, not hidden compatibility claims.

## Stage2 rendering foundation now qualified

Latest authoritative runner /tmp/terrain-v2-stage2-qualified passes geometry215,
world78, LOD339 with fresh provenance. Native GPU colour+shadow test978 per
modern4.6 and portable3.3 profile passes (shadow vs independent CPU mesh exact).
Existing runtime-render54, shader47, particle-collision37 and native editor15
pass. Genuine game save/quit/load ran on fresh stage2 copy and succeeds.
Shader test initially failed only its3 old canonical hashes; source-reviewed pins
updated with independent native controls, negative controls unchanged; failures
preserved under stage2/. The original unflushed-normal-draw oracle failure is
also preserved there. No production workaround for that test mistake.

Scope stage2 checkpoint now, then continue native terrain asset/residency work.
Do not call rendering benchmark total-game performance or claim all five stages.
Last build sessions completed. No currently running benchmark. Source identity
pins are in tests/ShaderLibraryTests.cpp and must be included in scoped commit.
