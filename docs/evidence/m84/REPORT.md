# M84 volumetric terrain — Linux development checkpoint

Starting accepted Experimental/main: `18413e3bde8c61f79339a37fd40ee32400e110d1`.
14 October 2026. Human M84 acceptance pending; Windows unvalidated/nonblocking.

## Scope and architecture

Practical **authoring/edit-and-cook** volumetric terrain. A bounded project-owned scalar
solid supports add/remove/smooth strokes, enclosed voids, tunnels and overhangs. Consistent
six-tetrahedra cells, shared edge intersections and outward winding produce normal model
archives and cooked static triangle collision. Reuse: M75 source/draft/publication patterns,
M66 model archive, existing collision/resource pipeline, M53/M76 navigation/dependencies,
normal scene/prefab, save, region and M38 export ownership. No new runtime asset type,
voxel simulator, gravity implementation, renderer-owned collision or gameplay C++.
No runtime JudasJS brush binding is claimed. API types/inventory remain unchanged and checked.

Authoring paths: `src/authoring/VolumeTerrain.*`, `src/editor/VolumeTerrainTools.*`,
`judas_volume_author`; current project `projects/volume_terrain_lab`.
The field's units are local metres with negative solid/positive air; it is an approximate
editing field rather than an exact global distance function. Boolean sphere composition
uses a bounded region plus a two-cell-diagonal halo; smooth uses six neighbours. Gestures
schedule by 3D arc length and retain bounded undo. Metadata and density comparison avoid
serializing the entire grid on every release. Background immutable previews/cooks, expected
output checks and rollback publication preserve editor responsiveness/ownership.

Minimum 3/4-sample creation was corrected to generate a real nonempty solid. The editor
caps new cubic solids at 37 samples, verified against existing collision budgets. Parsed
sources allow 3..129 per axis, <=300000 samples; <=200000 vertices/65536 triangles per cook.
Over-budget, empty, stale, cancelled and foreign-owned output failures are explicit.

## Requirement → meaningful evidence

| Contract | Positive/negative proof |
|---|---|
| Excavation changes physics | Initial solid blocks through-ray; carved tunnel permits it; ceiling/floor/pocket walls are actual collision. |
| Topology | Closed face adjacency for carved volume, enclosed pocket and exact-zero sampled sphere; deterministic extraction. |
| Add/remove/smooth | Source sign changes, undo/redo/cancel exact arrays, smoothing, invalid brush/no mutation and event-rate-independent scheduled gestures. |
| Arbitrary frame | Unit character/body/queries under identity and transported quaternion; same application terrain/character/gravity/nav under .8 rad about normalized(1,2,3). |
| Actual body interaction | Dynamic sphere has real terrain contact impulses (it rolls); public JS throws a normal rigid prefab inside the cave and contact is observed. |
| Authoring UI | Real activation-button and native viewport events; independently changed add gesture before Escape; changed focus-ended gesture; exact undo; GPU preview matches extraction. |
| Navigation | Actual underground Detour route; real geometry edits invalidate bake; explicit rebake and saved-scene batch dependencies. |
| Source ownership | Exact reload, invalid counts/nonfinite/border rejected, cancellation retains products, concurrent file change rejected, foreign source cannot overwrite another registered destination. |
| Persistence | Fresh-process physical position/orientation/velocities and collider reference compare exactly before first advancement, then actual terrain support continues for120steps. |
| Streaming | Suspend destroys old actor; new identity resolves same edited collision; direct floor ray succeeds after republish; normal reload works. |
| Prefab | Existing Create/Instantiate/ResolvePrefab preserves normal immutable collision references. |
| Edited export | Editor-excavated pocket changes actual exported ray distance; script-thrown body, streaming and reload pass on that export. |
| Moved standalone | Unmodified read-only package launches from unrelated /tmp and closes its own window normally. |

69 final volume checks PASS (`volume-candidate-closure.txt`). Existing heightfield authoring
89 checks /0failures and terrain pipeline15 checks PASS. Native cave/rotated, write/read,
stream/reload and final lit scenes PASS. Editor actual source/GPU/Play-Stop follow-up has
17 individual PASS checks and0failures. Package follow-up: edited18checks, rotated17checks,
moved-package normal startup all PASS. API drift/TypeScript5.9.3:32exports,252operations,
16callbacks,42examples,4negative drift controls,3result-shape checks PASS; this M84 check
uses source/type enumeration, not a newly claimed live API enumeration. No binding changed.
Final Release target build logs contain no warnings. No whole production-suite marathon.

## Measured Linux reference

