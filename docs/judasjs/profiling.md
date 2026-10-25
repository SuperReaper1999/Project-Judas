# `profiler` — custom diagnostics

Judas owns diagnostic collection. Scripts may annotate policy cost; timings must
never determine game behaviour, simulation cadence or quality. Import from the
normal virtual module:

```js
import {profiler} from 'judas';
const result = profiler.scope('Choose destination', () => chooseDestination());
profiler.counter('Requests', 1);                 // sum in captured outer frame
profiler.counter('Queue length', queue.length, 'latest');
profiler.counter('Peak queue', queue.length, 'max');
```

## `profiler.scope(label, callback)`

`label: string`, `callback: () => T`, returns **T**. Synchronous callback executes
exactly once, even with capture disabled. Original return value/exception and
QuickJS runaway interruption propagate unchanged. Native RAII closes markers on
normal return/error/destruction; nested scopes preserve hierarchy. Does not catch
errors, reset interruption budgets or implement an async runtime. A returned
Promise is merely the callback value; it does not extend the measured interval.
Ordinary script lifecycle restrictions on async callbacks still apply.

## `profiler.counter(label, value, mode = 'sum')`

Finite numeric value; mode `'sum' | 'latest' | 'max'`; returns `undefined`.
Bad label/callback, nonfinite value or unknown mode throws `TypeError` even when
capture is disabled. Runtime numeric conversion is the ordinary bridge conversion;
use actual numbers. Sum/max use observations in the completed frame; latest uses
newest monotonic observation across lanes. A frame without observations has no
entry. Use one mode consistently for a label within a frame.

Custom labels are cached/bounded to 128 per script VM; the native process has
2,048 total labels and rejects empty/128-byte-or-longer names. Overlong/exhausted
markers stop recording and increment diagnostics; their callbacks still execute.
Collection off skips label registration and event writing. Avoid generating
entity/time-dependent labels; use constant names and counters.

There is no script API to alter profiling cadence or read timings into gameplay.
Use editor **View → Diagnostics and extensions → Profiler** or opt-in runtime environment settings described in
[the native profiler guide](../PROFILER.md). These annotations are not save/session
state. See [lifecycle](lifecycle.md), [practices](practices.md) and
[executed profiling example](examples/profiling.js).
