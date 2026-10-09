# Making a Judas game by hand

[API index](../JUDASJS.md) · [Lifecycle](lifecycle.md) · [Cookbook](cookbook.md) · [IDE setup](practices.md)

Judas runs ordinary project JavaScript. You author entities/components/assets and
use scripts to supply game rules. No C++ recompilation is needed to change those
rules. This guide describes the current API including the M71 candidate; its
human-review status is on the [landing page](../JUDASJS.md).

## 1. Create an ordinary project

Open the current `judas_editor` and choose **File → New project…**. Supply a
directory and game name. The editor creates a `.judasproj`, `Assets/`,
`Scenes/main.judas` and `Saves/`, then opens the starter scene. The scene contains
a floor, uniform-gravity region, light and a historical Player start marker.
New projects use `legacy-gameplay false`: that marker does **not** supply a modern
character, input policy or camera. Add those explicitly when your game needs them.

A small project can look like this:

```text
MyGame/
  MyGame.judasproj
  Assets/
    scripts/main.js
    scripts/main.js.judasmeta
    ui/hud.judasui
    ui/hud.judasui.judasmeta
  Scenes/main.judas
  Types/judas.d.ts         # optional editor tooling
  jsconfig.json           # optional editor tooling
  Saves/
```

Use the inspector and **Create** menu for scene content. The named source editor
under **View → World building / named source** supports direct document editing;
see [formats, field names and CLI](../NAMED_AUTHORING.md). These are authoring
formats, not APIs you import into the game VM.

## 2. Write, register and attach a script

1. Save a UTF-8 `.js` file under your project's `Assets/scripts/` directory.
2. Open **View → Asset Browser**, press **Rescan**, then **Track** the script in
   the untracked list. This creates its stable `.judasmeta` identity.
3. Select an entity, choose **Add component… → Scripts**, and choose the script
   asset for its enabled behaviour slot. **Add behaviour** adds another slot.
4. Save the scene. Press **Play scene** / F5. **Stop** / F5 returns to authored state.

Start with the complete, executed [minimal.js](examples/minimal.js): copy it to
your own asset, attach it to any entity and change its inspector `label` property.
It logs at startup and keeps independent frame/fixed-step counters in `this.state`.
Logging goes to the application's stdout; launching the editor from a terminal
keeps that output visible. Errors report the script asset, entity, slot and callback.

Imports use `import {input, physics, world} from 'judas'`. Import only the names you
need. Shared helpers use relative ES imports such as `./math.js`; those `.js`
files must also be tracked. A helper module needs no default class unless you
attach it as a behaviour. There is no Node, npm, browser DOM, timer service or
general async callback runtime. Top-level module code also runs during inspector
metadata inspection, without a world: keep native game work inside callbacks.

Code changes take effect in a fresh Play/scene world. Stop, save source, and Play
again; there is no script hot reload. Editing runtime objects does not rewrite
your authored scene.

## 3. Use properties, state and references deliberately

Export `properties` as a schema of number, boolean, string or entity fields.
Defaults supply values omitted by the authored slot. The inspector edits the
values passed into `constructor({entity, properties, restored, initialState})`.

```js
/** @type {import('judas').PropertySchema} */
export const properties = {
  speed: {type: 'number', default: 4},
  destination: {type: 'entity', default: null}
};
```

`properties.destination` is a safe Entity wrapper or null, selected with the
inspector's entity picker. On disk, a non-null reference is
`{entity:'decimal-stable-id'}`. Never find gameplay participants by their display
name or invent IDs. Asset APIs instead use stable asset IDs shown in the Asset
Browser. IDs are opaque strings; entity IDs can exceed safe JavaScript integers.

Keep per-instance mutable facts on `this.state`, not module globals. State is
bounded plain JSON; wrappers/functions/closures are not serializable. Use `session`
for facts surviving scene replacement and `saves` for explicit disk slots. Modern
loads install saved state after construction and call `restore` instead of
`start`; reacquire UI/entities there rather than spawning the starting world again.
[State and safe handles](scenes-state.md) · [Save slots](saves.md).

## 4. Put work in the correct phase

