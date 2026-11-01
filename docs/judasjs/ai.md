# Utility AI

`import {UtilityAgent} from 'judas';`

An optional native decision agent owned by the creating script instance. Native
scoring chooses an action/target; project callbacks implement effects. See the
[authoring guide](../GAME_AI.md) and [editable workshop](../../projects/ai_workshop/README.md).

```js
import {UtilityAgent} from 'judas';
import definition from './guard.ai.js';

export default class {
  constructor({entity}) { this.entity=entity; this.state={brain:null,health:1}; }
  start() { this.makeAgent(); }
  makeAgent() {
    this.brain=new UtilityAgent(definition, {
      patrol: {
        start:(context,choice)=>{/* acquire the target/destination */},
        update:(dt,context,choice)=>{/* use existing movement APIs */},
        stop:(context,choice,reason)=>{/* release movement/destination */}
      },
      hide: {
        update:(dt,context,choice)=>{/* validate cover and move there */},
        stop:(context,choice,reason)=>{/* release temporary ownership */}
      }
    }, 'Guard');
  }
  restore() { this.makeAgent(); if(this.state.brain)this.brain.restore(this.state.brain); }
  fixedUpdate(dt) {
    this.brain.tick(dt, ()=>[
      {action:'patrol',target:'route-a',facts:{}},
      {action:'hide',target:'cover-a',facts:{health:this.state.health},
       eligible:this.state.health<0.8,reason:'No injury requiring cover'}
    ], this);
    this.state.brain=this.brain.snapshot();
  }
  destroy() { this.brain?.dispose(); }
}
```

The example omits actual route/cover handling deliberately. Copy the workshop for
a full working implementation; these comments are not built-in patrol behaviour.

## Definition

Version 1 is a bounded plain JSON object, with unknown fields rejected:

| Field | Meaning |
| --- | --- |
| `version: 1` | Definition schema identity. |
| `name` | Human-readable definition name. |
| `interval` | Decision-evaluation interval in simulation seconds. |
| `initialDelay` | Optional first evaluation delay, 0 through interval; deterministic staggering. |
| `switchAdvantage` | Challenger advantage required to replace the current candidate. |
| `actions` | Array of uniquely named action definitions. |

Each action has `id`, optional `weight`, `mode` (`product` or `mean`), `cooldown`,
`minCommitment`, `interruptible`, `requiredFacts` and `considerations`. Each
consideration has a named `fact`, optional `weight`, input `min`/`max`, a `curve`
(`linear`, `inverse`, `power`) and a positive `exponent` for power curves.
Required facts must be present and numerically positive. Missing consideration
facts make that candidate ineligible. The engine does not invent facts or targets.

Candidate identity is the pair of action ID and target string. A target is opaque:
use decimal entity IDs or your own stable keys, not full-width IDs converted to
Number. Candidate facts are finite numbers. Use `eligible:false` with a concise
`reason` for project conditions such as an empty pantry or unreachable cover.

Definition limits: 64 actions, 32 considerations per action. Evaluation limits:
256 candidates, 128 facts per candidate, 16 agents per owning script slot and
4,096 agents per script session. These are rejection bounds, not performance
guarantees. Snapshots include normalized definitions; the existing 64 KiB total
script-state limit still applies. Large definitions/multiple agents may need
separate owning scripts or smaller configurations. Invalid definitions and oversized or
malformed inputs throw; no hidden candidate truncation is promised.

## Agent methods

| Member | Contract |
| --- | --- |
| `new UtilityAgent(definition, handlers={}, name='NPC AI')` | Validate and create a native agent for this script owner. |
| `tick(dt, candidatesOrFunction, context={})` | Advance simulation time, evaluate when due, dispatch action lifecycle and return the current decision result. |
| `finish(status, context?)` | Report `success`, `failure` or `cancelled`; supported from an action update. |
| `snapshot()` | Detached JSON containing native continuation state. Store it explicitly in project `state`. |
| `restore(snapshot)` | Validate compatible state before restoration; active action re-enters start at the next tick. |
| `inspection` | Detached latest native scores/reasons/selection/timers for observation. |
| `dispose()` | Retire the agent and release callback ownership; also use normal script cleanup. |

A lazy candidate function is invoked only on evaluation ticks. Expensive sensing
should be performed there or refreshed independently and cached; creating an
array before every tick still performs your array/sensor work on skipped ticks.
Actions update on normal ticks even when no new decision is due. Native decisions
and eligibility are refreshed at that cadence; actions should still check a
target's actual validity before using it on each execution tick. `dt` must follow
the engine's fixed-step simulation, not variable render timing.

Handlers are keyed by action ID. Each can supply:

- `start(context, selection)` when entering or rehydrating an action.
- `update(dt, context, selection)` to execute game policy.
- `stop(context, selection, reason)` to release policy-owned state on retirement.

`selection` is `{action, target}`. Handlers are not serialized. An active restored
action's start must reacquire state safely rather than repeat damage, purchases
or rewards. Finish/cancel and interruption are different from merely observing a
low score. Call `finish` from update or ordinary project code, not from start/stop
callbacks; recursive ticks and restoring during action dispatch are rejected.
`tick` reports the completed outcome if update finishes the action. Selection
objects passed to handlers are immutable. Dispose, script destruction and faults must not be used to fake action
success. The engine never updates entity transforms on behalf of a selected action.

The result contains `selected`, `previous`, `changed`, `evaluated`, `reason` and
native timing/diagnostic information. A skipped evaluation may omit candidate
rows; use `inspection` to inspect the last complete evaluation. The selected
value is null when no candidate is eligible or after completion leaves no action.

## Errors, ownership and portability

Keep native agent handles out of saved JSON. Store snapshots alongside the
project facts/action progress and reconstruct on restore. Definition compatibility
is deliberate; changing an action set is not automatically a save migration.
A stale/foreign script instance must not take over another agent's state. Normal
entity destruction retires script agents at the engine's next safe script-lifecycle
synchronization boundary; no detached decision loop runs in between.

No external service or platform-specific AI backend is required. Decisions do
not assume world-up, a particular character controller or 2D versus 3D. Native
world compatibility and export qualification remain the existing engine's
contracts; this API does not make unsupported world combinations saveable.

For networking, project authority determines where agents run. There is no
automatic AI replication or client-side reconciliation in this initial framework.
