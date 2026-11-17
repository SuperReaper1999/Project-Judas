# Deformable simulation and ownership

For physical panels and structural carriers, start with
[deformable authoring](DEFORMABLES_V2_AUTHORING.md). This guide explains the
current cloth/solid numerical model, ownership, contacts, connected hulls and
material scheduling; [qualification](DEVELOPMENT_READINESS.md) has its own scope.

## Ownership and numerical model

`RuntimeWorld` owns instances, fixed-step evaluation and lifetime. Immutable
`.judasdeform` assets contain topology, named groups, boundary triangles and stored
render weights. Nodes are not entities or rigid bodies. Instances contain double
local-frame positions/velocity, previous samples, plastic rest matrices, material
settings, released attachments and bounded contact storage.

CPU XPBD uses actual `fixedDt/substeps`: lambda starts at zero each substep and
accumulates only across its iterations. Cloth has two Green membrane extension
constraints and shear per triangle, plus an opposite-vertex distance bend surrogate
on shared edges. This simple bending law is not a calibrated dihedral shell.
Solids use six Green strain constraints per tetrahedron and a separate determinant
volume constraint. No matrix blending, global vertex normalization or surface-only
volume balloon. This is a game-oriented strain-energy approximation rather than
calibrated Neo-Hookean material or measured Young's modulus.

Compliances divide by triangle area / tetrahedral rest volume; their physical
meaning is inverse generalized strain energy stiffness. Bend compliance is inverse
distance-constraint stiffness. Density is kg/m² for cloth, kg/m³ for solids; positive
nodal weights multiply the geometry mass distribution. Damping and relative-air
velocity relaxation use exponential rates in s⁻¹. Gravity is sampled through the
selected Judas field at every node; zero gravity is valid.

Plasticity uses the magnitude of Green strain relative to each instance's current
rest matrix. Above yield, bounded rate-limited rest flow removes rigid rotation via
polar iteration and preserves each element's original rest volume. Maximum rest
strain bounds the supported flow; topology never changes. Below yield no rest
update occurs. Large folds/inversions are outside the supported solid envelope;
nonfinite/inverted final elements halt with a diagnostic, never silently reset.

