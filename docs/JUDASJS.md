# JudasJS — Alpha v1.0.0 candidate API reference

[Practical ownership, readiness, presentation and save lessons](DEVELOPER_WORKFLOWS.md)

Judas provides engine primitives. Project JavaScript supplies game behaviour.
This reference describes the current repository implementation, including the public
module developed through M100 and the preceding accepted Pre-Alpha corrections. It is not an
eternal compatibility promise or an Alpha release announcement.

**Start here:** [Your first visible project](FIRST_PROJECT.md) →
[Making a game by hand](judasjs/getting-started.md) →
[Copyable cookbook](judasjs/cookbook.md).

For editor completion, copy [judas.d.ts](judas.d.ts) into your project and follow
[IDE setup](judasjs/practices.md). Judas remains JavaScript; TypeScript, npm and
Node are optional developer tools and are not part of the game runtime.

## Find an API

| Topic / searchable names | Current reference |
|---|---|
| Module imports, properties, start, restore, update, fixedUpdate, presentationUpdate, uiUpdate, destroy | [Lifecycle](judasjs/lifecycle.md) |
| Entity, transform, tags, spawnPrefab, scriptState, physical materials | [Entities and prefabs](judasjs/entities.md) |
| Entity.renderVisible, rendererVisible, imported-part visibility | [Render visibility](judasjs/entities.md#render-visibility-m72) |
| Entity.motionType, moveKinematic, setKinematicVelocity, pointVelocity | [Kinematic bodies](judasjs/entities.md#kinematic-motion-m71) |
| input, time, console, touch, paired sticks | [Input and time](judasjs/input.md) |
| physics.raycast, sphereCast, capsuleCast, boxCast, closestPoint, batched casts, Joint, collision/trigger callbacks | [Physics, queries and joints](judasjs/physics.md) |
| Entity.collider, classification, safe query witnesses | [Collider inspection](judasjs/entities.md#entitycollider) |
| Entity.gravity, physics.gravity, FaithfulGravity / RadicalGravity, scripted selection | [Gravity](judasjs/physics.md#gravity-physical-materials-and-runtime-joint-configuration-m65) · [Selection guide](GRAVITY_SELECTION.md) |
| CharacterMotor / entity.character, support, intent, query capsule | [Character](judasjs/character.md) |
| navigation, NavigationAgent, links, obstacles | [Navigation](judasjs/navigation.md) |
| Animation, clips, events, rootMotion, layers, jointTransform, limb, configureIK | [Animation and IK](judasjs/animation-ragdolls.md) |
| Ragdoll, physical animation, configureBalance, requestBalance | [Ragdolls and physical balance](judasjs/animation-ragdolls.md#ragdollconfigurebalance-requestbalance-cancelbalance-balancestate-m77) |
| Wheel suspension, torque, braking, steering, contact state | [Wheels](judasjs/wheels.md) |
| Powertrain sources, shafts, gears, clutches, differentials | [Powertrains](judasjs/powertrains.md) |
| Fracture, physical parts, interface failure, plastic observations | [Fracture](judasjs/fracture.md) |
| Deformable, cloth/solid forces, attachments, current-surface picking | [Deformables](judasjs/deformables.md) |
| liquid, LiquidVolume, conserved reservoirs, dynamic surfaces | [Liquid](judasjs/liquid.md) |
| Terrain add/remove/smooth, asynchronous collision/navigation publication | [Runtime volumetric terrain](judasjs/terrain.md) |
| world.origin, requestOrigin, Planet terrain, collision interest | [Planetary terrain and local origins](judasjs/planetary-terrain.md) |
| Weather, profiles, spatial wind, transitions, storms | [Weather](judasjs/weather.md) |
| Audio emitters, one-shots, groups, cursors, acoustics | [Audio](judasjs/audio.md) |
| Particles, cameras, world.setView / project / viewRay, world.fluidSample | [Effects and cameras](judasjs/effects-camera.md) |
| Individual lights, multiple views, rendering selection | [Rendering](judasjs/rendering.md) |
| Material, world.appearance, sun, ambient, environment | [Materials and scene appearance](judasjs/materials.md) |
| ui, UIDocument, UIElement, text fields, lists, focus, onUI | [Runtime UI](judasjs/ui.md) |
| localization, Unicode layout, catalogs/fonts | [Localization and text](judasjs/localization.md) |
| scenes.load / reload, session, safe handles, script state | [Scenes, state and lifetimes](judasjs/scenes-state.md) |
| scenes.requestRegion, ownership, pins, suspension | [Additive streaming](judasjs/streaming.md) |
| saves, durable references, restore, migration and recovery | [Project save slots](judasjs/saves.md) |
| signals, queued slot-owned messages, onSignal | [Signals](judasjs/signals.md) |
| players, LocalPlayer, PlayerInput, devices, viewports and HUDs | [Local multiplayer](judasjs/local-multiplayer.md) |
| network, NetworkEntity, sessions, authority, replicated state | [Online multiplayer](judasjs/network.md) |
| mods.active, contributions, resolve | [Read-only mod catalog](judasjs/mods.md) |
| profiler.scope, profiler.counter | [Script instrumentation](judasjs/profiling.md) · [Integrated profiler](PROFILER.md) |
| JSDoc, editor setup, conventions | [Practices](judasjs/practices.md) |

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
  in [developer tools](M94_DEVELOPER_TOOLS.md). Source edits do not trigger automatic
  hot reload, and arbitrary closures/native handles are not reloadable save state.

Capabilities have explicit composition limits. Origin shifting, region suspension,
modern saves and online replication support different subsets; a method's existence
is not proof that every component combination is supported. Consult each subsystem
page and the [current Pre-Alpha support/issue guide](PREALPHA.md).

## Authority, verification and history

The public wrapper is registered in [ScriptSystem.cpp](../src/ScriptSystem.cpp).
Native dispatch also uses [UI](../src/ScriptBindingsUI.cpp),
[joint](../src/ScriptBindingsJoint.cpp),
[local-player](../src/ScriptLocalMultiplayer.inl) and
[network](../src/ScriptNetwork.inl) bridges, plus the runtime systems they call.
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

Milestone reports and `docs/evidence/` preserve the requirements, failures and
acceptance scope at those checkpoints. They are historical records; use this
reference for current scripting behaviour. M98/M99 combined human acceptance is
[recorded separately](evidence/m99/HUMAN_VALIDATION.md). Later automated checks do
not invent human, controller, hardware or platform acceptance. Current portability
and outstanding qualification are tracked in [building/onboarding](FIRST_PROJECT.md),
[Windows](WINDOWS.md), [Android](ANDROID.md) and [Pre-Alpha](PREALPHA.md).

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
