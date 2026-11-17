# Ragdoll ownership and physical animation

A Ragdoll component maps imported skeleton keys to ordinary native rigid bodies
and constraints. RuntimeWorld owns the articulation and its final pose contribution;
the renderer consumes skin matrices and does not own physical bones. Mapping,
physical regions and authority are separate from animation playback and IK.

## Author the mapping

Add Animation and Ragdoll to an ordinary model entity. Choose explicit joint keys,
parent mapping, shape, offset/orientation, mass, material and collision filters.
The mapping is ordered parent-first, with one physical root; unmapped visual/helper
nodes need no body. Boxes/spheres and supported passive fixed, hinge, ball and slider
constraints reuse MK2. Do not infer anatomical axes or limits from imported names.
The editor/import tools can fit explicit mappings; no automatic humanoid controller
or arbitrary rig conversion is promised.

Named scene/prefab serialization and ordinary dependency/export paths preserve
mapping configuration. Refer to [model import](MODEL_IMPORT.md),
[IK configuration](MULTI_TARGET_IK.md) and [the exact API](judasjs/animation-ragdolls.md).

## Explicit authority transitions

`ragdoll.configurePhysical(settings)` declares physical regions/settings; it does
not itself request active mode. `setMode` queues an authority request at the fixed
boundary. Animation, partial, active and passive are distinct policies:

- Animation uses the resolved clip/layer/IK reference.
- Partial physical animation retains motor/root ownership while mapped dynamic
  regions respond to real forces, contacts, inertia and bounded relative torques.
- Active/full physical control requires an explicit motor handoff. Torque effort
  can saturate and physical bodies can lag their targets.
- Passive releases active effort while retaining physical momentum and contacts.
  `effortWeight` is not `poseWeight`: a visual blend does not disable physics.

Returning to animation/motor control requires a game-chosen collision-valid placement
and explicit resume/fade policy. Refusal preserves physical authority. No implicit
stand-up, natural get-up, contact selection or animation recovery is supplied.
[Balance](BALANCE.md) is a separate bounded controller with its own request and
support conditions, not a guarantee that an arbitrary articulation can stand.

Use the public reference's complete `setMode` placement/options schema. Existing
`enter`/`leave` operations retain their explicit legacy *API form*, not a separate
legacy simulator or old persistence system.

## Pose, bodies and events

References/IK prepare before physics; articulation poses resolve from settled body
frames afterward, preserving bind/root relations and explicit visual blending.
Use `ragdoll.body(key)` for a fresh safe mapped body (or null); ordinary impulses,
queries and filters apply. Presented joint/socket reads serve visuals, while
physical policy reads authoritative body state. Event opt-in routes supported
mapped contact observations to the owner through the existing event lifecycle.

Disabling/destroying a mapped body or losing/replacing the skeletal asset retires
that articulation at its validated boundary. It does not silently bind old bodies
to a new skeleton. Owner/hierarchy destruction, Stop and scene replacement release
owned bodies and constraints. Generation checks prevent recycled slots aliasing
an old mapping.

## Persistence and integration

Modern SaveService/WorldPersistence slots preserve supported active articulation,
physical mode/settings/poses/velocities, motor relation and animation continuation.
Internal transient bodies are durable resources of their owner, not independently
saved gameplay entities. Derived palettes/status rebuild; previous contact events
are not replayed as new gameplay. Retired pose-delta saves are not supported.

Ordinary compatible animation can suspend using retained region records; active
articulations, pending transitions or cross-owner attachments can pin a region.
Read the actual retention reason. Origin shifts translate the same physical owners,
not a visual-only skeleton. Whole-scene reload is not a save load.

Use the current [animation studio](../projects/animation_studio/README.md), native
IK/physical-animation/save fixtures and [qualification matrix](DEVELOPMENT_READINESS.md)
for exercised scope. Source-only examples are not all bundled in an installed SDK.
