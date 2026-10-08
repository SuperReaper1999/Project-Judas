# M42 collision and sensor events

**Status through M69:** M42 is operator-accepted and checkpointed. Candidate/pending
statements and measurements below record the original milestone review state,
not a current outstanding acceptance gate. Later contracts take precedence;
start with [Architecture](ARCHITECTURE.md) and [JudasJS](JUDASJS.md).

Current public JavaScript signatures, examples and lifetime rules: [JudasJS reference](JUDASJS.md). This document retains milestone architecture and evidence context.

Judas reports contact; project JavaScript decides what it means. Scene body
inspectors expose **Sensor (events, no response)** and **Collider enabled**.
These optional authored fields serialize through scenes and normal prefab
properties/overrides. Legacy bodies remain enabled, non-sensor solid bodies.
The optional `Judas.BodySensor.1` fingerprint extension identifies changed
sensor/enable metadata; existing no-sensor baselines retain their bytes.
No scene or save format version bump is needed.

## Authoritative path

PhysicsWorld observes real touching manifolds in its normal narrowphase and
actual TOI events. Speculative proximity alone does not enter a collision.
Observations are aggregated per generation-checked body pair, not per point or
compound child. Previous/current pairs produce one enter, stay or exit per
fixed step, in ascending full-handle pair order. Changing a touching pair
between solid and sensor emits an exit of the old kind then enter of the new.

Sensors use the existing tree, bilateral M39 layer/mask policy and narrowphase
at resolved fixed-step endpoints, including static sensors. They are omitted
from support, impact, position, player-blocking and fluid-boundary response.
This is discrete overlap detection: a fast object crossing completely between
endpoints may not trigger. Continuous triggers are outside M42.

The ordinary Simulation fixed step dispatches after physics/state sync and
simulation-time advance, before fidelity changes. RuntimeWorld freezes entity
identities before invoking callbacks; pair identity survives body-slot reuse.
Callbacks run per pair, first canonical body then the other, with each entity's
scripts in authored slot order. Start precedes an instance's first event.
Destroying a recipient skips its remaining deliveries; collider disable prevents
further enter/stay callbacks to it. Survivors receive exit on the next step,
with a safely invalid `other` if it was destroyed. Callback exceptions use M40
fault isolation/budgeting. No events run at render cadence. A script-spawned
body joins detection on the next physics step. World destruction clears the
physics/event state; reset clears pair history and restores authored enable state.

## JavaScript

```js
export default class {
  constructor({entity}) { this.entity=entity; }
  onTriggerEnter(event) {
    console.log('entered',event.other.id,event.other.valid);
  }
  onCollisionExit(event) { /* project behaviour */ }
}
```

Callbacks: `onCollisionEnter`, `onCollisionStay`, `onCollisionExit`,
`onTriggerEnter`, `onTriggerStay`, `onTriggerExit`.

Event fields:
- `other`: normal M40 Entity wrapper; validate `.valid` before accessing a
  destroyed entity. `.id` remains useful for identity bookkeeping.
- `point`: existing world-space manifold point.
- `normal`: existing manifold normal toward the callback recipient.
- `relativeVelocity`: other's minus recipient's point velocity at observation,
  including their existing angular velocity. It is not a post-impact estimate.
- `normalImpulse`: summed available accumulated support impulse, or `null`
  when not available (including sensor/TOI-only observations). No new impulse
  reconstruction is performed. Exit retains the last observed geometry/data.

`entity.setColliderEnabled(boolean)` changes runtime collider participation,
not authored data and not script execution. Physics queries omit sensors by
default; their existing filter accepts `includeSensors:true` explicitly.

Events cover ordinary rigid bodies (box, sphere, compound, terrain), not the
custom player's sweep-only locomotion or individual fluid particles. Coarse/
dormant entities without a PhysicsWorld body have no contact detection.
Runtime pair history is transient and is not serialized into saves.

## Demo

`projects/touch_demo/touch_demo.judasproj`: red body contacts the grey stop;
green translucent sensor detects green prefab patrols; blue patrol is in an
excluded collision category. The authored M41 HUD shows enter/stay/exit counts.
**P** spawns another ordinary prefab patrol; **R** resets. All demo reactions
and patrol/spawn behaviour are JS, not engine object-name branches.

Human checklist: collision reacts; sensor doesn't block; leaving produces exit;
blue produces no sensor event; spawned prefab behaves independently.
Human visual/input acceptance is pending.
