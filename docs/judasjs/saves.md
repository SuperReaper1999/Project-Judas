# `saves` — project-owned save policy

[Save identity diagnostics and recovery walkthrough](../DEVELOPER_WORKFLOWS.md)

Judas captures/reconstructs engine state. Projects decide when to save and what
score, inventory or other game data belongs in script `state`/`session`.
See [participation and storage contract](../M61_SAVES.md),
[lifecycle](lifecycle.md), [scene/session](scenes-state.md),
[streaming](streaming.md), [safe handles](entities.md).

```js
import {saves} from 'judas';
const request = saves.save('slot-a', {name: '日本語 — my game', metadata: {chapter: 2}});
// Poll in a later update/UI callback. Queued is NOT disk-commit success.
const result = saves.status(request);
if (result?.state === 'failed') console.log(result.error);
```

| Member | Result / semantics |
|---|---|
| `save(slot, {name?, metadata?}={})` | Numeric request token. Captures at a later safe frame boundary, not the exact call instant. |
| `load(slot, {migrate?:boolean}={})` | Request token; validates content and builds a private world. Publication replaces the world once. |
| `delete(slot)` | Request token; removes current and previous-good generation of this slot only. |
| `refresh()` | Request token for bounded metadata listing. |
| `list()` | Detached cached metadata array; refresh explicitly after boot. No world construction. |
| `exists(slot)` | Checks cached listing, including incompatible/corrupt entries; not a synchronous filesystem probe. |
| `status(token)` | Detached status or `null` after record retirement (32 requests retained). |
| `cancel(token)` | `true` only while queued/preparing; reading/writing/committed operations cannot be cancelled. |
| `exclude(entity, excluded = true)` | Omit an explicitly ephemeral simple body or visual. Required modern components and references cannot be hidden behind this flag. |
| `reference(entity)` | Stable opaque reference string, or `null` for destroyed entity. Full-width IDs never go through Number. |
| `resolve(key)` | Fresh Entity or `null` if absent/unloaded/destroyed; never loads a region from a getter. |

**Historical defect (M78 checkpoint audit, LIF-002):** excluding a legally eligible simple
dynamic actor omits its scene entity but retains a sleep record. Full restoration
then fails with `saved sleep body unavailable/duplicate`. The normal non-excluded
control passes. The original failure remains in the
[measured reproduction](../evidence/m78-closure/audit-lifecycle/save-exclusion/MEASURED_RESULT.md).
The subsequent M78 correction applies the existing retained-body ownership rule
to sleep capture as well as contacts. Newly captured eligible exclusions restore;
required mapped articulation bodies retain their sleep records despite their
internal transient flag. Sleep format 1 and strict missing/duplicate/truncated
record rejection are unchanged. Previously emitted orphan-bearing slots are not
repaired retrospectively. See [current scoped results](../M78_LIFECYCLE_CLOSURE.md).

Slot IDs are 1–64 ASCII letters/digits/underscore/hyphen, not paths. Names are
independent bounded Unicode strings. Metadata and script/session data must use
bounded plain JSON values, not wrappers, functions or closures. One operation is
accepted at a time per project session; conflicting requests/scene replacement
throw a useful busy error. Request records and cached lists do not survive process
restart; slot files do. A caller's entity may disappear while its write completes.

New slot diagnostics record the actual engine build ID rather than a milestone
label. An older envelope without that optional diagnostic reads as `unknown`;
no historical build is guessed. This field is provenance, not migration policy
or a compatibility promise. Unsupported native participants still reject with
their entity/component named, including unsupported global thermal state even
when an actor is marked ephemeral. Failed native capture preserves the caller's
previous complete snapshot; a successful save still uses the normal storage
publication and recovery contract below.

