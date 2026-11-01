# Game AI: decisions and project-owned actions

Judas provides an optional Utility AI decision mechanism. It does not contain
predefined combat, hunger, personality, cover selection or social simulation.
Developers define candidate actions/targets and sensed facts; native scoring
selects a candidate. Ordinary project action callbacks perform the gameplay.
No machine learning, model download or external service is involved.
[Initial qualification](GAME_AI_QUALIFICATION.md) ·
[Usability, sensing and reservation qualification](AI_USABILITY_QUALIFICATION.md).

A tiny enemy can offer `patrol` and `hide`. A simulated person can offer meals,
beds, work and conversations. Both use the same decisions, action lifecycle,
cooldowns and inspection; the smaller project need not adopt the larger policy.

Start with the editable [Little Haven project](../projects/ai_workshop/README.md)
and the [JudasJS API](judasjs/ai.md). Navigation and movement remain existing
subsystems, not a second AI-owned physics engine.

## Start with a selected object

In a script-owned project, select an empty object and open the
Inspector’s **Add gameplay script** section (open by default on a fresh object). Choose **Create AI behaviour**, enter a name, and create it. Judas
creates two normal Script assets in `Assets/Scripts`: a constrained `.ai.js`
definition (decision settings) and a `.js` behaviour (your action code), then
attaches the behaviour in one undoable scene operation.

A fresh 3D object gains a visible capsule and CharacterMotor. Three visible root
markers are wired as patrol, return and cover targets. Place a floor beneath the
actor and press Play. For an existing kinematic native 2D actor the starter uses
its real collider and authored plane. Existing rigid/dynamic ownership is rejected
with an explanation instead of silently converting a vehicle or physical assembly.
The starter moves directly with collision checks; it is not a pathfinder.

The Inspector gives separate **Decision settings**, **Action code** and
**Inspect this NPC** shortcuts. Live decisions can follow the selected object's
actual script ownership. A coordinator's agents remain owned by its coordinator;
Judas does not guess ownership from the visual object an action controls.

The generated patrol/hide example is ordinary readable game code. Send
`signals.send(npc, 'ai.damage', {amount: .6})` from your combat script to see the
hide action compete against patrol. Recovery and movement are script policy.
Target references, cancellation, one-second renewable claims and save restoration
are included. The embedded template is in `src/editor/AIBehaviourTemplate.h`;
no external template package is required. Undo removes scene setup while keeping
created source assets, as with existing editor script workflows.

## Authoring

An AI definition is a normal tracked Script asset with exactly this content shape:

```js
export default {
  "version": 1,
  "name": "Simple guard",
  "interval": 0.2,
  "switchAdvantage": 0.1,
  "actions": [
    {"id":"patrol","weight":0.2,"considerations":[]},
    {"id":"hide","weight":1,"minCommitment":0.4,"cooldown":1,
     "considerations":[{"fact":"health","curve":"inverse","min":0,"max":1}]}
  ]
};
```

Open **View > AI actions and decisions**. The **Author actions** tab edits this
constrained data format, validates it and saves the same
native definition used by scripts. It does not overwrite arbitrary JavaScript.
Import the module into a gameplay script and instantiate `UtilityAgent` with
project action handlers. Existing Script asset identity, dependencies and export
rules are reused.

Candidates are **action/target pairs**. Supplying three food sources allows target
distance, availability and quality to influence selection. Target strings are
opaque identifiers: AI does not dereference a target, assume a world axis or
teleport an NPC. The project must validate actual entity ownership/lifetime.

## Scores and eligibility

Each consideration normalizes its fact over `min..max`, clamps the input to 0..1,
and applies its response curve. Linear, inverse and power curves cover common
needs; weights and product/mean aggregation are explicit in the definition.
For curve response `r` and positive consideration weight `w`, product uses
`product(r^w)` and mean uses `sum(w*r)/sum(w)`. The action weight multiplies
the aggregate. Inputs normalize/clamp to [0,1]; eligibility remains separate.
Product aggregation makes a zero consideration veto the score. Mean aggregation
allows other considerations to compensate. These express different policies.

Eligibility is separate from score. A missing meal is not a very unattractive
meal: it is an unavailable candidate. Required facts, invalid facts and explicit
project eligibility produce inspectable reasons. Finite, valid sensor inputs are
required; invalid values must not become hidden winning scores.

Stable identity breaks ties rather than container iteration order. Minimum
commitment, interruption policy and switching advantage prevent a tiny score
variation from repeatedly restarting an action. Cooldowns apply through the
native completion contract; projects should finish genuinely completed/failed
actions rather than manually manufacturing winner changes.

