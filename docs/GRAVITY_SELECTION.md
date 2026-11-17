# Per-entity gravity selection

Follow-up to checkpoint `0f3694bb7142dc38a59abbd7365a113b673bae5e`.
On 2026-10-09 the operator marked human validated and authorized this gravity
API follow-up for commit/push. Existing automated evidence remains unchanged;
this authorization does not claim additional gravity-demo or Windows human tests.

## Default remains spatial

Absent `SceneObject.gravitySelection` means ordinary position-based gravity.
Existing scenes/projects require no migration and keep their canonical fingerprints.
FaithfulGravity/uniform and RadicalGravity remain ordinary GravityField implementations.
Gravity, support normal, attachment, reference frames and cameras remain separate.

The optional owner intent is either:

- **Uniform**: explicit finite world-space acceleration `{x,y,z}` in m/s². Zero is valid; magnitude is bounded at 10000.
- **Field**: stable existing authored gravity-source Entity identity. The source's actual field samples at the consumer position, independently of region bounds. RadicalGravity remains position-dependent.

`RuntimeWorld::SampleEntityGravity` is the shared resolution path. It looks up the
current source rather than keeping a durable native pointer. Sources' own selections
are not followed, so chains/cycles cannot replace field sampling. Missing/unpublished
source gives spatial fallback and `available:false`. Changing selection preserves
world velocity; there is no teleport, scripted force repair or automatic camera change.

## Public API

See [Entity.gravity reference](judasjs/physics.md#entitygravity--entitygravity),
[declarations](judas.d.ts) and [executed example](judasjs/examples/gravity-selection.js).

```js
this.entity.gravity.select(existingGravitySource);
this.entity.gravity.setUniform({x:-9.81,y:0,z:0});
const acceleration = this.entity.gravity.acceleration;
this.entity.gravity.clear(); // restore applicable zones at the current position
```

Selection chooses a field/acceleration. JS decides when and why. A wall-walking
project can raycast a real surface, choose acceleration opposite the hit normal,
and independently follow the motor's presented orientation with its camera.

## Consumers and limits

CharacterMotor samples the selected field for acceleration and its existing
bounded up reorientation. Support normals remain collision observations. Its
`gravityScale` still scales acceleration; it does not change selection. In zero
acceleration, the existing last usable orientation reference is retained.

Ordinary full/coarse local-gravity rigid bodies, gravity-enabled visual particles
and non-rigid deformables sample their owner intent. Kinematic bodies retain
prescribed motion. Selection is not implicitly inherited by children, bones or
an entire ragdoll; individually owned bone bodies may select explicitly.

Position-only `physics.gravity(position)` uses the spatial resolver; independent
Newtonian/celestial pair forces retain their explicit pairwise ownership. Entity
selection does not create a gravity source or move authored fields.

## Authoring and lifecycle

Inspector **Add Component → Gravity selection** exposes uniform acceleration or
an existing gravity source. Remove the component to restore spatial routing.
Sources are normal Gravity region entities; no new asset or identity format.

The optional fields serialize normally in scene/named-scene/prefab data. Existing
prefab, duplication, template and region remapping remap source identities.
Modern saves capture runtime intent and rebuild sources in a fresh world. Retained
streamed regions restore references using fresh runtime identities. External
selection pins a source region until cleared; it does not grant attachment.
Stop/reload restores authored settings. Stale JS owners/sources fail safely;
invalid writes are atomic and identical writes do not repeatedly wake bodies.

Canonical fingerprint schema remains 5: only non-default selections contribute a
new tagged extension. No save archive participant/version was added. Uniform
vectors are explicitly world-space; if content is relocated into a rotated frame,
its author must express that intent in the target world frame.
