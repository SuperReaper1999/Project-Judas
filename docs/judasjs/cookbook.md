# Copyable, executed examples

[Index](../JUDASJS.md) · [Practices](practices.md)

These are ordinary default-exported scripts, not pseudocode or hidden test paths.
Import/track a script in Assets, attach an enabled slot to an appropriate entity,
and set its properties in the inspector. Relative imports must also be tracked.
The focused `judasjs_examples_tests` target executes these files through the normal
QuickJS/RuntimeWorld path with the fixtures below. It does not replace the renderer,
physics or VM. Example names/IDs are content, not engine-owned semantics.

| File | Required setup / task |
|---|---|
| [minimal.js](examples/minimal.js) | Any entity; lifecycle/properties/state. |
| [input-motion.js](examples/input-motion.js) | Any movable visual entity; `move_x` axis. Explicit teleport example, not collision locomotion. |
| [spawn.js](examples/spawn.js) | Registered prefab ID property; constructs independent hierarchy. |
| [queries.js](examples/queries.js) | Floor below origin, colliders; ray and sphere cast, ignore owner. |
| [contacts.js](examples/contacts.js) | Dynamic collider resting on floor; enter/stay/exit reactions. Use same script on authored sensor for triggers. |
| [audio.js](examples/audio.js) | Authored AudioEmitter; play/pause/resume/stop requests. |
| [particles.js](examples/particles.js) | Authored visual ParticleEmitter. |
| [ui.js](examples/ui.js) | Registered `.judasui` ID property with `counter`, `start`; runtime loading and UI broadcast filtering. |
| [scene-session.js](examples/scene-session.js) | Ordinary registered project; stores session facts and requests reload once. |
| [joint.js](examples/joint.js) | Owner of authored hinge/slider joint; bounded motor. |
| [animation.js](examples/animation.js) | Skinned asset with Wave/Stretch clips and Root/Elbow/Tip keys (change properties/content for your skeleton). |
| [ragdoll.js](examples/ragdoll.js) | Authored animation + mapping; physical entry, impulse, visual return. |
| [character.js](examples/character.js) | CharacterMotor, `move_x/move_y/jump` map; generic JS acceleration/launch. No camera ownership. |
| [stale-handle.js](examples/stale-handle.js) | Prefab ID; validates stale Entity and retained Character behaviour. UI/Joint lifetime differences are documented separately. |
| [surface.js](examples/surface.js) | Any entity; reflects actual module/class surface for drift verification, not game behaviour. |

Development command: `python3 scripts/m50_validation.py --typescript /path/to/typescript/lib/typescript.js`.
Supply `--node /path/to/node` if Node is not on PATH; choose a fresh `--output`
directory for a later run. An installed TypeScript package can replace the explicit path.
It builds only the new focused test target against the existing Release engine,
checks declaration/example types and source/runtime coverage, then runs one current
character-project startup and real outer-frame scene/session reload smoke.
No production-suite rerun for documentation.
Results: [M50 evidence](../evidence/m50/README.md). Human review still matters.

## M51 physical control

[physical-control.js](examples/physical-control.js) executes on a dynamic-body
entity: project damping uses mass/inertia snapshots and requests pointer
capture. The complete workshop/flight compositions live in
`projects/boundary_demo/Assets/scripts/rules.js`; no private native calls are used.

M52 adds [impulse at a world point](examples/impulse-point.js): an off-centre impulse through the ordinary body API, with validation before mutation.

M52 [presentation.js](examples/presentation.js) follows a body's/motor's existing
interpolated world pose in the post-simulation presentation phase; it leaves
authoritative motion untouched. Attach to a moving body or CharacterMotor.

- [Navigation guidance into CharacterMotor](examples/navigation.js) ([reference](navigation.md)).

- [Conserved reservoir transfer and submersion](examples/liquid.js) ([reference](liquid.md)): attach to lab entity 10 with source 20 and storage 21 from the current liquid project. Waits for CPU resources and fixed-step registration, then transfers 800 L once and checks the material ledger.

- [Dynamic surface control](examples/liquid-surface.js) ([reference](liquid.md)): attach to entity 10 in `projects/liquid_surface_demo`; conserved drain/refill, momentum impulse, retained-state suspension and presentation sampling.

- [Exception-safe custom profiling](examples/profiling.js) — `profiler.scope` and finite counters, capture on or off.

## Localization / Unicode UI (M58)

[Executed localization example](examples/localization.js) uses the text-lab catalogs
and document. It preserves supplementary/combining UTF-8, named whole messages,
Russian plural rules and invalid-locale handling. [API and asset syntax](localization.md).

- [Additive preload/activation](examples/streaming.js) runs on a persistent root in `projects/streamed_range`; requests never block.

## M61 slots and durable references

[saves.js](examples/saves.js) uses the ordinary public service. Attach to an entity
in a registered project with supported modern components. Default policy queues a
save and polls later; the focused fixture cancels before capture. The separate M61
application proof executes committed saves and fresh-process restores. Never treat
`queued` as success, and wait for `completed` before exiting. [Save API](saves.md).

- [Fracture](examples/fracture.js): physical part impulse, an explicitly labelled
  tool cut, coherent callback state, independent prefab and stale-handle rejection.

## Developer integration (M65)

[Executed example](examples/developer-integration.js): final joint reads, IK, visual
socket, gravity sample, physical coefficients and generation-safe runtime joints.
Fixture is the original rig in [current integration project](../../projects/m65_integration/m65_integration.judasproj).

- [Imported parts and extracted clip interval](examples/imported-model.js)
  ([model workflow](../MODEL_IMPORT.md)): attach to the original import-lab model.
