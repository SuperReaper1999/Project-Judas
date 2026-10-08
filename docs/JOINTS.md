# M45 — rigid-body joints

Current public JavaScript signatures, examples and lifetime rules: [JudasJS reference](JUDASJS.md). This document retains milestone architecture and evidence context.

Judas provides physical relationships between bodies. JavaScript decides what those relationships represent.

## Solver and ownership

`PhysicsWorld` owns joint records and generation-aware body references. `JointSolver` builds accumulated scalar impulse rows, alternates ten velocity iterations with the normal contact solver, and warm-starts subsequent steps. It uses body mass and world-space inverse inertia, local anchors and local frames. Constraint error becomes a velocity stabilization bias; joints never teleport poses. No-joint scenes keep the existing contact solve.

New impact islands include connected joint bodies and solve passive constraint rows alongside contact rows. Under the accepted FTFT4 policy, mechanically coupled impacts are inelastic. Event solves omit motor/spring forcing and stabilization bias; the ordinary step applies those once. Persistent contacts, events and explicit queries retain their existing paths.

## Types and coordinates

- **Fixed:** three translational and three rotational restrictions.
- **Ball/socket:** three translational restrictions; unrestricted relative rotation.
- **Hinge:** anchors coincide and frame X axes align; rotation about X remains free.
- **Slider:** relative translation is restricted to frame A's X axis; relative orientation is fixed.

Frames are body-local quaternions. There is no universal world-up. Set the frames to agree in the intended initial pose. Anchors are metres in physical body-local coordinates; visual mesh scale does not scale them.

Hinge coordinate is the signed angle of frame B relative to frame A about A's X axis, in radians, wrapped to `[-pi, pi]`. Slider coordinate is B's anchor relative to A's anchor along A's X axis, in metres. Thus positive speed with A dynamic and B fixed moves A in the negative coordinate direction.

Hinge and slider support lower/upper limits, bounded speed motors and implicit springs/damping. Motor `maxForce` is torque (N m) for hinges and force (N) for sliders. Accumulated motor impulse is bounded by `maxForce * fixedDelta`. Springs target `rest` in coordinate units; stiffness/damping use the corresponding linear/angular units. Limits take priority in row order. A limit that is already violated (for example one enabled while the coordinate is outside its range) is corrected at no more than 0.5 m/s for sliders or 0.5 rad/s for hinges, rather than rebounding with a speed proportional to the violation. This is an iterative real-time solver, not an exact articulated dynamics solution.

## Authoring and lifetime

Add **Joint** in the entity inspector. Choose Body A, Body B or World anchor, anchors, frame rotations, type and settings. A separate owner entity can hold the joint. With Body B absent, anchor B and frame B are relative to the owner's authored transform at construction; they become a fixed world anchor. Use a static body reference if a body-local static anchor is desired.

Body participants remain Full fidelity while active. Disabling a participating collider deactivates its joints; re-enabling reactivates them. Destruction retires joints, and recycled body slots cannot revive handles. Teleports/enable changes clear warm impulses. Destroying an authored participant removes dangling joint components from the scene. Runtime owner destruction is synchronized at fixed script boundaries.

Prefab source IDs remap through the normal stable instance mapping. Joint settings use generic serialized-property overrides. Runtime `SpawnPrefab` creates ordinary bodies and joints after the complete hierarchy exists.

Canonical fingerprint schema remains **5**: a tagged `Judas.RigidJoints.1` extension includes all joint settings and references only for scenes containing joints. Existing no-joint fingerprints remain unchanged. Created runtime joint definitions use ordinary world-state entity serialization; references are preflighted before mutation. Under the original M45 legacy-save contract, runtime motor configuration and warm impulses were transient. Modern M61 slots preserve runtime enabled/limits/motor/spring settings and references; numerical solver caches are rebuilt. See [current save contracts](M61_SAVES.md).

## JavaScript

```js
import {physics, world} from 'judas';
const joint = physics.joint(world.entity('31')); // owner entity, null if unavailable
if (joint?.valid) {
    joint.setEnabled(true);
    joint.setLimits(-0.8, 0.8);
    joint.setMotor(-0.5, 15);       // speed, maximum force/torque, enabled=true
    joint.setSpring(0, 15, 4);     // rest, stiffness, damping, enabled=true
    console.log(joint.state.coordinate);
}
```

Handles have monotonically assigned runtime identities. `valid` returns false after retirement; other stale operations throw a safe JS reference error. Invalid settings throw without applying changes. State exposes `active`, `enabled`, `coordinate`, `motorImpulse` and numeric type (fixed=0, hinge=1, ball=2, slider=3). Coordinate is sampled when constraint rows are prepared; it is not an extra geometric query after every pose change.

## Demo and scope

Open `projects/joint_demo/joint_demo.judasproj`. From left to right: fixed dynamic pair with support, free hinge pendulum, limited motor-driven physical door, ball pendulum, motor/spring slider. Rear: oblique hinge. **G** reverses motors; **P** spawns an independent fixed prefab assembly; **Esc** pauses. The door is an ordinary dynamic body + hinge + project JS. Historical M16 kinematic doors and evidence remain untouched.

Limitations: hinge limits must stay within the wrapped angle range; initially opposed hinge axes and very large joint errors are not suitable authoring configurations. Extreme mass ratios, large articulated stacks and stiff motors may require smaller steps or future solver improvements. No breakable joints, gear trains, ropes or kinematic trajectory redesign (M45 scope; articulated ragdolls arrived in M48, see [RAGDOLLS.md](RAGDOLLS.md)). Visual/physical acceptance belongs to the human operator.

## Explicit passive rotational resistance

`JointConfiguration.rotationalResistance` (and the ordinary joint/ragdoll mapping inspector field) is a nonnegative viscous coefficient in N·m·s/rad. Default **0** preserves existing content. Ball joints resist all three free angular axes; hinges resist their free axis. It is solved implicitly alongside contacts and constraints, has no target pose, and does not force sleep. Fixed/slider joints do not use it. The post-M65 Skate adaptation authors 0.05 on the free head and right-foot joints; the unchanged rig and its continued-motion observations remain in the original review and corrective baseline evidence.

[Corrective review and measurements](POST_M65_CONSUMER_REPAIRS.md).
