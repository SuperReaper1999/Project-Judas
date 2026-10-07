# Input, time and logging

[Index](../JUDASJS.md) · [Lifecycle](lifecycle.md) · [Character](character.md)

## input

`import {input} from 'judas'`. Names refer to the current project's M35 map.
`held(name)`, `pressed(name)`, `released(name)` return booleans; `axis(name)`
returns a number. Unknown names/no attached input return false/zero. `vector(name)`
returns a copied `{x,y}` for a named paired binding, or neutral for an unknown name.
Paired controller reads are described below; there is still no raw key/mouse,
binding-edit or consumption API exposed to JS. Project/editor/C++ own binding
authoring; scripts consume the logical surface.

Frame action edges live for one logical frame. Fixed callbacks use a latched
snapshot: a short press across zero-step frames survives to the next fixed step,
then does not repeat in later catch-up steps. Read an edge in one behavioural
phase; reading it in update AND fixedUpdate can intentionally observe it twice.
The engine does not consume it just because one script read it.

Axes retain source semantics: keyboard/stick axes generally normalize/clamp to
[-1,1], sticks use configured deadzones; mouse/wheel axes are relative frame
deltas. Fixed callbacks read the current axis snapshot, not redistributed mouse
motion. Apply mouse look once in `update`/`uiUpdate`, using stick rate × dt where
appropriate. UI can consume a logical action and all actions/axes sharing its
physical bindings; consumed reads are false/zero for that frame's fixed steps.
One active logical gamepad is supported; hardware feel remains operator-tested.

## Paired sticks (M69)

| Method | Result |
|---|---|
| `stick(side)` | Side `left` or `right`; copied current raw `{x,y}` with components in [-1,1]. |
| `stickDelta(side)` | Side `left` or `right`; copied net raw change during the latest render input pump, components in [-2,2]. |
| `vector(name)` | Copied processed named paired binding `{x,y}`. |
| `stickSamples(side, afterSequence=0)` | Non-destructive ordered observation snapshot; cursor must be a nonnegative safe integer. |

Both raw components belong to the **same selected controller**. Values use the
existing signed SDL normalization (negative /32768, positive /32767), X right is
positive, Y down is positive. Raw means before Judas deadzone, scale, smoothing
or response processing; controller firmware and OS processing still apply.
Raw diagonals are not clamped to a unit circle. Existing scalar axis mappings,
buttons, triggers and mouse/keyboard bindings keep their previous processing.
Invalid side names/cursors throw TypeError, including a cursor ahead of this
input service (for example one retained from another service lifetime). No
controller/unavailable or UI-consumed gameplay input yields neutral values.
These are input snapshots, not mutable device state.
When a callback has no attached input service, pair reads are neutral and
`stickSamples(side,0)` returns empty history with sequence 0, both flags false
and capacity 128. A positive retained cursor then fails the same ahead-cursor
check; keep cursors within their input-service/world lifecycle.

The current raw pair and frame delta advance on the ordinary render-frame input
pump, even if no fixed step runs. Repeated readers see the same snapshot. Delta is
an **endpoint difference**, not path length: neutral → diagonal → neutral can have
zero delta. Use history to retain delivered intermediate movement.

### Circular vector binding

Add a vector entry in the project's existing input map/inspector. Example M67
named `input` document (the same `data` is the project's named `input` group):

```json
{
  "schema": 1,
  "kind": "input",
  "data": [{
      "name": "flick_stick", "axis": false, "vector": true,
      "bindings": [{
        "control": "stick:Right", "scale": 1, "scaleY": 1,
        "deadzone": 0.2, "circular": true
      }]
    }]
}
```

`stick:Left`/`stick:Right` select the whole physical pair. `scale` acts on X,
`scaleY` on Y; negative scales invert. Both must be finite. With `circular:true`,
let raw pair `v`, magnitude `m`, inner deadzone `0 <= d < 1`:

```text
m <= d: (0,0)
m > d:  (v/m) * min(1, (m-d)/(1-d))
```

This radial response is applied **once**, then the two signed scales, then
component clamping to [-1,1]. It preserves the raw direction before deliberate
unequal scaling/inversion. Raw `stick()` is unaffected. `circular:false` retains
independent component deadzone processing; omitted fields mean `vector:false`,
`scaleY:1`, `circular:false`. Existing maps need no changes. Invalid/nonfinite
settings are rejected by ordinary input-map validation. Multiple bindings retain
normal summed/component-clamped map behaviour.

Legacy maps without new fields continue writing version 1. The new legacy format
uses version 2 and kind 2 for vectors:

```text
2 1 "flick_stick" 2 1 "stick:Right" 1 0.2 1 1
```

Each binding stores `control scale deadzone scaleY circular`; existing action/axis
kinds remain 0/1. Named load/save and the input inspector use the same InputMap.

### Ordered observations and fixed-step delivery

`stickSamples()` returns:

```js
{samples: [{sequence, time, x, y}], sequence, reset, overflow, capacity: 128}
```

`sequence` is the latest **global** monotonic observation/reset cursor, shared by
both sticks and retained across resets; each returned sample's sequence is strictly
ordered. `time` is monotonic **seconds since this InputSystem was constructed**,
measured at receipt by the backend pump; it is neither simulation elapsed time
nor a precise hardware capture timestamp. Capacity is 128 observations **per
stick**. Only delivered movement can be retained; this does not recover movement
that the device/backend never reported.

Store the returned cursor per recognizer. Read after that cursor to process each
new observation once in `fixedUpdate`; zero-step frames accumulate observations,
and further catch-up steps return none until the next delivered change. Readers
do not steal samples from other scripts. A frame reader may separately inspect
the latest raw/circular pair and delta for presentation.

`reset:true` means the cursor predates a focus/controller/session/pause history
reset; clear previous recognizer state before processing the **included new**
samples. `overflow:true` means older requested observations were evicted; the
returned suffix is incomplete, so do not classify it as a complete flick. Adopt
the returned cursor and restart recognition from a known baseline. The first
observation after reset/reassignment/focus regain establishes a baseline without
creating movement/delta, avoiding artificial flicks. Disconnect, focus loss,
pause and scene lifecycle clear pending history rather than replaying stale
actions on resume. An observation history does not move authoritative gameplay
into render callbacks.

[Copyable right-stick reader](examples/paired-stick.js) shows both values, frame
delta, independent cursor/reset/overflow handling and recent ordered samples.
[100-ray fan](examples/ray-fan.js) demonstrates the corresponding query batch.
[The small operator fixture](cookbook.md#small-operator-fixture) displays these
samples using ordinary authored UI, with C for same-world scalar comparison.

## time

`time.elapsed`: authoritative simulation seconds (fixed clock, pauses with world).
`time.delta`: current callback's delta, seconds. `time.fixed`: current phase flag.
Neither is wall clock or an interpolation timestamp. Teardown retains the last
callback timing; do not integrate motion in `destroy`. Frame catch-up is bounded;
this is not a guaranteed replay/network determinism protocol.

## console

`console.log(...values)` converts each with JS String, joins with spaces and
writes `JS: ...` to application stdout. It returns undefined. Objects generally
print `[object Object]`; use `JSON.stringify` for structured output. No warn/error,
log levels, file sink or developer debugger API. The virtual module also sets
`globalThis.console` to this object.

## input.pointerCapture (M51)

Boolean get/set request for relative pointer capture in this runtime world.
Modal UI temporarily releases actual capture; the getter reports script intent,
not device state. The request resets at world destruction/authored reset.
New projects start uncaptured until a script requests capture. Historical projects
with `legacy-gameplay "true"` retain their compatibility capture policy.
