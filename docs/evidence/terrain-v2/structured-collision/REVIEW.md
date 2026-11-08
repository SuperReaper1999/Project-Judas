# Exact structured heightfield collision preparation

The real game profiling found that 1,024 terrain chunks were inexpensive in steady
play, but expensive to construct. A one-kilometre, 1,025²-sample source at one-metre
spacing took about 2.14 seconds to build its scene and around 518 MiB peak process
memory. These are terrain chunks, not 1,024 interacting dynamic bodies.

The generic collision cooker repeatedly establishes exact position welding, edge
adjacency and absence of boundary T junctions. Those general searches are needed
for arbitrary geometry. For a proven complete tensor-product heightfield, the same
facts follow directly from the strictly monotone X/Z lattice and full triangle
pattern. `TerrainCollision.cpp` performs that proof, retains the generic cook's
first-use vertex/face order and arithmetic, and constructs the identical adjacency
and crease flags. It calls the existing deterministic BVH builder through one
small internal interface. It does not introduce another broadphase, physics
solver, approximate height query, collision LOD or altered contact response.

Runtime preparation attempts this specialization only for ordinary, unedited
heightfield chunks. Volumetric/cave chunks and projected planetary geometry retain
the generic cook. Failure of the proof also falls back to that strict cooker.
Invalid geometry is not silently repaired. The output is published only after
complete success, using existing staging and revision rules.

## Correctness evidence

- `TerrainV2CollisionTests`:338 checks. Sixty-four complete grids cover cell counts
  4,5,8,16,24,32,63,64 and spacing0.125,0.3,2,128, with irregular slopes/creases
  and two different chunk anchors. Generic and structured serialized collision
  files match **byte for byte**. These include every vertex, source feature,
  triangle normal, adjacency, active edge, BVH node/order, bounds and mass metadata.
  Derived SAT arrays and warning lists are compared too. No approximate tolerance.
- Eight malformed/noncanonical cases reject atomically and retain the destination:
  invalid indices, collapsed coordinates, non-tensor vertices, nonfinite heights,
  incomplete triangles, source identities, unsupported cell count and partial
  primitive. Generic arbitrary-mesh validation remains unchanged.
- Original world93 checks pass; 300 supported seam steps retain zero difference
  against independent monolithic collision. Generic collision1,305 checks pass.
- Preparation26, component38, navigation reuse67 pass. Ordinary one-kilometre
  regional streaming69write/13cold-read, geographic components40/9 and geographic
  regions37/14 pass in genuine separate processes. Navigation compressed layers,
  inactive edit state and origin placement retain their original checks.
- Actual editor15/creation46/geographic editor23 pass on both modern and portable
  NVIDIA contexts. Existing water/particle/source-solid integration85 passes.
- Both relocated standalone games pass native cooking, export, real tunnel
  excavation/traversal and cold restore, including material detail and the flat
  game's reliable particle contact. See corresponding game logs/results.

## Matched real-game measurements

The same editable game, source bytes, scripts, build configuration, camera,
resolution1280x800 and fixed simulation settings run through the previous frozen
binary and the integrated binary. Three paired invocations alternate order. Each
invocation contains180 intact and180 excavated measured frames after warmup.
No compiler or other test runs overlapped. Raw commands, source/binary SHA256,
all frame rows and `/usr/bin/time -v` are retained.

| Metric | Previous three runs | Structured three runs |
| --- | --- | --- |
| Complete scene construction |2202.19 /2142.73 /2143.41ms|1043.64 /1035.29 /1025.51ms|
| Excavated simulation median |0.375 /0.377 /0.383ms|0.379 /0.371 /0.374ms|
| Peak process RSS |586604 /528784 /529080KiB|504652 /504936 /504696KiB|

Median scene construction falls about51.7%. The warmed peak-memory samples improve
by roughly24MiB. The first old run has an extra process/driver allocation peak;
**do not attribute its entire82MiB difference to this optimization**. Exact vertex
reservation removes a known source of spare per-chunk capacity, but RSS includes
CPU, driver and temporary preparation allocations. It is not a precise terrain
payload counter. Steady simulation is effectively unchanged; no FPS/solver speedup
is claimed. Complete median/p95/worst CPU and GPU elapsed data are in
`matched-summary.json`; GPU elapsed includes submission bubbles.

The isolated output-equivalence test reports diagnostic cook timings as well:
32-cell chunks measured roughly1.67ms generic versus0.58ms structured in the initial
run. They are small, fixed-order samples, not a separate rigorous performance
claim. The real-game paired result is the adoption criterion.

## Scope and retained failures

No public physics API, save format, collision feature identity, shader or physical
resolution changes. The native source/region residency policy remains: every
physical chunk is present while its region is active. Fine chunk/data paging is
still open, not secretly accomplished by removing distant collision.

The first large fixture independently rounded each material weight to four decimal
places; some sums exceeded the existing1e-4 validation tolerance. The unchanged
validator rejected it. The corrected fixture assigns integer weight units summing
to exactly10000. The original generator, rejected input hash and failure log are
retained. No engine check was weakened; heights and physical settings are unchanged.

Frozen binary: `.cache/terrain-v2/residency/before-game-benchmark`.
Fixture reproduction: retained `prepare_case.py` creates only its own disposable
project and refuses replacement. Its input and native products are hashed. Current
benchmark: `build/judas_terrain_v2_game_benchmark PROJECT.judasproj`. Timing tests
are diagnostic; correctness gates contain no arbitrary speed threshold.

Main, releases, the installed RPG SDK, Advanced Fluid lock, existing game projects
and independent research remain unchanged. Automated evidence is not human testing.
