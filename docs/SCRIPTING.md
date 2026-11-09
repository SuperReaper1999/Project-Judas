# JavaScript gameplay and the M40 foundation

[Native Scripting V2 architecture and extension guide](judasjs/scripting-v2.md).

For complete current signatures, examples and lifetime rules use the
[JudasJS reference](JUDASJS.md), [first project](FIRST_PROJECT.md) and
[lifecycle guide](judasjs/lifecycle.md). This page explains the shared scripting
foundation and labels the older M40 save path separately; it is not a second
complete API inventory.

Judas embeds **QuickJS-NG v0.17.0** (MIT). Core sources and the complete
license are under `third_party/quickjs`; runtime exports include its credits
and license in `engine/third_party/RUNTIME_NOTICES.txt` through the existing
exporter. No Node, browser, CLI or QuickJS libc/OS modules are shipped.

## Authoring

Import/track `.js` in the project's Asset Browser. Add **Scripts** to an entity,
select an asset, then edit the declared boolean/number/string/entity fields. Add more
behaviours for ordered slots; persistent slot IDs are separate from slot order.
Prefab source/instance edits use normal component serialization and overrides.
Properties may store asset IDs as strings; entity properties use the documented
[inspector entity-reference schema](judasjs/lifecycle.md). Editing a property does not rewrite JS.
Restart Play for a clean world, or use [M94 deliberate script reload](SCRIPT_DEBUGGING.md). It can retain bounded JSON instance state; no arbitrary live object/closure migration is provided.

```js
import {input, world, console} from 'judas';
export const properties = {speed: {type: 'number', default: 2}};
export default class Behaviour {
  constructor({entity, properties}) {
    this.entity = entity;
    this.properties = properties;
    this.state = {count: 0};
  }
  start() {}                      // new session / legacy delta initialization
  restore() {}                    // modern loaded slot, instead of start
  update(dt) {}                   // once per active outer gameplay frame
  fixedUpdate(dt) {               // before ordinary fixed-step forces/physics
    if (input.pressed('interact')) this.state.count++;
  }
  presentationUpdate(dt) {}       // presentation poses / camera, not physical intent
  destroy() {}
}
```

Lifecycle callbacks are optional and synchronous; UI, collision/trigger and
animation-event callbacks are documented in the current reference. A default
exported class gets one independent instance per slot. Constructors initialize
instances; put gameplay in callbacks. Relative imports must resolve to registered
project `.js` assets. Bare imports other than `judas` and native/absolute imports
are rejected. ES module globals are shared by that Play session, intentionally;
`this.state` and constructor properties are per instance. No per-frame compilation.
Asynchronous callbacks/top-level await are unsupported; no promise scheduler.

One engine-owned `ScriptSystem` lives in each played `RuntimeWorld`. Calls run on
the runtime/main thread, outside renderer/physics ownership. Both standalone and
editor Play call the same `InteractivePlay` frame and `StepPlayedWorld` fixed boundaries.
Menus pause gameplay callbacks; M41 uiUpdate/onUI remain active for UI. Fixed input uses M35's latched snapshot, including short
presses across zero-step frames, without consuming another system's snapshot.
Entities run by stable ID, then authored slot order. Spawns join at the next
callback boundary; destruction invalidates handles immediately. Stop destroys
instances/context before world resources. No scripting VM is created for worlds
without script slots.

The inspector evaluates module metadata in a **no-world context**, with no
constructors or gameplay callbacks. Top-level module code must be declarative;
world calls there fail. Metadata is cached until its selected source file changes.
Imported metadata helper edits require reselect/reopen/restart (no live reload).

## Public API

[`docs/judas.d.ts`](judas.d.ts) describes the public `judas` module. Common
vectors/quaternions are plain objects; some native configuration objects also
accept arrays as documented in their subsystem reference. Entity IDs are
**decimal strings** (runtime IDs exceed JS safe integers).
Wrappers resolve ID against their own live world on every call. They retain no
body pointer or native lifetime; physics calls reacquire generation-checked handles.

- `entity`, `world.entity`, `world.queryTags`, `world.spawnPrefab`, `world.overlap`,
  `world.sweepCapsule` (existing player-shape capsule, **not arbitrary capsules**).
- Entity transform, parent/children, destroy hierarchy, tags/classification,
  controlled state snapshot by slot, force/impulse/torque and velocities.
- Named input held/pressed/released/axis; time delta, fixed flag, authoritative
  fixed simulation elapsed time. No physical SDL keys or wall clock.
- Authored audio play/stop/pause/resume/enable and status; particles burst/enable/rate;
  existing render-target camera inspection/enable. M49 additionally exposes world.setView/clearView for independent script-owned main-view intent;
  see the current camera reference.
- Query include/exclude layer names, required/excluded tags and ignored entities.
  `world.overlap` returns **conservative broadphase candidates**, not exact
  intersections. M44's explicit casts and closest-point queries have their own
  shapes and result semantics; see [physics](judasjs/physics.md).

The list above is the original scripting foundation, not the current complete
surface. Animation/IK, ragdolls, CharacterMotor, navigation, terrain, liquids,
weather, wheels, powertrains, networking, saves and other current exports are
indexed in [JUDASJS.md](JUDASJS.md). The
[binding inventory](judasjs/API_INVENTORY.md) records verification coverage.

Transform writes use ordinary authoritative bodies/runtime components; body pose
writes are **teleports**, not continuous kinematic trajectories. Scale is visual,
consistent with Judas's explicit collision geometry. Force APIs require an active
dynamic body. Existing runtime-spawn component limits remain unchanged.

## Persistence and failures

`this.state` supports bounded plain JSON: null, booleans, finite numbers, strings,
arrays and objects. Maximum depth 16, 4096 visited values and 64 KiB serialized per
slot (4096 saved slots per world); no cycles/accessors/functions/custom prototypes. Invalid state rejects saving
rather than dropping it or serializing the VM. Authored properties are separate.

### Modern slots

Use the public [`saves` service](judasjs/saves.md) for cold-process persistence.
It captures the current supported native participants, script JSON state and
session state through the existing owner-thread/worker publication boundary.
The default is exact-content compatibility. M98 adds explicit developer-owned
[save migration policy and recovery](SAVE_EVOLUTION.md); changing registered
code does not automatically authorize loading an old save. Reloaded scripts
require Stop/Play before compatible saves; see [developer tools](SCRIPT_DEBUGGING.md).
Unsupported participants and incomplete historical continuation data remain
explicit errors/limitations, not silently reconstructed state.

### Current persistence only

Modern SaveService slots preserve supported runtime/script state. Native pose-delta `.judasstate` saves and their script-state extensions are retired. There is no migration or compatibility overlay; open historical projects with their matching historical engine.
