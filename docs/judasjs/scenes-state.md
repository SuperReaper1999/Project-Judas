# Scene/session/state and safe lifetimes

[Index](../JUDASJS.md) · [Lifecycle](lifecycle.md) · [Entities](entities.md)

## scenes

`scenes.current` is project-relative scene name; `registered` is sorted unique
registered startup/Scenes-directory `.judas` paths. `load(name)`/`reload()` return
true for an accepted request, or throw TypeError when unregistered/session ending.
A later valid request while one is pending also returns true but does not replace
it: **first valid request wins** until the safe outer application boundary.
Acceptance is not synchronous load completion. Invalid candidate builds preserve
the live world and report failure through normal application diagnostics.

Replacement builds a fresh normal RuntimeWorld, ends the old Play/VM/audio/UI,
then starts the new world. Reload reconstructs authored state, not an automatic
save overlay. No old entities, closures, UI, animation mixer or ragdoll state survive.
Whole-scene replacement is distinct from [M59 additive residency](streaming.md). Scene operations require an active
project SceneSession; isolated no-session worlds throw.

## session

`get(key)` returns a detached JSON value or null if absent. `set(key,value)` and
`delete(key)` return undefined. Keys are strings; set requires 1..128 bytes,
maximum 256 keys/64 KiB combined. Values obey the same bounded plain-data checks
below. Invalid values/budgets throw TypeError. get/delete missing keys are safe.
Session data survives scene replacement/reload but ends when Play/application
session ends. It is not automatically disk-persistent; explicit [M61 slots](saves.md)
include the bounded session map. Re-fetch values after modification:
mutating `session.get('x')` does not update the stored value.

## this.state and disk persistence

An instance receives `{}` if no state is set; explicit constructor state survives
unless saved state replaces it. Store bounded plain JSON: null/booleans/finite
numbers/strings/arrays/plain objects (null prototype allowed), depth <=16,
<=4096 visited values, <=64 KiB serialized per slot. No functions/accessors/symbols,
cycles/custom prototypes, Entity/UI wrappers, BigInt or VM pointers. Save state root
must be an object/array for restore; prefer an object. Do not put handles in state;
store [durable references](saves.md) and reacquire/validate, or game facts.

Engine M29 captures `this.state`, keyed by entity/slot, validates the authored
baseline and restores before `start`. Invalid captured state prevents valid save
serialization; faulted slots are omitted. `entity.scriptState(slot)` reads a
detached JSON snapshot or null. This is not live inter-script object sharing.

**Historical M40–M60 contract, superseded for modern slots by M61:** no JS save/load-disk binding existed. The runtime/editor's existing save controls
are separate. Session is not saved automatically. Private instance fields/module
globals/animation mixer/active ragdoll/UI/live voices are not VM-persisted. Authored
script/UI/prefab content contributes strict content fingerprints; don't assume
save compatibility after changing source. [Persistence architecture](../SCRIPTING.md).

## Safe handle rules

| Surface | Missing/stale behaviour |
|---|---|
| `entity(id)` / `world.entity(id)` | Null for falsy/"0", otherwise constructs even if stale. `.valid` checks existence. |
| Entity operations | Stale generally ReferenceError; dynamic-body methods TypeError if no dynamic body. |
| `entity.character` | Null if stale/missing motor; retained Character operations ReferenceError. |
| `entity.animation` / `.ragdoll` | Null if valid owner lacks component; stale owner throws. Retained facades throw. |
| `entity.material(slot)` | Constructs a facade; methods validate owner/render/slot. Stale owner ReferenceError; absent component/invalid slot TypeError. |
| `entity.renderVisible` / `rendererVisible` | Local render-only booleans. Stale owner ReferenceError; renderer gate additionally requires an existing Render component. |
| `entity.deformable` / `.fracture` | Null until ready or absent; `.valid` false for stale retained epoch handles, other stale operations ReferenceError. |
| `entity.liquid` | Null until registered or absent; retained `.valid` false when stale, state/control ReferenceError. |
| `entity.navigation` | Null without agent; facade exists while disabled. Disabled/unregistered state/control ReferenceError; enabled setter can restore it. |
| `physics.joint(owner)` | Null if no live joint; `.valid` false on stale Joint; state/mutations throw. |
| `ui.get(name)` | Null if absent. Document/element stale accesses throw; no valid flag. |
| Region request tokens | `regionStatus` null / activation and release false for retired or old-world tokens. |
| Save tokens / durable references | Request status can retire to null; request IDs are not persisted. `saves.resolve` returns a fresh wrapper/null without loading missing regions. |
| Query hits/support/collision other | Historical data and wrappers can go stale; test `entity && entity.valid`. |
| Component commands | Valid owner but absent audio/particles/camera often false/null; see reference. |

Entity wrappers resolve full IDs against THEIR VM's world each call; rigid body
slots are reacquired generation-safely. Joint/UI lifetime rules differ. None are
cross-scene handles: new VM/new scene means reacquire even if authored ID text is
identical. Constructor/handle fields are ordinary JS data, not immutable native
capabilities; treating IDs as opaque is a usage convention. The internal `__judas`
bridge is unsupported. There is no independent public Body class.
Modern disk references are opaque qualified identities, not retained wrappers;
store `saves.reference(entity)` in bounded state and call `saves.resolve` again.
Scene replacement ends native ownership; additive region suspension can also
invalidate wrappers while the one VM stays live. Reacquire region entities after
publication instead of assuming identical local IDs preserve runtime identity.

Destroy invalidates entity lookup immediately; destroying script slots itself is
retired at synchronization, not a promise that `destroy()` runs recursively before
the call returns. `destroy` may see an already-stale owner. Keep teardown defensive;
faulted scripts receive no destroy callback. Support-body disappearance is handled
by the motor's safe generations, not explicit transform parenting.

## M59 additive residency

For additive region requests, ownership, pins and suspension see [Streaming](streaming.md). Whole-world reload remains distinct. The historical legacy delta save remains disabled for composed worlds; M61 slots now include qualified resident/retained state.

## Project slots (M61)

The reusable [`saves`](saves.md) service persists the bounded session map and supported world state. It stages a coherent replacement and never overlays a legacy delta afterward. Session-only data remains distinct from durable disk state.

M73 [signals](signals.md) are world-local transient delivery, not session/save facts.
World replacement discards subscriptions/queues. Re-register in restore; persist
required intent explicitly rather than relying on replay of accepted notifications.
