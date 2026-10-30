# Native 2D Stage 2 — planar physics and playable presets

Experimental development toward Alpha v1.1.0. This is not publication or human
acceptance. Starting source: `3916ff5b57b75503f8f2d770962407c39b9c0290`.
Main and published Alpha packages remain unchanged.

## Delivered architecture

Native circles, boxes, capsules, strictly convex polygons and static two-sided
chains share MK2's world, handles, clock, broadphase, contacts, CCD, material
filters, sensors and persistence. They are not thin spatial solids. Body-local XY
is the motion plane; spin is about its fixed world normal. Visual sprites remain
independent from collision, mass, sorting and flips. The original sprite-only
Stage 1 project remains available.

For unit plane normal `n`, reduced linear mobility is `(I - nnᵀ)/m` and angular
mobility is `nnᵀ/Iz`. The physical inertia tensor stays invertible for validation;
the constrained response is reduced before solving. A custom tensor uses
`1/Izz`, which is generally different from `(I⁻¹)zz`. Ordinary lamina tensors
retain their direct scalar path. Shape mass uses surface area and exact lamina
moments, not arbitrary extrusion thickness. The generic mass-weighting field
named `volume` carries area for this shape; it is not a fluid displacement volume.

Contact normals/witnesses come from native rounded-core/polygon geometry in
binary64 parent-local frames. Polygon incident-edge clipping retains distinct
penetrating corners. Planar contacts refresh their finite/rounded features during
substeps: a rolling material anchor is not the next geometric support point.
Planar friction has one physical tangent, including oblique frames. No solver
iteration budgets, damping or 3D physical acceptance thresholds were changed.

Native pin joints use two anchor rows and free spin, with existing motor, limit,
spring and rotational-resistance settings. Welds add one angular row. Coupling
preflight rejects unsupported spatial joints/assemblies instead of silently
adding alignment constraints. See [API and exact boundaries](judasjs/physics-2d.md).

## Developer workflow

File → New project → New 2D game, top-down or side-view, creates ordinary sprites,
native collision and editable project JavaScript. The side-view preset includes
gravity/jumping; the top-down preset has no gravity. The player uses bounded
forces, jump impulses, native casts and support point velocities. No height clamp,
pose parenting or second movement/physics system is hidden in C++.

Hierarchy creation includes native static/dynamic boxes, circles, capsules,
static chains and both player presets. Body → Native 2D edits geometry, rotation
locking, materials, sensors and filters; changes are validated before immutable
shape publication and participate in normal undo/redo. Collision preview and
body debug drawing show contours; collider-only chains are selectable away from
their pivot. Pin/weld authoring is in the existing Joint component inspector.
Modern scene/prefab/save/export paths carry the native descriptor and actual mass,
inertia, fixed normal and existing joint continuation.

Public additions: `Entity.body2D`, `physics.cast2D`, joint types `pin2d`/`weld2d`,
and `planar` collider/hit metadata. Existing force/velocity/kinematic APIs are
reused. Invalid off-plane script commands raise catchable TypeErrors before
publishing a partial definition/physical endpoint. Declarations, reference and
inventory are checked against the actual public module.

## Playable lab

Project: [`projects/planar_playground/Planar_Playground.judasproj`](../projects/planar_playground/Planar_Playground.judasproj).

```sh
build/judas_editor projects/planar_playground/Planar_Playground.judasproj
build/judas projects/planar_playground/Planar_Playground.judasproj
```

WASD/arrows move; Space jumps. **1** platformer, **2** top-down, **3** a tilted-plane
version; **R** restarts; **F6/F7** use modern save/load. Push crates, traverse the
ramp, ride translating/rotating supports and reach the green sensor. The editable
lab is ordinary scene/assets/scripts. Its authoring source is
`tools/demos/PlanarPlayground.cpp`; `judas_planar_playground FRESH_DIRECTORY`
regenerates it. Export with the normal `judas_export` tool.

## Verification and numerical evidence

The committed evidence index is [here](evidence/2d-stage2/README.md). Local detailed
outputs, builds, screenshots and the standalone are under `.cache/2d-stage2-evidence`.
The supported qualification is native Linux Release, GCC 15.2; graphics application
checks use Mesa llvmpipe. No Windows/Android or human acceptance is inferred.

