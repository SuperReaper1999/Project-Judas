# `saves` — project-owned save policy

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
| `load(slot)` | Request token; validates content and builds a private world. Publication replaces the world once. |
| `delete(slot)` | Request token; removes current and previous-good generation of this slot only. |
| `refresh()` | Request token for bounded metadata listing. |
| `list()` | Detached cached metadata array; refresh explicitly after boot. No world construction. |
| `exists(slot)` | Checks cached listing, including incompatible/corrupt entries; not a synchronous filesystem probe. |
| `status(token)` | Detached status or `null` after record retirement (32 requests retained). |
| `cancel(token)` | `true` only while queued/preparing; reading/writing/committed operations cannot be cancelled. |
| `exclude(entity, excluded = true)` | Omit an explicitly ephemeral simple body or visual. Required modern components and references cannot be hidden behind this flag. |
| `reference(entity)` | Stable opaque reference string, or `null` for destroyed entity. Full-width IDs never go through Number. |
| `resolve(key)` | Fresh Entity or `null` if absent/unloaded/destroyed; never loads a region from a getter. |

Slot IDs are 1–64 ASCII letters/digits/underscore/hyphen, not paths. Names are
independent bounded Unicode strings. Metadata and script/session data must use
bounded plain JSON values, not wrappers, functions or closures. One operation is
accepted at a time per project session; conflicting requests/scene replacement
throw a useful busy error. Request records and cached lists do not survive process
restart; slot files do. A caller's entity may disappear while its write completes.

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
Strict content compatibility is the default; container 1 timestamp seconds have
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
native handles and solver caches rebuild. These guarantees belong to explicit
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
previous motion samples and numerical caches are not serialized gameplay state.
Use `restore` to reacquire wrappers and let the next fixed solve publish diagnostics.

Kinematic bodies retain current motion authority/physical state plus durable target
or velocity commands, including the target's remaining duration. Save/load does not
reapply authored initial velocity over a captured command. The legacy pose-delta
`.judasstate` save rejects changed authority/kinematic intent; use modern slots.
[Kinematic reference](entities.md#kinematic-motion-m71) · [Pose/physical reference](animation-ragdolls.md).
