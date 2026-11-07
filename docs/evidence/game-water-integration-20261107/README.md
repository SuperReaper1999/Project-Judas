# Grid Water V2 — ordinary runtime integration

Experimental source preview, Linux qualification, 7 November 2026. Developed from
`d777c4f92f8c2dd8647c8d9115ea150ceff8c21b`. Main remains
`1094d64515604fe9d03fdab479efc7f880b112a8`; the Alpha fluid lock is unchanged.
This is implementation and automated evidence, **not human acceptance**, an Alpha
unlock, published SDK or clean-machine/platform certification.

## What is now integrated

- The existing **Fluid Volume** component selects the grid backend explicitly.
  Old particle components keep their format, backend and canonical fingerprints.
  New editor-added volumes select Grid Water V2 and expose dimensions, fill,
  density, colour, opacity and the experimental warning.
- RuntimeWorld owns one optional service; separate modules own geometry/cache,
  queries, presentation, sources and persistence. Default-off projects allocate
  neither a grid service nor the legacy particle solver. Grid-only projects do
  not start PBF. Scripting uses the existing shared JudasJS VM and a local Water
  binding module, not a parallel runtime or central-dispatcher expansion.
- Existing MK2 geometry supplies moving boxes, spheres, capsules and convex
  compounds. CharacterMotor query capsules are moving geometry, not static bake
  input or independently integrated rigid actors. Existing swimming queries sample
  actual local water, including the excluded volume around a swimmer.
- Signed source/drain requests return actual accepted cubic metres. Initial,
  added, removed, stored and exterior-air quantities are explicit ledgers.
  Sealed/full sources apply backpressure. Invalid data and excessive CFL demands
  reject explicitly; failed fluid preparation/steps do not publish their staging.
- Normal save slots include the versioned grid state; water/air and source receipts
  survive private cold reconstruction. Missing, corrupt or mismatched water data
  rejects instead of reseeding. Derived geometry/gravity caches rebuild on load.
- Existing origin shifts move the field. Regional collider publication/removal
  invalidates static capacity without reconstructing/reseeding the grid. Water
  owners themselves remain resident: unsupported owner suspension and networking
  have explicit diagnostics rather than claims of universal composition.
- Gravity uses either the original uniform path or cached per-cell scene samples
  for a bounded planetary tangent chart. The reference down axis and 26-degree
  chart limit remain explicit. This is not one grid covering an entire planet.
- Renderer V2 uses a cached physical mesh with Basic Water's Fresnel/glint/sky
  material, plus existing underwater optics. Multiple resolved vertical layers
  have separate surfaces. Presentation does not own water mass.

The editable [Water V2 Workshop](../../../projects/water_v2_workshop/README.md)
provides walking/swimming, grabbing/throwing props, pumps/drains, impulses, normal
and tilted/radial pools, original planet-basin geometry, saves and ordinary export.
Game rules are project JavaScript. Its geometry generator and font licence ship
with the project. Existing games and independent research were not modified.

## Defects caught while integrating

1. Construction initially sat in the append-object path. Streaming in a collider
   destroyed the resident water service. The combined test crashed; its native
   backtrace is preserved. Construction now occurs only at world Build/reset;
   collider streaming updates the existing field. Publication/unload both have
   quantity and geometry-invalidation checks.
2. Character query actors initially looked static to the bake. A generic,
   read-only `PhysicsWorld::IsQueryOnlyBody` distinguishes them; moving characters
   no longer force static rebakes or leave a stale capsule hole.
3. Direct participant restoration passed, but the actual exported game revealed
   that SaveService's inventory omitted the grid chunk and rejected cold loads.
   That inventory now checks grid presence against active scene components and
   keeps the unknown/extra-participant count strict. The misleading generic
   deformable error is now a general inventory error, while real missing
   deformable state retains its specific diagnostic.
4. Programmatic enormous dimensions could overflow fingerprint multiplication.
   Bounds now precede multiplication. Serialized grid dimensions use the native
   total-cell contract rather than the unrelated particle-lattice axis limit.
5. Physical meshes need an explicit vertex-shader path that preserves their Y
   coordinates. The reviewed canonical mesh-vertex identity changed accordingly;
   it is not exempted from checking. Native pixels compare a 2 m physical surface
   with an independent prescribed 2 m plane and a deliberately wrong 0 m plane.
   The correct images agree exactly; the negative control differs by 9.45 average
   channel levels in the rotated case. Basic Water rendering still restores.

`failed/` also retains fixture mistakes separately: the first cold fixture lacked
save metadata; its rounded 89.8 m³ expectation was tighter than binary32 voxel
capacity. The final cold oracle records the actual independently accounted source
quantity, with tighter restoration tolerance. No original physical gate was relaxed.
Restricted-context graphics/LSan failures were rerun with actual desktop access;
these are not reported as engine defects or hardware passes from software GL.

## Qualification

- **28 native grid checks**, **41 ordinary runtime checks**, **5 live JudasJS
  binding checks**, and separate cold-process write/read (**13 + 13**) pass.