States are `queued`, `writing`, `reading`, `working`, `preparing`, `restoring`,
`completed`, `failed`, `cancelled`. Capture occurs atomically inside the frame
boundary; it is measured but is not a multi-frame pollable phase. Times are
milliseconds; timestamps are UNIX milliseconds. `completed` for a save means
file and directory synchronization finished. A late failure explicitly reports
publication with uncertain durability. Recovery from a valid previous generation
sets `recovered`; corrupt/incompatible/missing/busy errors are not successful loads.

## Load-aware lifecycle

The constructor receives `{entity, properties, restored, initialState}`; modern
restoration has `initialState:null`. It should establish
references/local helpers, not spawn new-game content unconditionally. Saved
`state` replaces constructor defaults before callbacks. Modern loaded slots call
`restore(dt)` **instead of** `start(dt)`, then normal phase callbacks. A missing
restore callback does nothing; start is never replayed as an implicit fallback.
The legacy `.judasstate` lifecycle remains separate.

```js
import {ui, saves} from 'judas';
export default class {
  constructor({entity}) { this.entity = entity; this.state = {score: 0, held: null}; }
  start() { this.reacquire(); /* new-game work belongs here */ }
  restore() { this.reacquire(); }
  reacquire() { this.hud = ui.get('hud'); this.held = this.state.held ? saves.resolve(this.state.held) : null; }
}
```

Callbacks run after publication. They may reacquire handles/UI/resources and apply
project policy; they must not replay target hits, initial water, starting enemies
or score initialization. Modules/closures/globals/UI focus are new, not serialized.
Required modern state is listed in the participation contract. Unsupported required
legacy state rejects the save explicitly. Save files never execute migration code.
Strict content compatibility is the default; M98 optionally accepts explicit, project-authored ordered migrations (below). container 1 timestamp seconds have
an explicit checked upgrade to container 2 milliseconds. Unsupported game or
participant versions reject before live mutation.

Linux runtime slots live in `$XDG_DATA_HOME/judas/games/<identity>/Saves/Slots`
(or `$HOME/.local/share/...`). Local/export namespace is stable across package
moves. New projects receive an authored random `save-identity`, keeping unrelated
same-name projects isolated. Equal legacy project name/filename identities share a
namespace unless an explicit `save-identity` is authored. Copies of an identity
intentionally represent the same game. Editor Play uses `EditorSlots`. Runtime
packages do not need write access to their installed content.

## Deformable/fracture participation — M62/M63

A scene containing deformables adds a required version-1 `deformables` participant;
M63 fracture scenes require version 2 of that participant, including irreversible
topology/material removal and owned rigid-joint state. Scenes without deformables
omit this participant. It preserves authoritative nodes/velocities, plastic rest state, runtime materials/attachments, enabled/sleep
state. Missing or failed required simulations reject save capture. Runtime handles,
contact candidates and presentation buffers rebuild on restore. Retained independent
region deformables use streaming participant version 2. See [Deformable](deformables.md)
for supported contact, scale and attachment semantics.

## Other modern state and legacy boundaries

Modern slots also retain motor support/motion state and remap the supporting
identity to fresh bodies. Runtime joint limits/motor/spring settings persist;
native handles and most solver caches rebuild. Current captures also retain a
guarded joint continuation context; see below. These guarantees belong to explicit
modern slot save/load, not ordinary reload or the legacy `.judasstate` path.
Composed projects use the same service with qualified resident/retained region
records; the legacy delta restriction does not disable modern slot saves.

## M65 participant compatibility

Authored/runtime limb IK settings, visual socket identity, typed entity properties,
physical-material selection/overrides and runtime-created joints use the existing
entity/pose/ownership records. An optional `sleep` participant (version 1) stores
settling state by stable entity identity. Old slots without it reconstruct awake;
new slots include it, with conditional deformable state still checked separately.
The SaveApplication participant assertion changes from 11 to 12 for this additional
chunk, rather than relaxing required participants. Cached GPU matrices/layouts are
not save data. Current integration includes a separate-process save/load check.

