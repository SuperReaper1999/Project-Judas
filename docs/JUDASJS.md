# JudasJS — current API reference (M67 provisional)

JUDAS PROVIDES ENGINE PRIMITIVES. JAVASCRIPT PROVIDES GAME BEHAVIOUR.

This reference describes accepted M66 plus the provisional M67 candidate, based on
checkpoint `b9a8cf3b8d8cd3786272480a8abe6cbe9fffe859`. It is not an eternal
compatibility/semantic-version promise. Source authority is `src/ScriptSystem.cpp`
and the runtime systems it calls; demos do not define API.
M67 is checkpointed provisionally by operator authorization; proper human validation
remains deferred rather than claimed complete.
M67 adds generic camera projection, transient UI layout and prefab construction
options; see [supported authoring](NAMED_AUTHORING.md) and the
[executed example](judasjs/examples/authoring-runtime.js).
M50 introduced the documentation/tooling. M51 adds generic mass/inertia snapshots
and pointer capture intent; see [engine/game boundary](M51_ENGINE_BOUNDARY.md).
M52 exposes the existing generic impulse-at-point body operation and render pose
through `Entity.presentedTransform` / `presentationUpdate(dt, alpha)`; the
[Spring Range project](M52_SHOOTER_GAME.md) implements all shooting/score rules in JS.

M59 added optional additive residency through [scenes streaming](judasjs/streaming.md).

## Find an API

| Topic | Reference |
|---|---|
| Imports, properties, start/restore/update/fixedUpdate/presentationUpdate/uiUpdate/destroy | [Lifecycle](judasjs/lifecycle.md) |
| Fracture, physical parts, interface failure and explicit removal | [Fracture](judasjs/fracture.md) |
| Deformable, cloth/solid forces, attachments and current-surface picking | [Deformables](judasjs/deformables.md) |
| Entity, transform, tags, spawnPrefab | [Entities/prefabs](judasjs/entities.md) |
| profiler.scope / profiler.counter | [Custom diagnostics](judasjs/profiling.md) |
| input, time, console | [Input/time](judasjs/input.md) |
| physics.raycast/sphereCast/capsuleCast/boxCast/closestPoint, Entity.collider, Joint, contacts/triggers | [Physics](judasjs/physics.md) · [Collider inspection](judasjs/entities.md#entitycollider) |
| Audio, particles, camera, world.fluidSample | [Effects/view](judasjs/effects-camera.md) |
| ui, UIDocument, UIElement, onUI | [Runtime UI](judasjs/ui.md) |
| localization, Unicode layout, catalogs/fonts | [Localization/text](judasjs/localization.md) |
| scenes, session, script state, safe handles | [Lifetime/state](judasjs/scenes-state.md) |
| saves, durable references, restore lifecycle | [Project save slots](judasjs/saves.md) |
| Animation.crossFade/layers, Ragdoll | [Animation/ragdolls](judasjs/animation-ragdolls.md) |
| entity.character / CharacterMotor | [Character](judasjs/character.md) |
| navigation / NavigationAgent | [Navigation](judasjs/navigation.md) |
| liquid / LiquidVolume / conserved reservoirs | [Liquid](judasjs/liquid.md) |
| entity.material, scene lighting/environment overrides | [Materials](judasjs/materials.md) |
| Executed scripts | [Cookbook](judasjs/cookbook.md) |
| JSDoc, editor setup, practical conventions | [Practices](judasjs/practices.md) |

## Tooling and completeness

- [judas.d.ts](judas.d.ts): offline completion/type information for JS editors;
  no TypeScript runtime dependency.
- [API inventory](judasjs/API_INVENTORY.md) and [machine manifest](judasjs/api-inventory.json):
  actual bindings → declarations → reference, native bridge operations and explicit
  internal/dynamic verification exceptions.
- `scripts/check_judasjs_api.mjs`: source AST, declaration AST, inventory/reference
  coverage, standard TypeScript checking and actual-VM enumeration comparison.
- Existing milestone records remain architecture/history; start here for CURRENT
  JS usage. No raw C++-only system is implied to be scriptable.

## Important boundaries

Use fixedUpdate for authoritative movement/physics, frame updates for relative
mouse input, presentationUpdate for render-following poses/cameras, and uiUpdate
for menus while paused. Worlds have fixed local
float coordinates with a double absolute origin; no automatic live rebasing or
universal world up. Tags, collision layers and render layers are distinct.
Read returned snapshots; assign updates through explicit APIs. Safe wrappers do
not extend native lifetime and cannot cross scene replacement.

Scripts are trusted QuickJS code, not Node/browser/npm or a malicious-code sandbox.
No arbitrary async callbacks, debugger/hot reload, raw bones, ECS, runtime layer
registry editing. Current subsystem limits are documented
on their pages rather than silently upgraded. Human hardware/visual/audio review
remains authoritative.

## Human developer review

1. Confirm this landing page and subsystem navigation are obvious.
2. Search raycast, Joint, Animation, Ragdoll and CharacterMotor.
3. Compare signatures/results with familiar runtime behaviour.
4. Open judas.d.ts in a TS-aware JS editor and inspect completion.
5. Copy/attach/run one cookbook script with its listed setup.
6. Check lifecycle/fixedUpdate explanations are understandable.
7. Check safe handles and scene/session lifetime rules are clear.
8. Check tags, collision layers and render layers remain distinct.
9. Check arbitrary-gravity coordinates do not assume world-Y.
10. Confirm engine primitives and game behaviour remain separate.

- [Navigation: queries, agents, obstacles and explicit links](judasjs/navigation.md)

## Diagnostic instrumentation (M56)

[profiler.scope / profiler.counter](judasjs/profiling.md) and [integrated profiler controls](PROFILER.md).

- [M57 materials, overrides and scene appearance](judasjs/materials.md)

## M58 Unicode text and localization

[Localization reference](judasjs/localization.md), [text/font integration](M58.md) and [executed localization example](judasjs/examples/localization.js). Current public surface includes M58.

## Streaming audio and acoustics

[Audio reference](judasjs/audio.md) covers M60 emitter settings, cursor/readiness,
independent one-shots, authored groups/fades and approximate spatial effects.
[Listening lab](../projects/audio_lab/README.md) provides ordinary authored content.

## M61 save/load

[Project slots, durable references and restore lifecycle](judasjs/saves.md) | [Subsystem participation and storage](M61_SAVES.md).

M64 adds [collider snapshots / closest surface queries](M64_COLLISION.md) through the existing physics/entity APIs.

M65 adds resolved joints/limb IK/visual sockets, gravity samples, shared physical
materials/runtime joints and ordinary motor touch events. [Integration](M65_INTEGRATION.md)
and [named authoring](CONTENT_AUTHORING.md) provide current examples.

M66 candidate: [model import workflow](MODEL_IMPORT.md), [imported model cookbook](judasjs/examples/imported-model.js), exact part visibility and clip root-motion queries use ordinary entity/animation handles.