- **64 preserved foundation checks** pass, including quiet water, floating/sinking,
  gate flow, finite air venting, terrain, original basin fixtures and cold processes.
  Across seven 1,200-step fixtures, **all recorded physical fields match the
  accepted foundation rows exactly**. Timing/memory fields are excluded. See
  `foundation/physical-comparison.json`; this is printed-field equality, not a claim
  that all private MK2 caches or every possible trajectory is bit-identical.
- The final coordinated Linux run passes **13 focused gates**: grid/unit/runtime/VM,
  MK2 physics, rotation and rotation cold saves, origin physics, water policy,
  fluid policy, project settings, Basic Water rendering/application/cold saves.
  Raw logs and machine provenance are in `regressions/`.
- Additional rendering checks: shader library **47**, water preview/physical
  surface pixels **26**, rendering architecture **44**, all pass. The old shader
  identity failure is preserved, then advanced only for the reviewed vertex change.
- Build-organization Python controls: **22** pass, including negative controls.
  The API/type checker passes 365 registered native operations, 25 structured result
  shapes, callable contracts and drift controls. The real Water VM test independently
  executes the new bindings; a whole live API-enumeration dump was not run here.
- ASAN + UBSAN + leak checking pass the **28 water unit checks**. The water sources
  are instrumented; the pre-existing whole engine library was not rebuilt under
  sanitizers. This is not a whole-engine memory-safety certification.
- Native GTX 970 rendering works in modern and forced portable paths. Editor
  Play/Stop leaves **byte-identical authored scene text**, and editor saving works.
  GPU diagnostic logging recorded no graphics errors in the qualified runs.
- The normal exporter creates a 4-scene game. A package is moved to a different
  folder containing spaces, launched from another working directory, and writes
  and loads a genuine save in separate processes. Explicit waits assert completion;
  the real restore callback proves pump state and nonzero source/drain receipts.
  `game-project/RESULTS.json` records the readback: 0.15250000795349475 m³ both added
  and removed, with quantity agreeing with its ledger within 6e-13 m³.

## Performance and memory

Ryzen 5 5600, 8 GiB RAM, GCC 15.2 Release. Native rendering: GTX 970 4 GiB,
NVIDIA 580.178.04 / GL 4.6. No concurrent compiler jobs during recorded CPU samples.
This was not a dedicated idle machine. Raw rows and process resource observations
are retained, with build/source hashes rather than an inferred version label.

| Ordinary runtime fixture | Mean / median | p95 | Worst |
| --- | --- | --- | --- |
| Tilted, 2,800 cells, disturbed 30 s | 2.439 / 2.417 ms | 2.572 ms | 2.844 ms |
| Radial, 2,800 cells, disturbed 30 s | 2.515 / 2.486 ms | 2.647 ms | 3.155 ms |

These include staging, gravity/geometry preparation, solver and rigid-body loads,
but not all other MK2/game/render work. Two retained grid buffers account for
1.20 / 1.27 MB; the multi-world native test peaks around 35 MB RSS. CSV quantity
uses six significant digits; separate native ledger/tolerance checks qualify it.
The 7,200-cell foundation pool, including MK2, measures 5.765 median / 6.298 p95 /
6.945 worst ms. This is essentially unchanged from the accepted foundation, not
an advertised speedup. The playable 6,000-cell pool reports about 6–7 ms inclusive
water work through the ordinary runtime and script-driven character.

A 900-presentation real-clock desktop trace retains its last 120 frames. Physical
water drawing is 0.335 ms median / 0.348 ms p95 on the GTX 970; the main GPU camera
interval is 0.815 / 0.828 ms. These are inclusive GPU intervals, not CPU simulation
or whole-session timings. The trace has no dropped profiler events or discarded
simulation time. Retained frame intervals average 2.18 ms; their 36.1 ms worst
includes the deliberate screenshot capture. This is a short diagnostic, not a
promise of hitch-free long sessions. Profiler-enabled process RSS peaks around
135 MB, including its substantial profiling buffer allocation.

## Reproduce and review

```sh
cmake -S . -B build -DJUDAS_BUILD_GAME_WATER_LAB=ON -DJUDAS_ENABLE_VALIDATION=ON
cmake --build build --target judas_game_water_qualification judas judas_editor judas_export -j2
build/judas_game_water_runtime_tests --measure /tmp/water-runtime-rows
# Create that measurement directory first if raw CSV output is wanted.
build/judas_game_water_runtime_tests --write /tmp/water-cold-fixture
build/judas_game_water_runtime_tests --read /tmp/water-cold-fixture
python3 tools/validation/qualify_game_water.py --executable build/judas_game_water_lab --output /tmp/water-foundation-proof
python3 tools/validation/qualify_water_workshop.py --runtime build/judas --exporter build/judas_export --project projects/water_v2_workshop/water_v2_workshop.judasproj --output /tmp/water-game-proof
```

The game-proof output must be fresh and outside the checkout; it retains its
relocated package and evidence. Native desktop access is required for rendering.
The current [support boundaries and human checklist](../../GAME_WATER_PREVIEW.md)
remain authoritative: resident/stationary domains, no independent-grid transfer,
no qualified moving sealed-container/air-piston or exact added-mass coupling,
coarse thin-feature geometry, and no new Windows/Android/browser qualification.
Main and the Alpha lock stay unchanged pending review. This checkpoint does not
claim every original Advanced Fluids ambition is implemented or that old research
failures have become scientific passes.