## Current pose and motion intent (M70–M71)

Modern slots preserve full-body IK configuration/targets and physical-animation
region settings in ordinary entity definitions. The skeletal participant preserves
playback/mixer/layers and source/final poses. Physical authority, pending mode
requests and the mapped articulation are separate participants. Load reconstructs
joint/body mappings and safe handles; solve timings, saturation observations,
previous motion samples and general numerical caches are not serialized gameplay state.
The bounded joint continuation exception below is numerical state, not script state.
Use `restore` to reacquire wrappers and let the next fixed solve publish diagnostics.

Kinematic bodies retain current motion authority/physical state plus durable target
or velocity commands, including the target's remaining duration. Save/load does not
reapply authored initial velocity over a captured command. The legacy pose-delta
`.judasstate` save rejects changed authority/kinematic intent; use modern slots.
[Kinematic reference](entities.md#kinematic-motion-m71) · [Pose/physical reference](animation-ragdolls.md).

<a id="joint-continuation--current-m78-corrective-candidate"></a>

## Joint continuation and legacy compatibility

Joint-bearing modern slots pair articulation participant version 5 (version 4
introduced continuation) with contact participant version 2. They retain actual latched angular-drive commands,
the selected finite-pass joint impulse history, and a separate completed contact
observation with its real epoch/timestep. Safe native handles reconstruct from
existing identities; no contact impulse is replayed and no enter callback is
invented. A changed timestep discards numerical guesses. Changed physical context
invalidates or rejects continuation; malformed/newer/missing paired data rejects
the private restoration before publication. No new JavaScript method is required.

If excluding a transient body omits an otherwise eligible positive completed
contact, the snapshot records that publication as unavailable. It does not
present the remaining subset as complete or empty; the native controller keeps
its ordinary first-sample wait. A genuinely completed empty publication remains
distinct from an unpublished one.

An independent character query-proxy refresh does not invalidate another articulation's restored physical publication. A mechanically connected proxy remains part of the physical context. The later [lifecycle correction](../M78_LIFECYCLE_CLOSURE.md) separately establishes authored query metadata/enabled publication on streamed activation/revisit; disk reconstruction alone did not prove that contract. The same closure also records live motor tag propagation to the query capsule; [CharacterMotor metadata](character.md) describes the current rule.

This is not a promise of universal bitwise trajectories or persistence of every
cache. Old articulation/contact versions remain readable through their previous
cold-start path. An older in-progress save that omitted this context can still
fail to continue its original bounded physical task. The original M77 failure is
preserved, separately from passing new captures. See the
[mechanism and compatibility report](../M78_COLD_CONTINUATION.md).

## M97 mutable regions

Slots include committed [Terrain](terrain.md) instance fields/settings. Pending edits
defer capture; cold restore reconstructs actual collision and navigation. Retained
regional terrain and supported primitive construction use streaming archive v9.
Ordinary joints now preserve exact native physical settings (articulation v5) before
strict warm-history validation; moving-owner frame round trips are not used as a
substitute for those settings. Earlier save formats retain their existing contracts.


## Save evolution — M98

Current saves expose separate `content` (SHA-256), `compatibility` (project-owned
contract string) and `contentVersion` (integer; 0 means legacy exact-only).
Container format 4 protects identity metadata and payload with a combined hash;
older containers 1–3 remain readable. Native snapshot/participant versions remain
independent and are validated strictly. A matching name or version never substitutes
for content identity.

```js
import {saves} from 'judas';
// The installed project must explicitly approve this exact source fingerprint.
const request = saves.load('before-update', {migrate:true});
// Poll on subsequent updates. No synchronous world replacement occurs here.
const result = saves.status(request);
if (result?.state === 'completed' && result.migrated) {
  console.log(result.migrationReport); // ordered steps, source/target hashes
  saves.save('after-update');          // source slot is protected in this session
}
```

`SaveRequest` adds `migrated`, `migrationMs` and `migrationReport` (`exact`, an
ordered JSON report, or empty before comparison). Conversion success does not
mean load success: required native resources, geometry, continuation and streaming
contracts must still pass in the private world. Report fields can describe a
conversion whose later native restoration failed. `migrated` therefore only means
a conversion was performed; always inspect `state` and `error`.

`SaveSlot` adds `content`, `compatibility`, `contentVersion`. Cached `status` is a
conservative exact-identity hint, not a migration feasibility or native-restore
certificate. Compare the identities and use the explicit load to get diagnostics.

No save file contains executable migration code. The installed project owns a
bounded `saveEvolution` JSON policy, edited in Project settings or `.judasproj`.
It pins approved source hashes and declares consecutive steps. Supported operations
patch project script/session JSON, rename a registered scene, or update a narrowly
allowed subset of saved scene/prefab metadata and appearance. Native physics
records, IDs, geometry and opaque participants are never rewritten or discarded.
Missing/changed mods reject unless an exact old/new context is explicitly approved
by the installed policy; package trust/dependency resolution still belongs to the
normal mod launcher. No package is installed/enabled by loading a save.

Loading/migrating is read-only on disk. Original source slots survive failures;
a successfully migrated session requires a **different destination slot**.
Ordinary overwrite retains the exact previous valid bytes. `judas_save` provides
inspection, dry conversion, separate-slot conversion/recovery and previous-good
recovery. It does not replace the game's full native restore validation.
See [policy schema, tools, fixtures and boundaries](../M98_SAVE_EVOLUTION.md).

## Active network sessions

Close the [network session](network.md) before saving/loading or replacing a scene.
Requests are rejected before mutation while a session is active; list/delete slot
management remains available. Sockets, peers, input and presentation buffers are
transient. Pending save/scene operations likewise prevent hosting or joining.

## Coordinating lifecycle operations

`saves.busy` is a read-only, allocation-free observation of the project session's
lifecycle gate. It is true during save/load/list/delete work **or scene
replacement**. It does not create a save service merely to query it. This is an
observation, not a reservation: another callback may start an operation afterward.
Concurrent requests still throw the existing busy error. There is no implicit
queue and `ui.quit()` does not flush a save automatically.

When implementing Save & Quit, keep ticking `uiUpdate` while paused, wait for
your particular token to reach `completed`, and show `failed`/`cancelled` to the
player rather than quitting. `!saves.busy` alone does not prove that token succeeded.

`SaveRequest` includes real monotonic-clock diagnostics in milliseconds:

- `elapsedMs`: elapsed since request acceptance, updated at outer boundaries and
  frozen at completion, failure or cancellation.
- `queuedMs`: time awaiting capture/read submission, including identity,
  streaming or resource preparation.
- `identityMs`: duration of the content-hash job consumed by this request.
- `workerQueueMs`: delay between job submission and worker start.
- `waitingFor`: `identity`, `boundary`, `streaming`, `resources`, `worker` or
  `restoration` while pending; empty at a terminal state.

Existing `captureMs`, `workerMs` and `restoreMs` retain their meanings. Timings
can overlap and are not additive. Initial identity work may predate the request;
the job duration is not the same as the request's wait for it. Status polling
does not itself advance these values. Request capture and restoration still
occur only at safe owner-thread boundaries.

The uncapped test harness advances synthetic time much faster than wall time.
A queued save spanning 90 harness frames does not establish a 1.5-second real
save latency. Measure these fields or a wall clock. Content identity continues
to re-read project dependencies; same-size asset edits must still be detected.

Save byte telemetry is obtained from the envelope already encoded by storage;
it does not trigger a second encoding. `bytes` describes encoded data when
available, not a durability guarantee. Check the terminal state/error. For the
latest consumer timing investigation and limits, see
[Hinge save latency](../SAVE_LATENCY_FOLLOWUP.md).