These choices use the compliance and bounded-substep approach of
[XPBD](https://matthias-research.github.io/pages/publications/XPBD.pdf) and
[Small Steps](https://matthias-research.github.io/pages/publications/smallsteps.pdf).
Finite iterations still affect response; the supplied numerical comparison reports
that difference. The separate volumetric/distortional direction in
[Stable Neo-Hookean Materials](https://matthias-research.github.io/pages/publications/neohookean.pdf)
informs the separation of constraints; deformable deliberately uses the simpler Green-strain
approximation and does not claim that paper's constitutive law. [Surface contact](https://graphics.stanford.edu/papers/cloth-sig02/)
is handled separately from material constraints.

## Contacts and coupling

| Pair / consumer | deformable coverage |
|---|---|
| Static/dynamic box, sphere, compound boxes | Authoritative geometry + tree candidates; surface nodes, edge midpoints, face centroids |
| Cloth self-contact | Swept vertex/face side test + discrete edge/edge proximity; one-ring exclusions |
| Cloth/solid and deformable/deformable | Same accelerated boundary contacts; bilateral layer/mask filtering |
| Terrain | Not an deformable contact primitive |
| CharacterMotor blocking, collision-event events | Unchanged; no deformable contact/event expansion |
| shape-query casts | Continue querying rigid geometry |
| Deformable ray picking | Current triangles; checked generation/triangle/barycentric location |

Primitive contact sampling is a bounded surface-aware approximation. Very small,
fast colliders can miss a coarse triangle between samples; edge/edge is not full
continuous collision detection. Use suitable authored resolution and substeps;
ordinary curtain/cape/loading tests define the demonstrated envelope. Static BVH
topology is refitted against current/substep-start bounds. Self-contact candidates
use a conservative half-thickness motion margin; both current and start positions
are monitored and exceeding that margin rebuilds the pairs. Narrowphase still runs
on the current geometry every iteration, at unchanged cadence/quality. Candidate
storage is capped with an overflow diagnostic rather than silently dropping pairs.

The ordinary rigid world steps once. After animation/ragdoll resolution, deformable
substeps project contact using finite node mass and body inverse mass/world inertia,
then submit the equal opposite impulse at the same point exactly once. Corrected
positions reconstruct node velocities; there is no second particle reaction impulse.
Moving body point velocity enters friction. External deformable pairs resolve at
one fixed boundary, with their positional delta applied once to velocity. Prescribed
world/bone/static targets may inject work; free dynamic targets receive reaction.

## Attachments, frames and presentation

Groups target owner-frame world anchors, body-local frames, or stable skeleton
joint keys. Bone targets consume the existing resolved authoritative world pose
observation after animation/ragdoll, without another animation clock. Targets have
previous/current fixed matrices sampled through substeps. Lost/disabled targets
release; explicit reattachment is project intent. Owner root is initial placement,
not a second carrier transform applied to world-space simulated nodes. `reset()`
coherently reconstructs placement, velocities, plasticity, interpolation and pins.
For a deliberate target teleport, reset dependent deformables explicitly. Replacing
a binding invalidates its old target-motion observations. Unit owner scale is required
and incompatible root body/motor/skeleton producers are rejected; attachments can
refer to other bodies/skeletons. Static/bone targets prescribe motion, while dynamic
body targets use a velocity-level bilateral solve and finite reaction impulse.

Fixed-origin double absolute world coordinates remain M23's policy; local floats
are used by gravity/physics/render boundaries. No live rebasing was added.
Stored barycentric/tet weights permit denser rendering. UV/material seams remain
separate render vertices; OBJ source position identity joins physical seams without
position-welding separate layers. Imported indexed meshes without a source identity
stream use their explicit indices as the topology contract.

Renderer owns indexed mesh updates, GPU lifetime, normals/tangents/bounds and normal
material slots, shadow/camera/mask/culling submission. Mapping uploads once per
changed pose/alpha and is reused by additional passes/cameras. No visibility-driven
simulation. Quiescent instances sleep, waking on force, impulse, changed gravity or
material, moving attachment/body, or deformable contact.

## Authoring and public contract

The ordinary component inspector creates sheets/tet blocks and imports indexed
cloth. A local selection box bakes a named node group. Attachment lists choose
world/body/bone targets; wireframe/pins and resource/configuration errors are shown.
Render subdivision is an explicit stored binding choice; normal Render materials
apply. `judas_deformable_bake` exposes the same generic helpers for developers.
Scene/prefab fields use the existing component property map and stable IDs. Helpers
remove bindings to groups absent from replacement topology and report it. Import
uses the normal mesh parser, including OBJ source-position identity across UV seams.
Bounds: 8,192 nodes, 32,768 simulation triangles/tetrahedra, 65,536 render vertices
and indices, 64 groups/attachments and 1–16 substeps/iterations; payload and adjacency
storage have explicit bounds. Large-resolution jobs may exceed these limits and
fail diagnostically. Derived geometry, render data and node/total mass must also
remain finite and representable; rejected material changes are atomic. Tetrahedral import is through the versioned pre-baked asset
representation; deformable has no arbitrary tetrahedralizer or universal external tet format.

[JavaScript reference](judasjs/deformables.md), [declarations](judas.d.ts), and
[executed example](judasjs/examples/deformable.js) cover the small handle API.
Forces, impulses, attachment/material changes and reset belong in fixedUpdate.
No raw node arrays/pointers are exposed to JS.

## Persistence and residency

save-slot adds a required conditional version-1 `deformables` participant. Scenes without
deformables keep their previous participant set. Saves include positions/velocities,
plastic matrices, runtime materials/attachments, release/enabled/sleep state; handles,
GPU buffers and candidate caches are rebuilt. Missing/failed required instances
reject capture. Asset bytes remain covered by existing content compatibility.

region captures independent state once before sliced teardown, using streaming
participant version 2 only when retained deformable state exists. Fresh owner IDs
remap internal and qualified attachment references. Live cross-region attachments
pin their target region, including runtime-changed bindings; released pins do not.
Animation/ragdoll regions retain their existing explicit residency limitations.
No large arrays are captured repeatedly during each unload-budget slice.

## Connected hulls and shared resources

A finite structural interface may set `JointMaterial.connected`. Its default is
false for general destructibles. Connected interfaces retain their material
stiffness, damping, yield and hardening. Cumulative plastic translation/rotation
have finite budgets; once consumed, further loading is elastic and physical
unilateral stop rows resist translation/rotation outside the authored envelope.
The solver applies reactions through ordinary MK2 Jacobians. It never teleports
sections, zeroes their velocity or releases a connection because its hull
plastic capacity was consumed. Hull connections reject explicit fracture/piece
removal requests that would disconnect the protected structure.

The editor exposes the flag per finite interface. The generic authoring command
`judas_deformable_bake connected-hull OUTPUT SOURCE [bondKey ...]` cooks selected
finite interfaces, or all finite interfaces when no keys are supplied. Select
only main-hull connections: retain ordinary finite failure laws for designated
separate components. The new asset revision `JudasDeformable7` records this
policy; ordinary fragmenting assets retain their existing representation.
Runtime plastic history remains instance-owned and uses the existing save
participant. A saved connected hull that claims broken protected interfaces is
rejected rather than silently restoring an impossible structural state.

Shared immutable geometry clones retain distinct material slots, samplers,
section ownership and per-draw palettes. Mutation of leased geometry through
vertex/topology update APIs is rejected. Destroying one instance releases only
its leases; the last instance frees the GPU geometry and image storage.

## Material phase scheduling

Full internal, self-contact and finish kernels run in exclusive batches on the
existing job pool when transfer is guaranteed no-throw. Rigid/terrain contacts,
attachment reactions and publication remain ordered on the owner. Small batches
and platforms without safe transfer execute the same kernels on the owner.
`JUDAS_MATERIAL_LANES=1|2|3|4` is a diagnostic execution policy; lanes do not change
physical quality, time, constitutive laws or authoritative publication order.
Workers join and state is restored on failure before shared phases continue.

Severe pile-ups remain expensive; no universal real-time crash guarantee follows
from numerical correctness. Use [whole-application measurements](WHOLE_ENGINE_PERFORMANCE.md)
and the [current qualification boundary](DEVELOPMENT_READINESS.md).
