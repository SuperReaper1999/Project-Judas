# Animation, pose layers and articulated ragdolls

[Index](../JUDASJS.md) · [Physics/joints](physics.md) · [Character](character.md)

Judas owns skeleton/pose infrastructure. Clips, physics and future producers
contribute to a resolved pose; no producer owns bones permanently. JS controls
WHY playback/physical transitions occur; it cannot access raw pose arrays or GL.

## Animation

`entity.animation` returns a facade/null. Its `entityId` is a plain opaque field.
`info` returns `{ready,playing,loop,speed,time,clip,clips,transitioning,
transitionFraction,error,joints,layers}`. `clips` are `{name,duration}` (seconds);
`joints` are stable hierarchy keys. `layers` contain only
`{id,clip,weight,enabled,additive}`, not all settings. Empty clips/error while async
asset loads: check ready. A facade's existence doesn't imply successful mesh import.

| Member | Behaviour |
|---|---|
| `clips`, `playing`, `time`, `layers` | Convenience detached info reads. |
| `speed`, `loop` | Read/write finite speed and boolean; speed may be negative (existing playback supports reverse). |
| `play(clip='')` | Named clip resets time/mixer; empty string resumes current playback. Unknown ready clip throws. |
| `pause()` | Pauses playback/mixer, true even if asset not ready. |
| `resume()` | Alias for `play()` with empty clip. |
| `stop()` | Reset through existing playback stop/rest pose; clears mixer. |
| `seek(time)` | Finite nonnegative seconds; sampling handles clip wrap/clamp. |
| `crossFade(clip,seconds=.3)` | Existing weighted shortest-path quaternion mix; interrupted contributors remain deterministic, zero duration immediate. Duration 0..3600. |
| `layer(id,settings)` | Add/update an ordered contribution; patch existing settings. |
| `removeLayer(id)` | Remove if present; true even absent when asset ready. |

Except pause/settings, commands return false while asset unavailable (not a queued
command). Validity checks still apply. Unknown clips/masks/settings throw once
ready. Up to 16 authored/runtime layers and 16 interrupted fade contributors.

`AnimationLayerPatch`: `clip`, `referenceClip`, `weight` [0,1], `speed`, `time`,
`referenceTime`, `enabled`, `additive`, `mask` (up to 4096 hierarchy keys). Missing
fields retain existing settings, or defaults for a new layer. New layers need a
valid clip; empty mask affects all joints. Masks affect local joints, not automatic
subtree expansion; parents/children compose normally. Updating the same clip
preserves its playback clock (including when supplying time); do not promise a
separate layer seek API. Enabled layers advance with the mixer; base loop/seek
controls do not automatically control each layer's clock.

Ordinary layers blend absolute sampled poses in insertion order. Additive layers
use explicit reference clip/time or rest pose, translation/rotation/scale delta;
not an arbitrary absolute clip treated as an offset. Stable joint keys aren't
humanoid assumptions. C++ exposes validated external sources used by ragdolls;
**no arbitrary external pose source/raw bone setter exists in JS**.
[Pose implementation/authoring](../ANIMATION.md).

## `Animation.rootMotion(clip, from, to, loop=false)`

Queries a named clip's extracted track independently of current playback/mixer.
Returns `{translation, rotation, extracted}` in model-local motion coordinates,
relative to the motion frame at `from`. Seconds may be negative; non-loop queries
clamp to the clip, loop queries compose whole cycles rather than teleporting back.
Finite times are bounded to +/-1e9 seconds and one million cycles. Unknown clip
returns null; unavailable animation or stale entity throws. A preserved/in-place
clip returns identity motion with `extracted:false`.

This does not sample a blended mixer's displacement and never moves an entity,
rigid body or CharacterMotor. Rotate the returned local translation by the
character's orientation before using it as motor intent. Collision resolution
still belongs to the motor. [Import/root policies](../MODEL_IMPORT.md).

## Ragdoll

