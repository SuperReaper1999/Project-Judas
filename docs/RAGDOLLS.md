# M48 — articulated physics as a pose producer

**M70 candidate extension:** shared multi-target IK and optional partial/full-active
physical regions now extend the historical clip/pose/passive-ragdoll scope below.
The older milestone statements remain their historical contracts. See
[M70](M70.md) for current phases, authority and limitations, and the
[JudasJS reference](judasjs/animation-ragdolls.md) for exact public interfaces.
M61 already persists animation/articulation runtime state; M70 extends that
ownership for physical modes and pending requests.


**Status through M69:** M48 is operator-accepted and checkpointed. Candidate/pending
statements and measurements below record the original milestone review state,
not a current outstanding acceptance gate. Later contracts take precedence;
start with [Architecture](ARCHITECTURE.md) and [JudasJS](JUDASJS.md).

Current public JavaScript signatures, examples and lifetime rules: [JudasJS reference](JUDASJS.md). This document retains milestone architecture and evidence context.

A Ragdoll component is an ordered, parent-first mapping from imported skeleton
joint hierarchy keys (or unique names) to ordinary runtime rigid bodies. There
is exactly one mapped physical root; not every skeleton node needs a body.
Supported shapes are boxes and spheres, with joint-local shape offset/orientation,
mass, friction/restitution and ordinary project collision layer/mask. Each
non-root mapping names a mapped skeleton ancestor and an M45 passive fixed,
hinge, ball/socket or slider constraint. Hinge/slider limits retain M45 semantics;
ball/socket does not acquire a new angular cone limiter in M48.

## Authority and transitions

`RuntimeWorld::EnterRagdoll` validates the entire mapping/scale before creation.
It places bodies from the **current final resolved pose**, not bind pose. The
recent two fixed-step world-pose observations provide linear and angular motion;
without observations the entity's existing motion is inherited. A seek/teleport
is not interpreted as a continuous physical trajectory. Uniform positive entity
and mapped joint scales are supported and frozen for the activation. Shear,
reflection and nonuniform mapped scale are rejected explicitly. A separate
collider on the animated owner is unsupported; mapped bodies are its physical
representation. Creation failure releases the partial articulation.

Automatic anchors capture each child skeleton pivot in both currently placed
body frames. Manual anchors are body-local authored values, scaled on entry.
Constraint frames are explicitly authored body-local frames (X is hinge/slider
axis), so limits retain a clear mechanical reference. Bodies and constraints
use the normal PhysicsWorld solver, contacts, gravity, generation-aware lifetime
and motion integration. No pose motor, fake force or post-step physics correction
exists. Nonadjacent self collision remains enabled by default; each connection
may suppress its parent pair. An explicit selfCollision=false suppresses all
intra-articulation pairs. Suppression lives in the ordinary early physical pair
filter, keyed by complete body generations and removed on destruction. Queries
remain independent of physical pair suppression.

After the normal fixed step, mapped physical transforms convert through explicit
shape offsets and the skeletal parent hierarchy into a validated M47 external
contribution, `ragdollPhysics`, priority 1000. The same final pose and GPU skinning
serve all cameras. The owner's reference **translation follows the physical root**;
its reference orientation/scale stay fixed. Physical rotation is represented in
the skeletal pose. Updating this collider-free reference never teleports the
mapped bodies. There is no duplicate skeleton and no renderer-only physics path.

Leaving captures the final physical pose, destroys the bodies/constraints, then
fades the full `ragdollReturn` contribution to the ordinary resolved animation.
Clip clocks continue underneath while active. The transition is visual recovery,
not balance, standing up or collision-controlled character locomotion. A duration
of zero is explicit immediate replacement. No active body is driven to animation.

## Runtime API / lifecycle

```js
const r = world.entity('10').ragdoll;
r.enter();
if (r.active) r.body('Root/Elbow/Tip').applyImpulse({x: 3, y: 1, z: 0});
r.leave(0.6);
r.enabled = false;
```

`body(key)` returns the ordinary safe mapped entity, or null. Existing force,
impulse, torque, query and filtering APIs apply. Collision events retain ordinary
per-body semantics; they are not newly aggregated onto the collider-free owner.
Destroying/disabling a mapped body invalidates the articulation at the next fixed
pose boundary and releases its remaining bodies/constraints. Owner/hierarchy
destruction, Stop, reset and scene replacement release everything. Complete
body-generation validation prevents slot reuse from aliasing a prior mapping.
Asset replacement/unavailability ends the old articulation rather than rebinding
old physical bodies to a different skeleton.

Mapped internal entities are transient owner resources, excluded from independent
save deltas. M48 does not persist active articulation/animation mixer state; normal
saves retain the owner's supported entity state and restore authored animation.
Authored ragdoll mapping contributes `Judas.Ragdoll.1` under canonical fingerprint
schema 5. Existing no-ragdoll baselines remain unchanged. Generic serialization,
prefab overrides and M38 registered-asset export use the normal paths.

Editor: add Animation and Ragdoll to an ordinary mesh entity. Configure mapped
joint/parent names, shape/offset/material, filters and passive frames/limits in
the inspector. No automatic humanoid generator or new controller exists.

## Combined demo / review

`projects/ragdoll_demo/ragdoll_demo.judasproj` uses an original three-segment bar,
with a uniform-scale Stretch variant. Orange/blue share one immutable asset.
G crossfades; J toggles a tip mask; K toggles additive elbow; C changes speed;
F toggles orange ragdoll; V toggles oblique blue ragdoll; H applies a real tip
impulse; P spawns an independent prefab directly into ragdoll; Escape pauses.
The M47-only `projects/pose_demo` remains a separate internal boundary.

Full-body ragdoll is demonstrated. Partial active physical control, animation
motors, recovery, IK and M49 work are absent. This is a skeletal/physics substrate,
not a humanoid behavioural system. Human visual/physical acceptance is pending.
