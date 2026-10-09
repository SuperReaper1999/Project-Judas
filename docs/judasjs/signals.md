# signals — owned transient messages (M73 candidate)

[Index](../JUDASJS.md) · [Lifecycle](lifecycle.md) · [Save/restore](saves.md) ·
[Streaming](streaming.md) · [UI](ui.md)

Import `{signals}` from `'judas'`. All operations require a live behaviour callback
(including `start`/`restore` and engine-event callbacks). Constructors and module
metadata evaluation cannot subscribe or emit. Only unsubscribe is allowed in
teardown. These are project-named observations, not durable commands/game rules.

| API | Return / behaviour |
|---|---|
| `subscribe(name, phase='fixed')` | Disposable string token owned by this slot generation. Requires callable `onSignal`; duplicate same slot/name/lane returns same token. |
| `unsubscribe(token)` | True if removed, false if already retired. Foreign world/slot or malformed token throws TypeError. |
| `emit(name, payload=null, phase='fixed')` | Broadcast to live subscribed slots; `{recipients, sequence}`. No listener: `{recipients:0, sequence:null}`. |
| `send(target, name, payload=null, phase='fixed')` | Same, only subscribers on the live Entity target (not children). Wrong type throws TypeError; stale entity throws ReferenceError. |
| `stats` | Detached counts: subscriptions, queuedEvents, queuedBytes, queuedRecipients, accepted, delivered, skipped, rejected. |
| `onSignal(event)` | Optional synchronous behaviour callback; required when subscribing. |

`event` contains `name`, `payload`, `phase`, `sequence` (world-local decimal string),
`senderId` and `senderSlot` (full-precision decimal strings) and `sender` (Entity|null).
Sender death after acceptance preserves notification; reacquired sender may be null.
Sequence is neither a persistent identity nor replay order across worlds.

Names are exact UTF-8, 1–128 bytes, without NUL. Phase is `'fixed'` or `'ui'`,
explicitly matching subscription and emission; never inferred from caller phase.
Payload is null, boolean, finite number, string, dense array or plain object,
including null-prototype objects. See limits below. Native handles must be converted
explicitly to normal durable reference strings if the project needs that meaning.
Objects with getters, setters, functions, symbols, proxies, cycles, exotic prototypes,
nonfinite numbers or undefined values are rejected without invoking hooks.
Capture copies publisher data; each recipient receives its own copy.

## Delivery and timing

Acceptance captures matching subscription generations; later subscribers do not
receive history. Recipient order is entity ID, authored slot order, token.
Cancellation, disable, fault, destruction or region suspension skips old reservations.
Unsubscribe/re-subscribe does not resurrect them. No subscriber activates a region.

Default fixed messages drain after completed physics/motor/pose/contact/fracture
notifications. `time.fixed===true`, `time.delta` is the fixed interval. Intent/forces
submitted then affect later physics, not another integration of this step. Fixed
messages wait while paused. UI messages drain after UI events before gameplay,
including paused/zero-step frames; `time.fixed===false`, delta is UI frame time.
Fixed-only setters remain guarded. UI handlers may queue later fixed messages.

Each lane freezes its eligible batch. Handler emissions wait for a later boundary;
several fixed steps in an outer frame are several boundaries. FIFO is preserved
through bounded deferred delivery. No recursive calls, promises or implicit replay.
Startup has no subscribe-all barrier: publish initial facts after listeners are
ready, or store/query facts explicitly. Do not rely on every `start` having run.

## Copyable broadcast / target with restore-safe registration

```js
import {signals} from 'judas';
export default class Counter {
  constructor({entity}) { this.entity=entity; this.state={count:0}; }
  register() { this.token=signals.subscribe('game.pulse'); }
  start() { this.register(); }
  restore() { this.register(); } // saved count has already been installed
  onSignal(event) {
    this.state.count += event.payload.amount;
    this.entity.material().set({baseColor:{x:0.2,y:0.7,z:0.9,a:1}});
  }
  destroy() { if (this.token) signals.unsubscribe(this.token); } // optional; native cleanup owns retirement
}
// From a publisher's live callback:
// signals.emit('game.pulse', {amount:1});
// signals.send(selectedEntity, 'game.pulse', {amount:1});
```

## Paused UI lane

```js
import {signals} from 'judas';
export default class MenuReceiver {
  constructor() { this.state={preview:0}; }
  register() { this.token=signals.subscribe('menu.preview','ui'); }
  start() { this.register(); }
  restore() { this.register(); }
  onSignal(event) {
    this.state.preview += event.payload.amount; // works while modal gameplay pauses
    signals.emit('game.pulse', {amount:1}); // fixed recipients wait for resume
  }
  onUI(event) {
    if(event.element==='preview' && event.type==='click')
      signals.emit('menu.preview',{amount:1},'ui');
  }
}
```

## Bounds, errors and lifetime

32 subscriptions/slot, 2048/world; 16 KiB payload, depth 8, 1024 values; 256 fan-out.
Both lanes retain at most 256 events, 1 MiB accounted bytes and 4096 recipient IDs.
Per drain, at most 256 recipient attempts; deferred old work precedes new work.
Capacity failures throw before accepting any part. Stats distinguish queued backlog,
rejections and retired/skipped recipients; byte count is not total VM/RSS memory.
`accepted` counts queued messages, `delivered` counts handler invocations (including
faults), and `skipped` counts retired reservations. `rejected` counts native validation/
capacity rejections after the lazy store exists; facade/permission/stale-target errors
still throw but are not included in that counter.

Handler errors fault that receiver through the existing per-invocation interrupt
budget and diagnostics, including signal name/lane. Subscriptions retire even though
faulted slots skip destroy. Other recipients continue. No work budget is reset by emit.

Stop/replacement clears everything. Compatible streaming adoption retains a live
instance's tokens; suspension/revisit registers fresh ones. State/session/save facts
are separate from tokens/queues. Save capture never invokes signal handlers. Modern
restore runs `restore`, not start: use the shared helper and preserve saved counters.
An accepted undelivered signal is **not persisted**; store required facts/intent.
No durable delivery, networking, wildcard routing, timers, schema/wiring editor or
request/reply service is supplied. Existing engine event callbacks keep their own
ordering/filtering and are not automatically rebroadcast.