## Runtime ownership

An agent belongs to its creating script instance. Script retirement, reload and
world destruction cannot leave a native decision object bound to an old owner.
Actions release their own destinations, movement ownership, interaction handles
and temporary state in `stop`. Destroying a target must make the candidate
ineligible or cause a project action failure.

`start`, `update` and `stop` are project callbacks. `finish` reports success,
failure or cancellation; a chosen action may be interrupted according to its
configuration. No native action writes authoritative entity poses. Use existing
CharacterMotor, MK2 forces/kinematic movement, navigation and animation APIs.

AI is authority-neutral: in a network game, instantiate/tick authoritative AI on
the authoritative host. The decision framework does not automatically replicate
arbitrary agent state, project effects or client-side script closures.

## Performance and determinism

Tick from a fixed simulation callback. Definitions have an explicit decision
interval; action execution can still run every fixed step. Supply a lazy candidate
function to avoid expensive sensing on non-decision ticks. Cache visibility,
nearby-target and path results as project facts when appropriate: comparing scores
is often cheaper than repeatedly discovering candidates.

Determinism requires identical definitions, ordered inputs and fixed-step calls.
This does not promise cross-platform bit-identical arbitrary JavaScript or physics.
Avoid wall-time-driven sensor state or unseeded random inputs in deterministic AI.
Bounds reject runaway definitions/candidate lists rather than silently dropping
possible winners. Set `initialDelay` in [0, interval] to stagger the first
evaluation deterministically (for example a stable agent index modulo a fixed
number of phases). This release provides per-agent cadence, not a global
wall-clock scheduler or an automatic sensing budget. Projects needing very
large populations should stagger updates
and measure sensing/action cost as well as native decision cost.

No agent means no decision evaluation. The framework does not run an unconditional
world scan or spawn sensors for projects that do not use it.

## Inspection and persistence

The **Live decisions** tab exposes every named runtime agent, independently of
scene selection, with the selected candidate, alternatives, rejection reasons,
consideration contributions and timing state. Inspecting does not change the
winner. Retained diagnostic data is bounded; it is not an unbounded event recorder.

Save `agent.snapshot()` inside the script's plain `state`, together with gameplay
progress. During `restore`, create fresh agents/handlers and call `restore` on the
matching snapshot. Definition identity/version checks prevent pretending an old
state is compatible with an unrelated definition. Restore never serializes a
closure or resumes a suspended JavaScript stack. An active action re-enters its
`start` handler on its next tick; handlers must rehydrate existing progress and
must not duplicate one-time rewards.

Regional suspension/reload follows existing script ownership and save boundaries.
The framework does not add universal suspension support to otherwise unsupported
engine combinations. If the world cannot save a required subsystem, adding AI
must not hide that failure.

## Optional observations and target ownership

`AISenses` performs bounded-result spatial observations of published physical
entities. Target filters and line-of-sight occluder layers are independent: a wall
need not have the food tag to block the sight of food. A zero/exhausted ray budget
returns `visible: null`, never an invented clear result. Timed cache reuse reports
its original sample time, age and `stale: true`, even if a same-tick wall change
cannot be detected cheaply. Check targets again before applying gameplay effects.

`AITargetReservations` provides exclusive world-session target leases. Different
instances are different claimants, including two instances in the same script.
One NPC can claim a bed, meal or work position while others make that candidate
ineligible. Leases use simulation time, support renewal/release and expire; no
wall-clock thread runs. Claims are released on script teardown/fault/reload before
action cleanup. They are transient: after loading, actions explicitly reacquire
instead of reviving saved ownership tokens or duplicating rewards.

These helpers do not decide who deserves a job, consume inventory, discover every
possible kind of target or coordinate a squad. See [API bounds and usage](judasjs/ai.md#optional-sensing).

## Initial boundaries

This is a utility/action framework, not a behaviour-tree graph, GOAP planner,
perception engine, crowd simulator or pathfinding replacement. Long sequences,
job queues, conversations and coordinated teams remain project policy. Exclusive
target leases are available; higher-level allocation/fairness policy remains yours. A chosen
attack still needs valid collision/hit timing; a chosen route can still fail.

Use runtime evidence to distinguish a bad score, missing fact, invalid target,
blocked action and unsupported world lifecycle. Do not assume a plausible-looking
NPC demonstrates save/export or multiplayer correctness.
