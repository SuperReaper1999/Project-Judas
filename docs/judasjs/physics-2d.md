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
and fixed world `normal`. A capsule has two core endpoints; total height is
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
