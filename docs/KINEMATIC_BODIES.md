# Kinematic rigid bodies

A kinematic body follows an explicit prescribed trajectory through the ordinary
MK2 world. Scripts author motion intent; collision response can impart work to
dynamic neighbours without giving those neighbours authority over that path.

## Ownership and fixed-step scheduling

| Phase | Owner / visible sample |
|---|---|
| Frame scripts / `fixedUpdate` | JS queues a validated pose or velocity command. Last write wins. Actual body reads still return the preceding resolved state; command readback is separate. |
| Existing IK/physical-animation motor/reference prephase | Retains its accepted preceding-resolved support sample. Legacy animation boundary placement remains explicit placement, not reinterpreted as a new motion command. |
| Start of `PhysicsWorld::Step` | Physics publishes one prescribed trajectory from the current COM/orientation and queued command. Finite-mass neighbours retain normal forces/gravity. |
| Contacts / continuous impacts | The existing broadphase/narrowphase/solver samples that trajectory at authoritative times. Infinite-mass prescribed state supplies external work; dynamic neighbours receive impulses. |
| End of rigid step / ordinary motors | Physics resolves the endpoint. The existing support/history contract carries translation/rotation; support velocity is evaluated about the actual COM. |
| Body sync / final poses / collision-event callbacks | Normal world state and final contact observations are published; safe body identity remains stable across supported authority changes. |
| Presentation | Interpolates resolved samples; creates no commands, motion history or collision state. |

Static, dynamic and kinematic authority are explicit. Prescribed motion is not
collision-free routing: static/kinematic obstacles do not automatically stop its
trajectory, and trapping requires project policy. Explicit transform placement
remains a separate operation. No world-up, camera, vehicle or gameplay policy is
introduced. Measured candidate results and remaining limits follow below.

## Authoring and runtime controls

`PhysicsWorld` explicitly separates motion authority from inverse mass. Existing
contact/impact solvers, broadphase, motion segments, filters and support queries
consume one prescribed path. Kinematic pose sampling uses constant world angular
velocity, including intermediate CCD samples; dynamics retain their existing
integration. Point velocity uses the actual COM, including compound offsets.
The represented-rotation check preserves IK/physical-animation's unchanged/sub-ULP correction.
Large absolute positions retain the existing double origin plus local float
representation; small offsets are formed before narrowing.

Commands publish at the next rigid step. A complete target consumes the next
fixed interval by default, or a positive duration up to 60 seconds, then holds.
Velocity control persists until replaced or retired. The last valid complete
write wins; invalid requests preserve the previous command. Targets interpolate
COM translation and the shortest quaternion arc; continuous spins use angular
velocity. Gravity and impulses do not own prescribed pose. Supported moving
colliders are sphere, box, convex and compound; concave mesh, radial terrain and
query-only capsule configurations are rejected for prescribed authority.

Normal runtime entities expose motion type, safe authority changes, complete
pose targets, COM/angular velocity commands, stop, detached intent and actual
point velocity. Use `fixedUpdate` for policy. Presentation command attempts are
rejected. Existing transform setters retain explicit placement semantics.
Authority changes preserve the body handle when supported, clear velocity and
commands by default, and allow an explicit transfer of actual velocity. Managed
articulations, joint participants and conflicting component authorities reject
generic transitions. See the exact [JudasJS contract](judasjs/entities.md#kinematic-motion)
and [executed cookbook](judasjs/examples/kinematic.js).

The normal inspector, undo, named authoring, scene serializer, prefabs and modern
save slots share this authority. Initial angular velocity is optional with zero
default. A conditional `kinematic-motion` save participant retains command and
physical state; disposable history restores empty. Regional retention uses the
existing streaming archive extended to version 4. Retained target/stopped state
replaces authored initial velocity at publication. Legacy `.judasstate` explicitly
refuses kinematic or changed authority and directs the consumer to modern slots.
Unchanged historical scene fingerprints are retained.