| Phase | Typical project work |
|---|---|
| `start` / `restore` | Acquire handles, construct presentation; restore skips new-game side effects. |
| `update(dt)` | Frame mouse delta and look policy; keep resulting intent for fixed steps. |
| `fixedUpdate(dt)` | Character velocity, forces, gameplay casts, fixed-only mutations and physical intent. |
| `presentationUpdate(dt, alpha)` | Camera/cosmetic follow using `presentedTransform`, after fixed catch-up. |
| `uiUpdate(dt)` / `onUI(event)` | Menus including paused frames; filter document and element. |
| Contact/trigger callbacks | React to completed physics observations with game policy. |
| `destroy` | Tolerate stale owners, release explicit interests/pins; no movement integration. |

There can be zero or several fixed steps per frame. Input edges are latched for
the next fixed step; reading the same edge in both frame and fixed code can
observe it twice. Do not integrate relative mouse motion once per catch-up step.
Ordering is described in [lifecycle](lifecycle.md), including M70 reference/final
pose phases. Returned transforms/results are snapshots: modify and assign a
transform explicitly. Physical transform assignment is a teleport; use a motor,
dynamic force/impulse or kinematic command for continuous motion.

## 5. Compose the engine primitives your game needs

| Goal | Author / use |
|---|---|
| Character locomotion | **Character motor** + script; [character.js](examples/character.js) uses `move_x`, `move_y`, `jump`. The script chooses speed/launch; the motor resolves collisions/gravity/support. |
| Main camera | Separate script-owned `world.setView`; [presentation.js](examples/presentation.js) shows interpolated following. Author your look/offset policy; a render-target camera is a different component. |
| Physical props/platforms | Body component; forces/impulses for dynamic bodies, [kinematic commands](entities.md#kinematic-motion-m71) for prescribed trajectories. Render mesh is not automatically a collider. |
| Probes, interaction, aiming | [Physics casts/filters](physics.md), then JS interprets hit entity/tags. Ignore the probing character where appropriate. |
| Connections | Authored or runtime [joints](physics.md#joint); JS chooses motor/spring policy. |
| HUD/pause | Registered `.judasui`; [ui.js](examples/ui.js), modal state and `input.pointerCapture`. UI consumes shared bindings; the script decides pause/resume meaning. |
| Feedback | Authored AudioEmitter / visual ParticleEmitter; [audio](audio.md), [particles](effects-camera.md#particles). |
| Animated actors | Imported skeletal mesh + Animation; [clips/layers/IK/physical regions](animation-ragdolls.md). Joint names/mappings belong to your asset. |
| Routes | Baked navigation surface + agent; [navigation](navigation.md) guides, a script drives the motor. |
| Liquid/deformables/fracture | Author their normal cooked resources/components; use the respective references, readiness guards and fixed mutation contracts. |
| Restart/next scene | Save a `.judas` in the project's scene directory; `scenes.reload/load` queues replacement at the safe outer boundary. |
| Independent instances | [Prefabs](entities.md#worldspawnprefab-construction-options-m67); asset data shares, runtime state does not. |

In **File → Project settings → Input actions, axes and vectors**, author logical
bindings and save the project. Scripts read names through `input`; they do not poll
keys directly. Review required names in each cookbook fixture. Unknown input names
read neutral rather than throwing. Gravity/support/camera up remain separate;
derive movement from entity/motor frames, not global world-Y.

For a complete inspectable game, [Spring Range](../../projects/shooter_game/shooter_game.judasproj)
composes character, first/third-person camera, queries, hinges, UI and feedback in
separate project scripts. [Character Lab](../../projects/character_lab/README.md)
shows current pose/physical composition. Their names and game rules are examples,
not engine conventions.

## 6. Get completion and ship the project

Copy the current [judas.d.ts](../judas.d.ts) into `Types/` and use the
[jsconfig/JSDoc setup](practices.md). Declarations are tooling only: do not import
interfaces or `.d.ts` at runtime. Keep their engine checkpoint in sync with the
runtime you develop against. All reference pages can be read offline.

Save source, scene and project settings. Select the startup scene in Project
settings; use **Run project** to exercise ordinary runtime startup. Then choose a
package directory outside the project and **Export project (Release)**. Export
uses saved files, not unsaved inspector edits. Copy the package elsewhere and run
its `judas` executable. [Export workflow](../M38.md) · [Windows status/requirements](../WINDOWS.md).

All registered assets is the safe default. If choosing dependency closure, declare
dynamic asset IDs (for example runtime-spawned prefabs) as project runtime roots;
source code strings are not a linker. [Export scene/asset selection](../NAMED_AUTHORING.md).
Packages contain runtime content, not the documentation tree or TypeScript tooling.
Keep this checkout's docs/declarations available while authoring.
