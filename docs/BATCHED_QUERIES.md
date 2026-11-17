# Batched physics queries and paired input

Accepted at `8ddec3b97d66815de4174d6e103be77e74b7eb0f`, originally based on
`c042797c755df68b45e36aa917c4ca72818b9bcd` (scalability). Operator acceptance
uses synthetic controller/VM checks; no physical controller was available.
named-authoring's deferred human authoring review remains deferred. This work adds two engine
primitives; flick recognition, ledge selection and climbing remain project JS.

## Public surface

```js
import {input, physics} from 'judas';
const raw = input.stick('right');
const circular = input.vector('flick_stick');
const delta = input.stickDelta('right');
const snapshot = input.stickSamples('right', cursor);
cursor = snapshot.sequence;
const hits = physics.raycastMany([{origin, direction, maximum: 30}], filter);
```

[Input reference](judasjs/input.md) defines the ordinary named/legacy vector binding,
X/Y scaling and radial deadzone. Raw SDL axes retain their signed normalization and
positive-down Y convention; raw diagonals are not circle-clipped. Existing scalar
maps keep their previous processing and canonical version-1 serialization.

InputSystem holds current raw pairs, render-frame endpoint deltas and 128 ordered
observations per stick. Each reader owns a cursor; reads do not consume another
reader's samples. Global sequence numbers and per-stick eviction watermarks expose
reset/overflow explicitly. Steady-clock receipt seconds are not hardware capture
timestamps. The existing selected-controller SDL event pump records intermediate
delivered axis observations, then polls both axes together. Lifecycle/input
ownership transitions clear history and establish a new baseline. Nothing can
recover motion the backend did not deliver.

[Physics reference](judasjs/physics.md) covers `raycastMany`, `sphereCastMany` and
`capsuleCastMany`. All three accept at most 256 requests and one normal shared
filter. One dispatcher crossing prepares the filter once, validates every entry,
then performs the same authoritative PhysicsWorld casts as scalar calls. Results
are ordered scalar-style hit snapshots/null with normal safe entity wrappers.
There is no batch box/overlap family, new geometry, query scheduler or copied world.
