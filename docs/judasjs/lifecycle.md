# Lifecycle, modules and script properties

[Index](../JUDASJS.md) · [Lifetime/state](scenes-state.md) · [Practices](practices.md)

## Module model

Registered `.js` assets are ES modules executed by QuickJS-NG v0.17.0. Import
`'judas'` for engine primitives. Relative `./` and `../` imports must resolve to
registered script assets. Other bare imports, absolute paths, Node and browser
modules are unavailable. Source modules are limited to 1 MiB. Modules are cached
once per world's VM; module globals are shared between instances in that world.
A new scene/reload/Play creates a fresh VM, not a retained module heap.

Default-export a constructible class. Each enabled authored slot creates its own
instance with `{entity, properties, restored, initialState}`. `restored` is true for modern
save restoration. `initialState` is bounded prefab construction data or null;
it is not automatically copied into `this.state`. See [spawn construction](entities.md#worldspawnprefab-construction-options-m67).
Slot IDs are stable identities, distinct
from order. The constructor runs before restored `state` is installed. Do not
make constructor side effects depend on saved state.

```js
export const properties = {
  rate: {type: 'number', default: 2},
  active: {type: 'boolean', default: true},
  label: {type: 'string', default: 'Counter'}
};
export default class {
  constructor({entity, properties}) {
    this.entity = entity;
    this.props = properties;
    this.state = {count: 0};
  }
  start(dt) {} // new-game or legacy-delta startup
  restore(dt) {} // modern slot load; saved state is already installed
  fixedUpdate(dt) { if (this.props.active) this.state.count += this.props.rate * dt; }
}
```

`properties` is an exported plain schema; supported types are `number`, `boolean`,
`string` and (M65) `entity`, usually declared as `{type:'entity', default:null}`; scripts
receive null or a safe Entity wrapper (see [entity references](entities.md#m65-references-and-physicalpresentation-consumers)).
Non-null authored entity values/defaults use `{entity:'decimal-stable-id'}`,
not an Entity instance or display name. `PropertySchema` describes these authored
values; `ScriptContext.properties` contains the resolved wrappers.
Supply matching defaults: a default is required whenever the authored
values omit that field. Authored values may override declared fields;
unknown fields/type mismatches fault the slot. The inspector edits those values,
not JS source. Asset references may be string properties. There is no exported
`Behaviour`, decorators or component scripting DSL.

Inspector metadata evaluation runs top-level module code in a no-world VM. Keep
top-level code declarative; importing `judas` is fine, calling its world API there
fails (logging is allowed). Restart Play after code edits; no hot reload.

## Callbacks and order

All callbacks are optional and synchronous. Return normally; do not return a
Promise. No async callback/top-level-await scheduler exists.

| Callback | Actual boundary and argument |
|---|---|
| `start(dt)` | Once before the first UI/frame/fixed callback that reaches this slot; not guaranteed to begin in fixed mode. |
| `restore(dt)` | Replaces `start` for a modern loaded slot, after saved `state` is installed; reacquire handles/UI here. No implicit fallback to `start` if absent. |
| `uiUpdate(dt)` | Each interactive outer frame, including while a modal UI pauses gameplay. |
| `onUI(event)` | UI input events, before gameplay input; broadcast to live scripts. |
| `update(dt)` | Active gameplay frame, before that frame's fixed-step catch-up. |
| `presentationUpdate(dt, alpha)` | After fixed-step catch-up, before the interactive view/audio/world draw. Runs also on zero-step frames and while UI pauses gameplay. Alpha is the same [0,1] interpolation fraction used by rendering; `dt` is outer-frame seconds. |
| `fixedUpdate(dt)` | Before ordinary fixed-step force/physics advance; zero or several calls per rendered frame. |
| `onCollisionEnter/Stay/Exit(event)` | Fixed-step authoritative contact delivery after physics/motor/pose publication. |
| `onTriggerEnter/Stay/Exit(event)` | Same event boundary; sensors generate no physical response. |
| `onFracture(event)` | Logical owner's scripts after fixed-step fracture topology publication, outside solver loops; see [event data](fracture.md#onfractureevent). |
| `onSignal(event)` | Explicit subscribed lane: fixed after contact/fracture publication, or UI after UI input before gameplay. Queued and bounded; see [signals](signals.md). |
| `destroy(dt)` | Slot removal/disable or ending the VM; last callback delta is passed, not a teardown timestep. Faulted instances skip this callback. |

Order within normal callback phases is ascending entity ID, then authored slot
vector order. This is callback scheduling order, not a guarantee that the physics
world has already consumed another script's intent. `start` (or modern-load `restore`) precedes the first phase reaching
an instance, not a global start-all-before-any-update barrier. Callback lists are snapshots: destroying an
entity invalidates its wrappers immediately; synchronization later retires its
slot and calls `destroy` if not faulted. Spawned script instances enter at the next
synchronization boundary, which can be another fixed step in the same outer frame.
Do not promise one-render-frame delayed spawning.

UI runs before gameplay, consumes logical actions sharing physical bindings, and
visible enabled modal documents pause gameplay/fixed accumulation. The frame
closing a modal is also withheld from gameplay. Physics steps call `fixedUpdate`
and advance the authoritative world once. Ordinary motors/poses publish after
physics. [M70 opted-in pose/physical owners](animation-ragdolls.md#referencefinal-phase-and-composition)
prepare motor/reference/IK and drives before physics, then resolve physical pose
afterward. Contacts dispatch after the completed fixed state; a joint read inside
`fixedUpdate` cannot observe a future solve from that same step.
Transitions are requested during callbacks and applied only after the outer
application frame returns. No world teardown occurs inside `scenes.load()`.

`time.fixed` is true in fixed callbacks, contact and fracture delivery; false in
frame/UI callbacks, including presentation. Character velocity/acceleration setters enforce fixed mode (contact
callbacks technically qualify too). Prefer submitting intent in `fixedUpdate`;
contact intent applies after the current motor step. Forces are not phase-guarded,
but use fixed callbacks to avoid frame-rate-dependent repeated force accumulation.
Read relative mouse delta in frame updates, not once per catch-up step.

Sample input/look policy in `update`; resolve physics intent in `fixedUpdate`;
then publish camera/cosmetic transforms in `presentationUpdate` using
`entity.presentedTransform`. The final phase does not sample another simulation
step. Motor/rigid-body translation and quaternion orientation share the existing
previous/current render interpolation. `entity.transform` stays authoritative in
all phases. This avoids a fixed-step camera following an interpolated model, or
a model copied before its motor moves. Interpolation intentionally presents up to
one fixed step behind simulation, as the renderer already does. Cosmetic followers
should have no physical body; pose writes on bodies are teleports, not rendering.
Only presentation reads use alpha; fixed queries/forces must use actual physics.
M71 authority/kinematic-command writes explicitly reject presentation callbacks;
they advance through PhysicsWorld, not a render-following transform assignment.

The scripted screenshot harness calls presentation with `dt=0` and its draw alpha;
standalone and editor Play use the ordinary interactive phase. This callback is
not a second input/physics update, a smoothing filter or a thread. As with `update`,
callbacks can call exposed APIs, but fixed-mode motor setters reject this phase.
Pausing freezes simulation; scripts should avoid advancing visual game timers if
that is their desired menu policy.

## Failures and budget

First module/constructor/callback failure records entity/slot/asset/phase/stack
and faults that instance until its VM is restarted. Other slots continue. A
faulted slot does not receive `destroy`. Legacy delta capture omits faulted slots;
modern M61 capture rejects missing required script records rather than silently
saving an incomplete world.
Module globals are still shared; error isolation is not independent VMs per slot.

The VM has 64 MiB managed memory, 512 KiB stack and a default 10,000 interrupt-poll
budget per invocation. This bounds runaway JS, not wall time or indivisible native
work. Standard synchronous language facilities exist, but no Node filesystem,
DOM, timers, networking, npm, promise pumping or JS worker API is supplied.
Scripts are trusted project code, not a security sandbox. `__judas` is an internal
native dispatch function; `globalThis.console` aliases the public logging object.

## Modern save restoration (M61)

The constructor context includes `restored`; restored construction has
`initialState:null`. Loaded script state is installed before `restore(dt)`, which replaces `start(dt)` for modern slots. Reacquire handles and UI in restore; do not replay new-game side effects. [Save contract](saves.md). Legacy delta restoration retains its historical lifecycle.

## Owned signals (M73 candidate)

[Signals](signals.md) are transient slot-generation-owned subscriptions and queues.
Register in both start and restore through a shared helper, preserving restored facts.
Constructor/module evaluation cannot publish; teardown only permits unsubscribe.
Retirement/fault cleans ownership without relying on destroy. Signal handlers emitted
during a drain wait for a later eligible lane boundary. Fixed signals wait while
paused; explicit UI signals run while modal input owns gameplay. Save capture does
not flush/persist signals. Existing observation callbacks retain their ordering.
