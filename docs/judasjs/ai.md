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


## Optional sensing

```js
import {AISenses, AITargetReservations, entity} from 'judas';
const senses = new AISenses({interval: .2, maxResults: 16, maxCandidates: 128, maxRaycasts: 8});
const claims = new AITargetReservations(); // instantiate inside your script lifecycle
const observation = senses.sample(actor.transform.position, 20, {
  requiredTags: ['food'], visibility: true
});
const candidates = observation.targets.map(t => ({
  action: 'eat', target: t.entityId, facts: {hunger, distance: t.distance},
  eligible: t.visible === true && claims.available(t.entityId),
  reason: 'Unavailable, occluded or visibility unknown'
}));
```

The fragment illustrates composition, not a complete attachable behaviour. Keep
helpers on the creating script instance and sample inside a lazy candidate function
or another deliberate sensing cadence. For fresh-object code use **Create AI
Behaviour** in the Scripts inspector and edit the generated action implementation.

`AISenses(settings = {})` settings: interval [0,60] simulation seconds (default .2),
maxResults 1..128 (default16), maxRaycasts 0..128 (default8), and maxCandidates
maxResults..4096 (default128). `sample(origin, radius, options = {})` takes a local
world-space `{x,y,z}`, radius [0,1,000,000] metres, and optional visibility, planar,
includeSensors, includeLayers, excludeLayers, requiredTags, excludedTags and
occluderLayers. Category arrays accept valid project names or numeric IDs;
unknown/retired names are errors. Occluder layers default to all, independently
of the target filter. `dispose()` releases the instance; script retirement does
so automatically.

Returned observation: targets `{entityId,position,distance,visible}`, cached, stale,
age, sampledAt, truncated, candidatesConsidered, raycasts and queryCandidates.
Entity IDs are decimal strings; positions are actual physical body centres, not
visual pivots. Visibility is boolean only when tested; otherwise null. Radius is
centre-based. Only ordinary published entity-owned physical bodies are targets;
render-only markers and auxiliary articulation bodies are not silently replaced
by another entity/root. The owner's ordinary body is ignored.

A cached sample is a timed observation, **not a guarantee about the current
world**. Every reused result sets stale=true, even at age zero. Age is simulation
time since the original sample. Retained target identity/position/filter changes,
request changes, entity publication and origin changes invalidate relevant caches.
New arrivals and moving occluders can remain unseen until refresh. Set interval=0
for an uncached query or use your own immediate interaction validation.

maxCandidates bounds processed broadphase matches; maxResults bounds returned
entities; maxRaycasts bounds LOS casts. The shared broadphase still traverses and
materializes its complete match vector. Those budgets do **not** guarantee a
constant CPU/memory cost in arbitrarily dense worlds. When truncated, results are
nearest within the processed stable-handle subset, not necessarily the globally
nearest targets. Unknown LOS is independent of truncation. A cache hit reports
zero new queries/rays/processed matches, while preserving the original observations.
Each script slot may own16 sensors; a session may own4096. No work is scheduled
without explicit calls; caches are transient and are not serialized.

## Exclusive target leases

`new AITargetReservations()` creates an independent, script-owned claimant scope:

| Method | Meaning |
| --- | --- |
| `available(key)` | True when unclaimed/expired or held by this same scope. |
| `acquire(key, leaseSeconds=1)` | Atomically acquire or renew; false for contention or capacity refusal. |
| `release(key)` | Release only this scope's claim; false for absent/foreign ownership. |
| `releaseAll()` | Release this scope's claims. |
| `dispose()` | Release claims and retire the scope; idempotent. |

Keys are opaque 1..256-byte strings, without NUL. They need not be entities.
Lease duration is .001..60 finite simulation seconds and must be representable
at the current clock. Invalid inputs throw without changing live ownership;
capacity denial returns false without evicting another claimant. Limits are256
live targets/scope,4096 live targets/board,16 scopes/script slot and4096 scopes/session.
Expired leases are reclaimed lazily when inspected/acquired; no background scan.

Check availability while building candidates, then acquire again in execution:
availability is an observation, not a lock. Renew while using the target and
release in action stop/finish. Separate scopes cannot share a claim merely because
their owner script is the same. Target destruction is project policy because keys
can identify arbitrary resources; release it or allow its bounded lease to expire.

The board is local to the authoritative script session, not a network lock service.
Run it on the appropriate host. Do not save handles or assume leases survive
load/reload/region teardown. Save gameplay progress and AI snapshots; reacquire
on execution and handle refusal without replaying one-time rewards. The generated
starter and native regressions demonstrate this contract. Existing world-save
restrictions still apply.