Intel Core i7-4790,8logical CPUs. Native Linux graphics/application, fixed60Hz; no parallel
CPU-heavy workload was intentionally used for acceptance. Values are scoped measurements,
not a universal60FPS guarantee or Windows/GPU-hardware certification.

| Final representative scene | Fixed p95 / max | Frame work p95 | Thrown-body fixed p95 / max |
|---|---|---|---|
| Lit cave |4.055 /5.698ms|5.286ms|3.785 /6.091ms|
| Lit rotated cave |9.740 /10.826ms|10.552ms|9.534 /11.140ms|
| Exported rotated cave |10.247 /11.780ms|11.054ms|9.878 /11.537ms|
| Editor-edited exported cave |4.803 /6.671ms|6.519ms|4.368 /5.951ms|

Seven normal physics registrations in the main scene (six ordinary bodies plus motor
query volume), eight with the thrown prefab; three ordinary cave lights. The retained annex
adds four ordinary bodies when active. No runtime density allocation/remeshing is measured.
The main source has42025 scalar samples; the numerical unit source27225/39048 triangles
extracts in23.394ms and cooks in422.717ms. Current lab cook/nav timings are in generation
and navigation receipts (roughly600ms volume publication and356–436ms navigation here).
Cooking includes archive/collision preparation and publication and runs off the input thread.

Small brush benchmark, same grid/2m path/radius.1m:65³/274625samples initially95.979ms stroke
and25.435ms release; final local-stencil/cached-dirty implementation1.541ms stroke/.072ms
release. Original whole-grid implementation and receipts preserved. This compares authoring
work, not different game scenes. Local distance values outside the bounded halo now remain
unchanged by design; do not call it exact global SDF reinitialization. Full grid validation
still runs at public/source boundaries. The earlier cluttered scene and final clear-route
performance are different contact workloads and are not reported as matched speedups.

## Failures preserved, not rewritten

- First test build used a nonexistent Shape::TriangleMesh helper; corrected harness to normal Shape::Cooked.
- Preview-widget build used disabled ImVec2 arithmetic; corrected explicit coordinates.
- Initial motor test's accelerating route drove out of the finite volume; retained unchanged as a negative control, with a separate in-bounds positive route.
- Initial sphere test mislabeled rolling as failure to settle; actual contact/position is now checked, and original .5m/s threshold/1.505m/s result remains in fixture clarifications. No existing/historical threshold changed.
- Missing world-meta source field and invented gravity-region priority property were generator errors. Corrected normal metadata and spatial authoring; no gravity resolver/motor change.
- Original cluttered rotated case: support sweep hits crate20's side at zero distance, normal approximately[-.773,0,-.635], while gravity is correct and actual floor remains about.926m below the centre. **Existing motor support-selection limitation remains open.** Exact scenes and probe/motion traces retained. Initial props are moved off the walk route; separate real thrown-body contact remains required. No filtering bypass or controller retuning.
- Automation profiler overlay intercepted later test viewport clicks; follow-up disables that overlay only for the M84 automation, like the existing M75 hook, and proves genuinely changed add/cancel/focus-loss gestures.
- First edited export omitted the JS-only prefab because the generator cleared runtimeAssets. Corrected explicit project declaration; exporter and runtime unchanged. Actual exported spawning/contact now passes.

`FIXTURE_CLARIFICATIONS.json`, `fixtures/`, original build/log outputs and brush-before
source preserve the failures and exact conditions. Research/history outside this new
M84 evidence directory is untouched.

## Deliverable and limitations

Current project: `projects/volume_terrain_lab/volume_terrain_lab.judasproj`.
Current standalone: `/tmp/judas-m84-volume-package-final/judas` (9assets,3scenes,62364137bytes).
Moved read-only copy: `/tmp/Judas_M60_M84VolumeAcceptedCandidate20261014/judas`.
Edited workflow proof export: `/tmp/judas-m84-edited-proof-package-final/judas`.
The editable project and generic tools are tracked; executables/packages/builds are not.

See `docs/M84_VOLUMETRIC_TERRAIN.md` for controls, creation/cooking, numerical units,
bounds and one consolidated human checklist. Explicit limits: static finite volumes;
no seamless stitching/LOD/octree/planet-scale store; no live gameplay excavation API,
soil simulation, physical excavation debris or material-mass ledger; source spacing limits
thin features; existing triangle collision requires rigid unit scale. Coarse/overlapping
contact and the recorded motor-side masking issue are not certified away.

Windows availability query finds win10 shut off. No M84 native Windows run or VM/config
change was made; Windows validation remains outstanding and nonblocking.
M84 human review remains pending. Only Experimental is checkpointed; main stays at the
accepted M83 hash. No tag, M85 work, protected history/fluid/research changes.
