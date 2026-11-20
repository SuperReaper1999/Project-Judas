# JudasJS V2 API reference

[Native Scripting V2 architecture and extension guide](judasjs/scripting-v2.md).

[Practical ownership, readiness, presentation and save lessons](DEVELOPER_WORKFLOWS.md)

This reference describes the current repository implementation, including the public
module and subsystem-owned bindings. It is not an
eternal compatibility promise or an Alpha release announcement.

**Start here:** [Your first visible project](FIRST_PROJECT.md) →
[Making a game by hand](judasjs/getting-started.md) →
[Copyable cookbook](judasjs/cookbook.md).

For editor completion, copy [judas.d.ts](judas.d.ts) into your project and follow
[IDE setup](judasjs/practices.md). Judas remains JavaScript; TypeScript, npm and
Node are optional developer tools and are not part of the game runtime.

Experimental native 2D Stages 1–4: [sprites, orthographic cameras and editor templates](judasjs/sprites-2d.md). [Native planar physics and editable player presets](judasjs/physics-2d.md) are also available, with [sprite sheets, frame animation and events](judasjs/sprite-animation.md). The [tilemap painter](judasjs/tilemaps-2d.md) adds layered grid authoring with real collision. A [complete fresh-project workflow](2D_QUICKSTART.md) is exercised by the editable Beacon Grove game. See the [platformer/action-adventure/tile-RPG guide](judasjs/2d-genres.md) for parallax, sword queries, height/sorting and explicit feature limits. This is development toward v1.1.0, not a published release.

## Find an API

[Game settings and preferences](judasjs/settings.md) covers video, audio and controls.

## Execution and ownership essentials

- Use `update` for frame-relative mouse input, `fixedUpdate` for authoritative
  movement and physics, `presentationUpdate` for cameras following
  `Entity.presentedTransform`, and `uiUpdate` for menus while gameplay is paused.
  See [phase ordering](judasjs/lifecycle.md#callbacks-and-order).
- Ordinary positions use the current local float simulation chart. Explicit
  `world.toAbsolute` / `toLocal` conversions use the double absolute origin.
  There is no universal world up. Gravity direction and support normal are distinct.
- Tags, collision layers and render layers have different purposes. Queries do not
  silently use gameplay tags or render visibility as collision policy.
- Returned objects are usually detached snapshots. Assign through documented
  setters/methods; mutating a copied result does not publish native state.
- Handles do not extend native lifetime. Reacquire after scene replacement, cold
  load or region retirement. [Lifetime rules](judasjs/scenes-state.md#safe-handle-rules)
  explain differences between Entity, Joint, UI and epoch-owned components.
- Scripts are trusted QuickJS code, without Node, browser DOM, npm packages,
  timers or a general asynchronous callback scheduler. They are not a malicious-code
  security sandbox. The internal `__judas` dispatcher is unsupported.
- Editor breakpoints, stepping, inspection and deliberate VM reload are described
  in [developer tools](SCRIPT_DEBUGGING.md). Source edits do not trigger automatic
  hot reload, and arbitrary closures/native handles are not reloadable save state.

Capabilities have explicit composition limits. Origin shifting, region suspension,
modern saves and online replication support different subsets; a method's existence
is not proof that every component combination is supported. Consult each subsystem
page and the [current Pre-Alpha support/issue guide](DEVELOPMENT_READINESS.md).

## Authority, verification and history

The [scripting catalogue](../src/scripting/Modules.json) owns native registration
and the embedded public wrapper fragments. Subsystem handlers live under
[src/scripting/modules](../src/scripting/modules), with existing UI, joint and AI
helpers retained behind their adapters. [ScriptSystem](../src/ScriptSystem.cpp)
coordinates the one runtime and lifecycle; [Scripting V2](judasjs/scripting-v2.md)
explains the extension workflow.
C++-only systems are not automatically scriptable.

[API inventory](judasjs/API_INVENTORY.md) and its
[machine manifest](judasjs/api-inventory.json) map runtime bindings to declarations
and reference pages, including explicitly limited dynamic checks.

For optional source/type verification from a clean checkout:

```sh
npm ci --prefix tools/verification
node scripts/check_judasjs_api.mjs
```

The checker validates the public surface, callable argument counts, selected
structured shapes and all cookbook types. `--runtime path/to/surface.json` also
compares a fresh actual-VM enumeration. This is not full behavioural certification;
[the cookbook](judasjs/cookbook.md) identifies runtime fixtures and limitations.

## Human developer review

1. Find raycast, Joint, Animation, Ragdoll and CharacterMotor from the table.
2. Open `judas.d.ts` in an editor and check useful JS completion.
3. Copy and run a cookbook script with its stated asset/component setup.
4. Compare its signatures and results with familiar runtime behaviour.
5. Confirm fixed/frame/presentation phases, safe lifetimes and scene/session
   persistence are understandable.
6. Confirm arbitrary gravity and the engine/gameplay boundary remain clear.

Project-wide water policy and basic/experimental water ownership: [Project Settings and water](PROJECT_SETTINGS_AND_WATER.md).

Engine/version provenance is available through the read-only [`engine.buildInfo`](judasjs/engine.md) API. It does not grant save or network compatibility.