`entity.ragdoll` returns facade/null for authored mapping. `id` is its opaque owner.
`active` boolean reports articulation. `enter()` activates from CURRENT resolved
pose, recent observed motion where available, returns true or throws. `leave(seconds=.4)`
captures physics pose, retires bodies/constraints, and visually fades back to
animation; duration 0..3600, zero immediate. `enabled` is setter-only (read undefined).
`body(jointKey)` returns mapped ordinary Entity or null; check validity before
force/impulse/query use. No built-in death/recovery/balance. The separate M70 API below adds bounded physical pose drives.

Mappings and constraints are authored via ordinary scene/editor/prefabs; JS does
not construct mappings. M45 joints/normal PhysicsWorld own articulation, M47
resolves its physics contribution into the same skeleton. Owner translation
follows physical root; reference orientation/scale remain fixed. Mapped body
collisions are per-body, not owner-aggregate. Asset replacement/destruction/disable
cleans articulation; mapped transient entities are not independently saved.
**Historical M46–M60 persistence limitation:** active ragdoll and mixer state were not persisted by legacy deltas. [M61 slots](saves.md) now preserve active articulation and mixer state through the shared pose/physics path. Uniform positive mapped scale
only, no owner rigid collider. The legacy enter/leave API remains available; M70 adds explicitly selected physical regions below.
See [mapping/lifecycle limits](../RAGDOLLS.md).

### `Ragdoll.receiveContactEvents`

Read/write boolean, default **false**. Opt an owner script into the existing M42
collision/trigger callbacks for its mapped bodies' contacts with external bodies:
`entity.ragdoll.receiveContactEvents = true`. An owner with unscripted transient
bone entities must both enable this flag and implement the callbacks on its own
script to observe a bone hitting ordinary unscripted ground. Alternatively author
`ragdoll.receive-contact-events true` through the normal named scene/prefab format.
This subscription is independent of animation/partial/active/passive pose authority.

Delivery preserves one event per actual body pair/phase; it does not aggregate
the whole articulation into one impact or assign gameplay meaning. `selfBody`
identifies the mapped body and `selfJoint` its stable joint key; `other` remains
the actual other body entity. `otherArticulation`/`otherJoint` identify an external
articulation when applicable. Wholly internal same-owner contacts are not forwarded
to the owner. Scripts on individual bodies retain ordinary M42 delivery unchanged.

