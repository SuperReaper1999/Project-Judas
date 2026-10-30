# Native planar physics (Experimental Stage 2)

A native 2D body uses MK2's existing world, time step, handles, broadphase,
contact solver, CCD and joint ownership. It has two translational degrees of
freedom and one spin degree of freedom. It is not a thin spatial box and no
post-step pose clamp repairs it. The initial local XY plane is fixed in world
space; arbitrary fixed rotated frames are supported. Curved motion manifolds
and coupled spatial/planar assemblies are unsupported.

## Authoring

New 2D platformer/top-down projects now contain real native bodies and the
editable `Assets/game.js` controller. Hierarchy → Create provides native boxes,
circles, capsules, static chains and both player presets. Body → Native 2D
contains geometry, rotation lock, motion authority, materials, filters and sensors.
Preview collision draws its actual contour. Keep transform scale 1; collider
sizes and sprite sizes are independent. Sprite flips, sorting and pivots never
change collision. Convex polygons require consistent strict winding (3–64
vertices); open two-sided static chains accept 2–1024 vertices. No automatic
sprite-alpha collision tracing or tilemap authoring is implied.

Use a modern project with `legacy-gameplay` disabled (the 2D templates set this). The legacy spatial player adapter is rejected in planar worlds. One runtime world must use one physical domain. Planar bodies reject spatial
CharacterMotor, ragdoll, wheel, powertrain, fluid and attachment coupling; 3D
visuals/cameras/UI remain allowed. Use the project-owned 2D controller rather
than the spatial CharacterMotor. Dynamic chains and unsupported mixtures fail
preflight with an actionable error. There is no hidden fallback to 3D collision.

## `Entity.body2D`

Detached metadata or `null`: `kind`, `radius`, local XY `vertices`, `lockRotation`
and fixed world `normal`, plus `oneWay` for authored one-way chains. A capsule has two core endpoints; total height is
`2*(radius+halfHeight)`. Existing `velocity`, `angularVelocity`, `applyForce`,
`applyTorque`, `applyImpulse`, `applyImpulseAtPoint`, `pointVelocity`,
`setMotionType` and kinematic commands operate on actual native bodies.
World-space vectors remain `{x,y,z}`; off-plane forces/impulses are absorbed by
reduced mobility. Forbidden off-plane velocity/pose commands reject, rather
than silently relocating the body. A rotation lock absorbs spin torque.

## `physics.cast2D(shape, pose, direction, maximum, filter={})`

Returns the ordinary `CastHit|null` snapshot, including entity, point, normal,
distance, fraction, initial overlap and stable chain-segment `childKey`.
Shapes: `{kind:'circle',radius}`, `{kind:'box',halfExtents:{x,y}}`,
`{kind:'capsule',radius,halfHeight}`, `{kind:'convex',vertices:[{x,y},...]}`.
A circle with radius 0 is a point ray. Pose is `{position,rotation?}` and has unit
scale. Keep interacting bodies coplanar; use sprite sorting rather than changing a physical body’s Z to change draw order. Direction must lie in the query plane. Casts translate at fixed rotation,
not predicted rotational sweeps; native simulation CCD handles moving bodies.
Existing layer/tag/ignored-body/sensor filters apply. Invalid descriptors,
unsupported casts and nonconvergence throw; no silent repaired hit.
Spatial ray/sphere/capsule/box queries, `world.overlap` and `closestPoint` intentionally do not hit planar bodies. Multi-hit 2D overlaps/closest-surface queries are not part of this stage.

```js
const body = player.body2D;
const floor = physics.cast2D({kind:'capsule',radius:body.radius,
  halfHeight:body.vertices[1].y},player.transform,
  {x:0,y:-1,z:0},0.12,{ignored:[player]});
```

## Joints, events and persistence

