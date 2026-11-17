# Copyable JudasJS examples

[Index](../JUDASJS.md) · [Getting started](getting-started.md) · [Practices](practices.md)

Import/register a script, attach an enabled slot and author its required properties,
assets, actions and components. These are scripts, not standalone games. Clip/joint/
entity names are fixture content, not engine conventions. Relative modules also
need registered identities. Never infer that a retired lab project is bundled.

All shipped examples have source/type/signature checks. The focused
`judas_cookbook_tests` executes minimal lifecycle, profiling, queries, off-centre
impulses, presentation and surface enumeration through the actual VM/world.
Other native/subsystem regressions have their own declared scopes; type checks
alone do not establish every example's behaviour or device support.

| File | Required setup / task |
|---|---|
| [minimal.js](examples/minimal.js) | Any entity; lifecycle/properties/state. |
| [sprite-starter.js](examples/sprite-starter.js) | New 2D project template authors a sprite, image and script. Free XY movement and script-owned orthographic pixel-perfect view; no hidden planar collision. Executed by `judas_sprite_integration_tests`, with separate pixel/render and cold-process checks. [Workflow](sprites-2d.md). |
| [first-project.js](examples/first-project.js) | Render-only box at `(0,2,0)`; explicit fixed camera and visible rotation, no input/controller dependency. The focused `judas_first_project_tests` case verifies new-project serialization, real VM execution and failed/successful reload. [Complete steps](../FIRST_PROJECT.md). |
| [input-motion.js](examples/input-motion.js) | Any movable visual entity; `move_x` axis. Explicit teleport example, not collision locomotion. |
| [spawn.js](examples/spawn.js) | Registered prefab ID property; constructs independent hierarchy. |
| [queries.js](examples/queries.js) | Floor below origin, colliders; ray and sphere cast, ignore owner. |
| [paired-stick.js](examples/paired-stick.js) | `flick_stick` paired right-stick binding; frame raw/circular/delta display and fixed-step ordered observations without stealing another reader's samples. |
| [ray-fan.js](examples/ray-fan.js) | Owner facing a wall/ledge (local -Z), `compare_rays` action; 100 ordered batch hit/miss results and an optional same-world scalar comparison. |
| [kinematic.js](examples/kinematic.js) | Ordinary box, sphere, convex or compound rigid body; velocity, complete target, stop and point velocity through public Entity commands. Type-checked; requires the stated authored setup. |
| [materials.js](examples/materials.js) | Existing Render component and registered fixture prefab; numeric/whole material patches, alpha/texture removal, independent visibility, sun/ambient writes and reset. Stable-part targeting runs when a ready imported model supplies parts. See [current runtime appearance](../RUNTIME_RENDERING.md). |
| [contacts.js](examples/contacts.js) | Dynamic collider resting on floor; enter/stay/exit reactions. Use same script on authored sensor for triggers. |
| [audio.js](examples/audio.js) | Authored AudioEmitter; play/pause/resume/stop requests. |
| [particles.js](examples/particles.js) | Authored visual ParticleEmitter. |
| [ui.js](examples/ui.js) | Registered `.judasui` ID property with `counter`, `start`; runtime loading and UI broadcast filtering. |
| [scene-session.js](examples/scene-session.js) | Ordinary registered project; stores session facts and requests reload once. |
| [joint.js](examples/joint.js) | Owner of authored hinge/slider joint; bounded motor. |
| [animation.js](examples/animation.js) | Skinned asset with Wave/Stretch clips and Root/Elbow/Tip keys (change properties/content for your skeleton). |
| [ragdoll.js](examples/ragdoll.js) | Authored animation + mapping; physical entry, impulse, visual return. |
| [multi-target-ik.js](examples/multi-target-ik.js) | Explicit rig matching `tests/fixtures/ik/configuration.judas` (source fixture); explicit shared four-contact mapping, last-solve residuals, atomic target replacement and clear. |
| [physical-animation.js](examples/physical-animation.js) | The matching explicit full rig with ordinary mapped bodies and CharacterMotor; partial impulse, explicit full handoff, passive release and collision-validated animation return. |
| [physical-balance.js](examples/physical-balance.js) | Explicitly mapped balance-capable articulation; human stand/recover/passive requests and copied actual support/status. Type-checked; requires the stated authored setup. |
| [character.js](examples/character.js) | CharacterMotor, `move_x/move_y/jump` map; generic JS acceleration/launch. No camera ownership. |
| [wheel.js](examples/wheel.js) | Authored Wheel referencing a dynamic chassis; `drive`/`steer` axes and `brake` action. Executed by `judas_wheel_script_tests`; torque and suspension remain engine primitives. |
| [stale-handle.js](examples/stale-handle.js) | Prefab ID; validates stale Entity and retained Character behaviour. UI/Joint lifetime differences are documented separately. |
| [surface.js](examples/surface.js) | Any entity; reflects actual module/class surface for drift verification, not game behaviour. |