- The native suite has 52 checks; runtime/editor/prefab/stream integration has 73 checks, plus 7 fresh-process restore checks. Native tests include the unchanged 300-step 3D trajectory digest
  `f67106f8392d2826`, lamina mass, impulses, rotation locking, finite contact,
  chain gaps/seams, filters/sensors, native pin/weld/motor/limits and persistence.
- The 150 m/s box stops at a real 0.1 m thick wall. Independent transformed-corner
  bounds keep the tested off-centre landing below 2 mm penetration.
- A rolling circle crosses a collinear chain vertex with minimum height
  `0.299999982 m` for radius `0.3 m`.
- The elastic spinning-box case (5 m/s, 20 rad/s, 2 seconds) has initial energy
  `66.6666666667 J`, measured peak `66.6666717715 J`; actual transformed vertices
  stay inside the declared finite walls. This is a measured floating-point bound,
  not an exact-arithmetic conservation claim.
- Rotated friction before correction: aligned/rotated speeds `2.54749703` /
  `3.21068668 m/s`. After the single-tangent correction: `2.54749703` /
  `2.54751706 m/s`; position difference about `4.6 µm` after 0.5 seconds.
- The actual JS player moves `1.47639 m` while its kinematic support moves `1.5 m`
  and remains at height `1.8 m`. A rotating support lifts the player to
  `1.91902 m`. Tests examine physical endpoints, not only grounded flags.
- Positive/negative runtime controls cover real floor landing, jumping, no-floor
  falling, top-down input, ignored-self casts, unsupported casts, atomic invalid
  mutations, descriptor corruption, native convex prefabs, actual region unload/
  revisit/continued rotation and fresh-process modern save restoration.
- Existing MK2 rotation, joints, kinematics, CharacterMotor, continuation, wheels,
  powertrains, isolated collision predicates and Stage 1 sprite/rendering tests
  remain part of the focused qualification. Machine results distinguish selected
  gates from the entire engine test suite.

`judas_planar_benchmark` reports 90 measured steps after 30 warmup steps for
16/64/128 native boxes in sparse resting stacks. Record median, nearest-rank p95,
worst, physical minimum height and plane drift together. Sleeping is enabled;
these are representative new-feature costs, not a before/after speedup claim,
active-impact stress certification or a promise for thousands of bodies.

## Boundaries and remaining work

- One runtime world's physical producers use the planar or spatial domain;
  mixed 2D/3D physical coupling is rejected, including the legacy spatial player adapter. Spatial visuals/cameras remain usable.
  Interacting bodies must be coplanar; sort sprites rather than changing body Z.
- Fixed rotated planes are supported; curved motion manifolds, changing a body's
  plane during simulation and spatial CharacterMotor/ragdoll/wheel/deformable/
  fluid coupling are unsupported. Native pins require aligned plane normals.
- Float body state has ordinary finite-precision error. Native geometry is not
  an arbitrary-scale exact-predicate certification. The rotated support fixture
  measures normal-position drift below `0.0001 m`; command/frame checks have
  explicit small numerical tolerances, not a post-step repair.
- Chains are static, finite, two-sided segment unions, not automatic filled
  contours, dynamic concave polygons or one-way platforms. Sensors publish normal
  fixed-boundary pair events; high-speed trigger crossing is not certified here.
- Snapshot casts translate at fixed rotation; native simulation CCD handles
  rotational motion. No 2D navigation generation or fluid-volume approximation
  is implied by these colliders.
- Legacy `.judasstate` deltas reject planar bodies. Use modern save slots.
- Tilemaps, frame animation and subsequent v1.1.0 stages remain unimplemented.
  The example character is intentionally a static sprite at this stage.
- The rotated lab's Play camera follows its plane. Editor 2D mode still edits
  world XY; use the existing 3D editor view to inspect arbitrary rotated frames.

## Human checklist

1. Create both 2D project templates. Walk into walls; jump/land in side-view;
   confirm W/up moves upward in top-down. Edit the copied project script.
2. Create native bodies/chains from the hierarchy. Edit dimensions, vertices,
   rotation lock and material. Preview/pick collision; undo/redo changes.
3. In the lab push crates, jump onto platforms, ride both moving supports,
   traverse the ramp and reach the green sensor.
4. Switch modes 1/2/3; confirm camera/input and physical contacts remain coherent.
5. F6, move objects/player, F7; inspect restored progress. Stop Play and confirm
   authored state is restored. Try the relocated standalone as well.

Human acceptance, main promotion and v1.1.0 publication remain operator decisions.