The current subscription is checked when observations are delivered. Toggling it
does not create synthetic enter/exit events or reset contact history: enabling it
while a pair is already touching can first deliver `onCollisionStay`. Nonboolean
writes throw TypeError. A stale owner throws ReferenceError; constructing a
facade for a live entity without a ragdoll mapping does not create one and access
throws TypeError. Event entity wrappers can become stale during delivery; test
their `valid` fields before using them. See [contact timing and fields](physics.md#collision-and-trigger-callbacks).

```js
// Methods on the script attached to the articulation owner:
start() {
  this.entity.ragdoll.receiveContactEvents = true;
}
onCollisionEnter(event) {
  if (!event.selfBody?.valid || !event.other?.valid) return;
  this.state.lastContactJoint = event.selfJoint; // durable string/null, not a handle
  this.state.lastContactBody = event.other.id;
}
```

## Resolved joint reads, visual sockets and limb IK (M65)

`animation.jointTransform(key, space='world', presented=false) → Transform|null`
reads the final M47-resolved pose, including layers/IK/M48 physics. Missing/unready
joints return null; stale entity throws. Space is local (parent-relative), model
(skeleton-relative), or world (composed entity pose). Invalid space throws TypeError.
In presentationUpdate, presented=true interpolates the entity root exactly as the
renderer does. With M70 enabled, joint-local poses interpolate the previous/current
fixed final pose too, matching sockets/skinning for that same presentation sample.
Legacy disabled-M70 consumers retain their existing evaluated joint-local sample. Simulation reads never use presentation roots.

### Legacy two-bone position IK: `Animation.limb`

`animation.limb(id, settings) → boolean` adds/patches a two-bone contributor:
root/middle/end stable keys, world target/pole Vec3 positions, weight 0..1, enabled,
order 1..999. Up to 16, sorted (order,id), after clip contributions and before physical
pose at order 1000. Direct hierarchy and positive uniform chain scale are required.
`removeLimb(id)` retires it. Invalid settings throw; unavailable asset returns false.
Unreachable targets preserve bone lengths and retain an honest endpoint error;
straight/folded chains use the input bend or deterministic model-space fallback.
This legacy API is position-only: `target` and `pole` are `{x,y,z}` objects, and
it has no target-orientation field. For palm/sole orientation and coupled multiple
effectors, use [M70 shared IK](#multi-target-full-body-ik-m70), whose targets accept
both position and orientation. Shared IK still provides no automatic balancing or
dynamic bone-driving; physical drives are a [separate capability](#partial-physical-animation-m70).
Authored/runtime settings persist; solver caches do not.

### Visual sockets

`entity.setSocket(target, joint, offset={}) → true` assigns a visual-only attachment.
Target is a safe animated Entity. Use `entity.clearSocket() → true` to remove the
attachment; `setSocket(null, ...)` is invalid and throws TypeError. Offset is partial local
position/rotation/scale relative to that joint. Reads are ordinary transform /
presentedTransform. Missing joints/invalid cycles/conflicting physics ownership
throw; no fallback to guessed clip or bind transforms. Target-first evaluation
handles chains, and normal prefab/stream/save reference remapping applies. Socket
owners cannot have bodies/motors/ragdolls/deformables: use physics joints for an
actual physical attachment. Asset replacement makes unavailable reads safely null.

```js
presentationUpdate() {
  const hand = this.entity.animation?.jointTransform('Hand', 'world', true);
  if (hand) this.marker.transform = {position: hand.position};
}
```

[Integration project and authoring](../M65_INTEGRATION.md) shows two independent
foot targets on a tilting board and a hand socket through interrupted crossfades
and ragdoll output. Scripts no longer need transitionFraction to estimate bones.

### Imported joint frames and observed motion

The editor's stable-key skeleton picker and rest-pose axis overlay show each imported
joint's **local** axes. M45 hinge/slider frame X is the constraint axis; orient the
frame to the intended imported joint axis rather than assuming humanoid names or Y.
The picker is not an automatic physical mapping/fitter. See [authoring](../M65_INTEGRATION.md).

Ragdoll entry uses recent evaluated world-joint samples and their simulation sample
interval when available, plus entity motion. Body-free scripted transform changes
between samples therefore contribute observed motion; they are not proof of a
continuous physical trajectory. Presentation-only placement should not be treated
as authoritative velocity. Large/teleported deltas need project policy; M65 does not
add a get-up controller or change the existing motion-inheritance model.

## Multi-target full-body IK (M70)

`animation.configureIK(settings|null) → boolean` validates one complete configuration
before publication. Null removes it. `animation.ikTargets(targets) → boolean` replaces
all targets atomically; omitted targets are removed. Both throw TypeError for invalid
configuration or unavailable skeleton; these are not arbitrary raw-bone setters.
New configuration/targets become effective at the next fixed reference boundary.
`animation.ikStatus` is a detached latest-completed solve snapshot: `ready`, `enabled`,
`diagnostic`, `converged`, `iterations`, `solveMicroseconds`, `rootCorrection`, `targets`.
Before the first configured reference phase, targets are empty and converged is false.
`ready` means the model resource exists, not that every authored mapping/target is valid.

### Configuration tuples versus live vector objects

The M70 batch configuration deliberately uses the same serializable tuple format
as its authored JSON: vectors are ordinary arrays `[x,y,z]` (`PoseVector`), and
quaternions are `[x,y,z,w]` (`PoseQuaternion`). These fields do not accept ordinary
`{x,y,z}` / `{x,y,z,w}` objects interchangeably. All components must be finite;
vector/quaternion arrays have exactly three/four components. Each indexed
validation error identifies the invalid target, chain or limit; no partially
accepted target batch is published.

| API / data | Spatial value format |
|---|---|
| `configureIK`: root bounds; joint-limit min/max/preferred; target position/offset | `[x,y,z]` |
| `configureIK`: joint-limit frame; target orientation/frame | `[x,y,z,w]` |
| `ikTargets`: position/offset and orientation/frame | The same three/four-component arrays |
| `ragdoll.setMode`: `placement.position` / `placement.rotation` | The same three/four-component arrays |
| `ragdoll.configurePhysical` | Region joint keys and scalar settings; no spatial vector/quaternion fields |
| `jointTransform`, `ikStatus.rootCorrection`, target `actualPosition` / `actualOrientation` | Ordinary detached `Vec3` / `Quat` objects |
| Ordinary transforms, motor velocity, physics vectors and legacy `limb` target/pole | Ordinary `Vec3` / `Quat` objects, as their signatures specify |

This exception does not change the [ordinary transform API](entities.md#entity).
Convert a snapshot explicitly when submitting a batch, for example after configuring
the `contact` chain (the marker is an ordinary project entity):

```js
// In fixedUpdate: choose an authoritative marker frame, not a presented pose.
const t = this.contactMarker.transform; // position/rotation are objects
this.actor.animation.ikTargets([{id:'palm',chain:'contact',space:'world',
  position:[t.position.x,t.position.y,t.position.z],
  orientation:[t.rotation.x,t.rotation.y,t.rotation.z,t.rotation.w],
  orientationWeight:1}]);
// Results remain objects, e.g. this.actor.animation.ikStatus.rootCorrection.x.
```

| Configuration | Meaning/default |
|---|---|
| `enabled` | Enable this shared pose solve, default true; disabled targets report disabled status. |
| `bodyRoot` | Required stable joint key; all chains descend from this skeletal root. |
| `rootMin`, `rootMax` | Model-space root correction bounds, metres; default ±0.2 on each axis. Must contain zero; each bound is at most 10 m. |
| `rootRotation` | Allow body-root rotation participation, default true. False disables it even when a chain names the root. |
| `spine` | Explicit participating ancestor joint keys; default empty. No inferred anatomy. |
| `chains` | Required `{id,joints:[root,…,effector]}` mappings. Ordered descendant paths; intervening unmapped helpers are retained. |
| `limits` | `{joint,frame?,min?,max?,preferred?,preferenceWeight?}`. Local rotation-vector radians relative to imported rest rotation in declared quaternion frame. Defaults frame identity, min/max ±π, preference zero/weight zero. |
| `targets` | Optional initial target array; default empty. |
| `iterations` | 1..96, default 64. A shared bounded solve, not one independent solver per limb. |
| `damping` | Positive numerical DLS damping ≤1, default .02; not a gameplay transition timer. |
| `positionTolerance`, `orientationTolerance` | Metres/radians, defaults .005 and .017453293 (one degree). |
| `orientationScale` | Metres-per-radian objective scale, default .25. |
| `maxAngularStep`, `maxTranslationStep` | Per-iteration radians/metres, defaults .22/.06; both bounded at 1. |

Each target is `{id,chain,enabled?,space?,position?,orientation?,positionWeight?,
orientationWeight?,offset?,frame?}`. IDs are unique and stable. Defaults: enabled,
model space, zero position, identity orientation, positionWeight 1, orientationWeight 0,
zero effector offset, identity effector frame. Weights are independent 0..1; zero
ignores that constraint. Position-only, orientation-only and combined targets work.
The offset/frame is the palm/sole/contact frame in the final effector's local space,
not necessarily the imported joint origin/axes. World targets use ordinary Judas
world coordinates: the fixed-origin local simulation frame used by entity transforms,
joint reads and physics queries, not absolute astronomical coordinates. Model targets
are relative to the entity's model transform.
World conversion subtracts the entity origin before the float solve. Positive uniform
entity/mapped-path scales are supported; singular/sheared/reflected participating
paths are diagnosed. Unrelated hidden zero-scale branches do not require inverses.

At most 12 chains/targets and 64 participating rotation joints. Position residuals
and quaternion-log angular residuals are weighted least-squares constraints solved
simultaneously. Target IDs sort deterministically; competing equal-weight constraints
receive equal objective weight. Joint/root bounds are projected with bounded monotone
line search. An explicit preferred bend supplies a small initial seed when correction
is needed; it is not an automatic gait or a guessed universal knee/elbow direction.

A target status is `reached`, `limited` or `disabled`, with `positionError` in metres,
`orientationError` in radians, and `actualPosition`/`actualOrientation` in model space.
Targets are value frames, not references that automatically track/pin another Entity.
A script forming support-relative targets checks that safe handle and removes the
corresponding intent with the next atomic batch when support disappears.
Disabled fields still validate. An unreachable/conflicting target gives a finite
bounded compromise and residual; it does not stretch bones, claim success or move
an entity. `rootCorrection` is skeletal pose correction only: no motor capsule,
root-motion track, support frame or dynamic body is secretly displaced. Each solve
starts from the current reference, so previous correction does not accumulate drift.

### Reference/final phase and composition

M70 owners retain `CharacterMotor` as locomotion authority. In a fixed step:

1. Project fixedUpdate writes motor intent and target/configuration requests.
2. Opted-in motor motion resolves; clip/crossfade/layers and existing ordered limb IK
   produce the reference pose. New shared IK solves that reference once.
3. Physical pose drives sample the solved reference before normal PhysicsWorld steps.
4. Actual physical contributions resolve after IK; final pose feeds sockets/joint reads
   and skinning. Presentation interpolates output and never feeds drive forces back.

Ordinary layers retain insertion order. Existing limb/external sources retain their
`(order,id)` order below physical order 1000; the shared IK stage follows them. Physical
sources at/above 1000 follow the new stage, so IK cannot overwrite their actual pose.
M70 disabled preserves legacy ordering/semantics. No solve occurs per camera.

Target writes made by a callback cannot make an earlier joint read see future physics.
If JS constructs a target from its own solved joint/socket, that is an explicit
previous-sample feedback choice; no native implicit dependency graph is evaluated.
Use fixedUpdate for authoritative targets. In presentationUpdate, request
`jointTransform(key,'world',true)` to compare to the same displayed pose.

```js
// Project chooses the contact; the engine owns the shared geometric solve.
actor.animation.configureIK({
  bodyRoot: 'Pivot', rootMin: [-.2,-.2,-.2], rootMax: [.2,.2,.2],
  spine: ['Vertebra0','Vertebra1'],
  chains: [{id:'contact', joints:['ShoulderA','ElbowA','PalmA']}]
});
actor.animation.ikTargets([{id:'palm',chain:'contact',space:'world',
  position:[1,1.8,-.2],orientation:[0,0,0,1],orientationWeight:1,
  offset:[0,0,.07]}]);
```

Mapping names here belong to the [Character Lab](../M70.md), not engine conventions.
Use `animation.info.joints` and the normal skeleton picker for another imported model.
Existing `crossFade`, `layer`, `limb` and visual sockets remain composable.

## Partial physical animation (M70)

`ragdoll.configurePhysical(settings|null) → boolean` assigns/removes settings on an
existing authored ragdoll mapping. It does not construct a second skeleton or physics
world. Settings are `{enabled?:true,regions:[…]}`; up to eight named regions.
Each region has required `id`, `joints` (1..32 existing mapped keys), optional `enabled`,
`stiffness` (default 30 N m/rad), `damping` (4 N m s/rad), `maxTorque` (20 N m magnitude),
`effortWeight` (1), `poseWeight` (1). Stiffness/damping/torque bounds are
10000/1000/10000; weights are 0..1. Later enabled region wins overlaps.

Drive targets are relative orientations derived through authored bone/body/COM offsets
and existing joint frames from the separate fixed animation/IK reference. Bounded
implicit PD torques affect normal rigid bodies; normal inertia, gravity, collisions,
joint limits and reaction torques decide the actual result. No dynamic-body teleport,
velocity replacement or hidden full-body root pinning is supplied.

The implicit response retains the full inertia tensor, including coupled axes;
effort is still capped by the authored `maxTorque`. Unchanged references permit
ordinary physics sleeping, which can retain a small residual pose error rather
than exact servo convergence. Meaningful target changes and physical impacts
wake the island through the normal physics path.

`effortWeight=0` removes active pose-restoring effort; constraints, authored passive
resistance and physical materials remain. `poseWeight` blends the simulated skeletal
contribution visually; a value below one is an explicit approximation whose displayed
pose can differ from collision geometry. Inspect `ragdoll.body(key).transform` for the
actual body and `physicalState.drives` for target error/saturation. A drive does not
imply that the body reached the requested pose. No balance/get-up controller exists.

`ragdoll.setMode(mode,options={}) → boolean` queues an explicit authority request;
validate before publication, apply at the safe fixed boundary. Modes:

| Mode | Authority |
|---|---|
| `animation` | Ordinary animation/IK; physical bodies retire through a captured pose return. |
| `partial` | Selected regions dynamic/driven, remaining mapped bodies are animated kinematic boundaries; motor may keep locomotion. |
| `active` | Fully dynamic articulation, selected region drives active; root remains free. |
| `passive` | Same dynamic articulation/velocities with active effort removed. |

Options: `fade` seconds (default .2, 0..3600), `motorHandoff` (default false),
`resumeMotor` (false), and optional `placement:{position:[x,y,z],rotation:[x,y,z,w]}`.
Placement uses the [configuration tuples](#configuration-tuples-versus-live-vector-objects),
not a `TransformPatch` object with `Vec3`/`Quat` fields.
The `fade` option controls only the captured return toward animation, not active
drive ramping. Change `effortWeight` and `poseWeight` independently in fixedUpdate
when a project wants separate effort/presentation fades. Unselected partial-mode
bodies are non-colliding, non-querying kinematic boundaries; selected bodies retain
normal authored filtering. A fully physical request must explicitly relinquish a
conflicting motor authority. `resumeMotor`/`placement` apply only when requesting
`animation`; they use endpoint/recovery/path checks from the normal motor collision
contract and can be refused;
inspect `physicalState.diagnostic`. No automatic recovery alignment or get-up selection.
`physicalState` returns `{mode,pending,diagnostic,drives}` for M70 authority; pending
is requested mode or null. Legacy `enter()`/`leave()` consumers use `ragdoll.active`: an
articulation created without M70 configuration still has a default animation mode
snapshot and does not acquire M70 authority semantics. Each drive reports `{joint,region,angleError,torque,saturated,sleeping}`:
angleError radians, torque N m. Configuration/mode failures throw TypeError; a later
boundary refusal leaves the prior coherent mode and records its reason.

A motor-driven kinematic boundary can exchange externally supplied work; its reaction
is not guaranteed to push the motor. Dynamic-to-dynamic reactions retain normal solver
semantics. Existing filters, queries and collision callbacks still see actual bodies.
`ContactEvent.selfBody`/`selfJoint` identify the receiving mapped body/key;
`otherArticulation`/`otherJoint` identify the other articulation/key when applicable.
For ordinary contacts, `selfBody` identifies the receiving entity; the articulation-specific fields are null. A destroyed body can leave an Entity wrapper with `valid=false`; unavailable identities are null. Owner forwarding requires the explicit [receiveContactEvents subscription](#ragdollreceivecontactevents). This is attribution, not an injury/event-gameplay system.

```js
actor.ragdoll.configurePhysical({regions:[{id:'arm',
  joints:['ShoulderA','ElbowA','PalmA'],stiffness:30,damping:4,
  maxTorque:12,effortWeight:1,poseWeight:1}]});
actor.ragdoll.setMode('partial');
// Later project decisions, not built-in bail states:
actor.ragdoll.setMode('active',{motorHandoff:true,fade:.2});
actor.ragdoll.setMode('passive',{fade:0});
actor.ragdoll.setMode('animation',{resumeMotor:true,fade:.25,
  placement:{position:[0,.05,0],rotation:[0,0,0,1]}});
```

Activation uses authoritative fixed reference/motion samples; first activation without
history uses existing owner motion and no invented preceding bone velocity. Active →
passive preserves velocities and does not replay an activation impulse. Asset replacement,
disable/destroy, streamed unload/adoption and scene transitions invalidate mappings/history.
Declared IK targets/physical settings and authority state participate in normal named
scene/prefab/save paths; disposable solve caches and GPU matrices are not serialized.
[Save ownership](saves.md) · [CharacterMotor](character.md) · [Physics](physics.md).