Focused documentation/type check:
`node scripts/check_judasjs_api.mjs --typescript /path/to/typescript/lib/typescript.js`.
It checks the current declarations and all cookbook files without running the engine.
The developer-only [type-contract fixture](type-contracts.ts) also checks accepted
partial main-view camera ranges and deliberate errors (missing cast distance,
raw-ID query ignores, unknown API names and incomplete vectors). It is compiled
only and must not be imported into a runtime project.
Add `--runtime /path/to/current/surface.json` when comparing a fresh actual-VM
enumeration. This verifies surface/types, not visual behaviour or every runtime branch.
| [physical-control.js](examples/physical-control.js) | See the matching subsystem reference for required assets/components and lifecycle; adapt fixture names to your project. |
| [impulse-point.js](examples/impulse-point.js) | See the matching subsystem reference for required assets/components and lifecycle; adapt fixture names to your project. |
| [presentation.js](examples/presentation.js) | See the matching subsystem reference for required assets/components and lifecycle; adapt fixture names to your project. |
| [navigation.js](examples/navigation.js) | See the matching subsystem reference for required assets/components and lifecycle; adapt fixture names to your project. |
| [profiling.js](examples/profiling.js) | See the matching subsystem reference for required assets/components and lifecycle; adapt fixture names to your project. |
| [localization.js](examples/localization.js) | See the matching subsystem reference for required assets/components and lifecycle; adapt fixture names to your project. |
| [streaming.js](examples/streaming.js) | See the matching subsystem reference for required assets/components and lifecycle; adapt fixture names to your project. |
| [saves.js](examples/saves.js) | See the matching subsystem reference for required assets/components and lifecycle; adapt fixture names to your project. |
| [fracture.js](examples/fracture.js) | See the matching subsystem reference for required assets/components and lifecycle; adapt fixture names to your project. |
| [developer-integration.js](examples/developer-integration.js) | See the matching subsystem reference for required assets/components and lifecycle; adapt fixture names to your project. |
| [imported-model.js](examples/imported-model.js) | See the matching subsystem reference for required assets/components and lifecycle; adapt fixture names to your project. |
| [authoring-runtime.js](examples/authoring-runtime.js) | See the matching subsystem reference for required assets/components and lifecycle; adapt fixture names to your project. |
| [signals.js](examples/signals.js) | See the matching subsystem reference for required assets/components and lifecycle; adapt fixture names to your project. |
| [mechanical-damage.js](examples/mechanical-damage.js) | See the matching subsystem reference for required assets/components and lifecycle; adapt fixture names to your project. |
| [ui-controls.js](examples/ui-controls.js) | See the matching subsystem reference for required assets/components and lifecycle; adapt fixture names to your project. |
| [terrain.js](examples/terrain.js) | See the matching subsystem reference for required assets/components and lifecycle; adapt fixture names to your project. |
| [save-evolution.js](examples/save-evolution.js) | See the matching subsystem reference for required assets/components and lifecycle; adapt fixture names to your project. |

## Adapt and run

Use the matching [typed API](../judas.d.ts) for exact options and the subsystem
reference for coordinate, phase, ownership, error and save rules. Read the example's
properties/comments before attaching it. Imported-model examples need a cooked
model with the explicitly requested clips; native IK fixture mappings require
source assets with matching keys. Supply your own compatible content rather than
expecting private original models or retired lab directories.

For the fully editable SDK example, open
[Developer Workflows](examples/developer_workflows/README.md). Use
[the first-project tutorial](../FIRST_PROJECT.md) for a minimal visible application.
Static checks run with `node scripts/check_judasjs_api.mjs` from the source checkout;
optional real-VM enumeration checks surface membership, not every method's semantics.