`physics.createJoint` accepts `type:'pin2d'` or `'weld2d'`. A pin has two anchor
rows and free relative spin; optional angular motor, limits, spring and
rotational resistance use the existing joint settings. The coordinate is B
minus A about the fixed plane normal, in radians. A weld adds one angular row.
Frames use local Z as the planar normal, not the spatial hinge's X. Spatial
joints and constitutive damage laws are unsupported for planar bodies.

Existing collision and trigger callbacks publish bounded pair events, including
contacts between otherwise unscripted bodies. Sensors have no impulse response.
Modern save slots carry actual mass, inertia, fixed plane, geometry and existing
joint continuation. Legacy `.judasstate` deltas explicitly reject native 2D;
use `saves.save`/`saves.load`. Prefabs/export reuse the ordinary scene path.

The reusable controller applies bounded forces and jump impulses in fixedUpdate;
support detection uses native capsule casts and point velocities. It does not
teleport, clamp height or simulate a separate physics engine. Gameplay-specific
coyote time, wall jumps, one-way platforms and character animation are project
policy and are not provided by this initial preset.

## One-way platforms

Hierarchy → Create → **2D one-way platform** authors a visible static segment.
Alternatively choose a two-vertex static chain in Body → Native 2D and enable
**One-way platform**. The blocking side is left of vertex 0 → vertex 1; a
left-to-right segment blocks from above. Preview collision draws an arrow toward
that side. Rotate the fixed frame for a different gravity orientation.

Admission uses the other body's lowest support at the beginning of the step,
with the existing 2 mm native contact margin. Broadphase pairs remain geometric;
only contact response/CCD admission is directional. A jump from below is not
blocked halfway through, and a separate descending body can land simultaneously.
Core distance geometry remains continuous for the chronological CCD solver.
Queries use their current start pose and the same side rule. Sensors continue to
report ordinary geometric overlap; one-way is response policy, not trigger hiding.

This initial representation is **straight, two-vertex, static segments**. Moving
platforms, curved one-way chains and a built-in drop-through command are not
provided. Spawn on the intended side; deeply intersecting starts are not repaired
or teleported above the platform. Standard two-sided descriptors retain their
version-1 bytes; opted-in geometry uses descriptor version 2. Normal scenes,
prefabs, modern saves and exports carry that descriptor. No private crossing timer
or disposable unpersisted solver flag is needed for cold continuation.

## Grid RPG preset

New Project → **New 2D game - grid RPG**, or hierarchy → Create → **2D grid RPG
player**. The generated script is an ordinary editable copy of
[grid-player-2d.js](examples/grid-player-2d.js). Use one camera owner; when adding
this to an existing game, disable the prior player/camera script or turn off this
controller's `followCamera` property deliberately.

The preset uses a rotation-locked **kinematic box** with cardinal movement.
Configure cellSize, speed and camera properties in the Inspector. Each input
starts a full, cast-checked cell step; releasing it does not leave half a step.
Held input repeats steps; diagonal input resolves one axis, never corner cutting.
The preset adds rebindable grid_left/right/up/down button actions alongside the
movement axes, so even a key press/release between physics steps is retained.
One upcoming short turn can be buffered while a cell finishes; it is saved state.
Existing configured bindings are preserved. The hierarchy preset persists newly
added logical actions in the project; New Project saves them normally.
Facing, committed cell, pending target, blocked status and completed-move count
are saved project state. Native velocity integration moves the body; there are no
position snaps, teleports, extra physics clock or fake collision boundaries.

If a new obstacle enters a step, the actor stops. It resumes when clear; reverse
input can back out to the last committed cell. Separating initial overlaps are
skipped only after another cast checks for blockers behind them. Eight overlapping
blockers exhaust the bounded query and conservatively stop rather than declaring
clear passage. A returned partial step does not invent a cell completion.

This is a bounded flat/fixed-frame RPG movement policy, not a pathfinder, NPC
schedule, battle framework, live-origin-shift controller or interpolated animation
system. Kinematic authority is deliberate: it does not make a dynamically pushed
ragdoll/controller. World updates and save compatibility remain normal project
policy. Foot-pivot visuals/Y sorting are authored separately from collision.
