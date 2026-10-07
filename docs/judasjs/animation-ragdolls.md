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
force/impulse/query use. No built-in death/recovery/balance or pose-driving motors.

Mappings and constraints are authored via ordinary scene/editor/prefabs; JS does
not construct mappings. M45 joints/normal PhysicsWorld own articulation, M47
resolves its physics contribution into the same skeleton. Owner translation
follows physical root; reference orientation/scale remain fixed. Mapped body
collisions are per-body, not owner-aggregate. Asset replacement/destruction/disable
cleans articulation; mapped transient entities are not independently saved.
**Historical M46–M60 persistence limitation:** active ragdoll and mixer state were not persisted by legacy deltas. [M61 slots](saves.md) now preserve active articulation and mixer state through the shared pose/physics path. Uniform positive mapped scale
only, no owner rigid collider; partial active physical control is not provided.
See [mapping/lifecycle limits](../RAGDOLLS.md).

## Resolved joint reads, visual sockets and limb IK (M65)

`animation.jointTransform(key, space='world', presented=false) → Transform|null`
reads the final M47-resolved pose, including layers/IK/M48 physics. Missing/unready
joints return null; stale entity throws. Space is local (parent-relative), model
(skeleton-relative), or world (composed entity pose). Invalid space throws TypeError.
In presentationUpdate, presented=true interpolates the entity root exactly as the
renderer does; joint-local samples are the actual currently evaluated skinning pose,
with no invented bone interpolation. Simulation reads never use presentation roots.

`animation.limb(id, settings) → boolean` adds/patches a two-bone contributor:
root/middle/end stable keys, world target/pole Vec3 positions, weight 0..1, enabled,
order 1..999. Up to16, sorted (order,id), after clip contributions and before physical
pose at1000. Direct hierarchy and positive uniform chain scale are required.
`removeLimb(id)` retires it. Invalid settings throw; unavailable asset returns false.
Unreachable targets preserve bone lengths and retain an honest endpoint error;
straight/folded chains use the input bend or deterministic model-space fallback.
No foot orientation, whole-body balancing or dynamic bone-driving is promised.
Authored/runtime settings persist; solver caches do not.

`entity.setSocket(target, joint, offset={}) → true` assigns a visual-only attachment.
Target is a safe animated Entity; null removes the socket. Offset is partial local
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
