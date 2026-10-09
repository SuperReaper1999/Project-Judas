# Copyable, executed examples

[Index](../JUDASJS.md) · [Making a game by hand](getting-started.md) · [Practices](practices.md)

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
| [paired-stick.js](examples/paired-stick.js) | `flick_stick` paired right-stick binding; frame raw/circular/delta display and fixed-step ordered observations without stealing another reader's samples. |
| [ray-fan.js](examples/ray-fan.js) | Owner facing a wall/ledge (local -Z), `compare_rays` action; 100 ordered batch hit/miss results and an optional same-world scalar comparison. |
| [kinematic.js](examples/kinematic.js) | Ordinary box, sphere, convex or compound rigid body; velocity, complete target, stop and point velocity through public Entity commands. Executed by the focused `judas_kinematic_lab_tests` target. |
| [materials.js](examples/materials.js) | Existing Render component and registered fixture prefab; numeric/whole material patches, alpha/texture removal, independent visibility, sun/ambient writes and reset. Stable-part targeting runs when a ready imported model supplies parts. M72 candidate execution status is in [the handoff](../M72.md#validation-status). |
| [contacts.js](examples/contacts.js) | Dynamic collider resting on floor; enter/stay/exit reactions. Use same script on authored sensor for triggers. |
| [audio.js](examples/audio.js) | Authored AudioEmitter; play/pause/resume/stop requests. |
| [particles.js](examples/particles.js) | Authored visual ParticleEmitter. |
| [ui.js](examples/ui.js) | Registered `.judasui` ID property with `counter`, `start`; runtime loading and UI broadcast filtering. |
| [scene-session.js](examples/scene-session.js) | Ordinary registered project; stores session facts and requests reload once. |
| [joint.js](examples/joint.js) | Owner of authored hinge/slider joint; bounded motor. |
| [animation.js](examples/animation.js) | Skinned asset with Wave/Stretch clips and Root/Elbow/Tip keys (change properties/content for your skeleton). |
| [ragdoll.js](examples/ragdoll.js) | Authored animation + mapping; physical entry, impulse, visual return. |
| [multi-target-ik.js](examples/multi-target-ik.js) | Original multipart `character_lab` rig; explicit shared four-contact mapping, last-solve residuals, atomic target replacement and clear. |
| [physical-animation.js](examples/physical-animation.js) | Same full rig with ordinary mapped bodies and CharacterMotor; partial impulse, explicit full handoff, passive release and collision-validated animation return. |
| [character.js](examples/character.js) | CharacterMotor, `move_x/move_y/jump` map; generic JS acceleration/launch. No camera ownership. |
| [stale-handle.js](examples/stale-handle.js) | Prefab ID; validates stale Entity and retained Character behaviour. UI/Joint lifetime differences are documented separately. |
| [surface.js](examples/surface.js) | Any entity; reflects actual module/class surface for drift verification, not game behaviour. |

Focused documentation/type check:
`node scripts/check_judasjs_api.mjs --typescript /path/to/typescript/lib/typescript.js`.
It checks the current declarations and all cookbook files without running the engine.
Add `--runtime /path/to/current/surface.json` when comparing a fresh actual-VM
enumeration. This verifies surface/types, not visual behaviour or every runtime branch.

**Historical M50 validation command:** `python3 scripts/m50_validation.py --typescript /path/to/typescript/lib/typescript.js`.
Supply `--node /path/to/node` if Node is not on PATH; choose a fresh `--output`
directory for a later run. An installed TypeScript package can replace the explicit path.
At M50 it built the focused test target against the existing Release engine,
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

- [Imported parts and extracted clip interval](examples/imported-model.js) (needs the local-only original character used by `import_lab`; fails on a clean checkout, see [model import](../MODEL_IMPORT.md#import-inspect-place))
  ([model workflow](../MODEL_IMPORT.md)): attach to the original import-lab model.

## Authoring-to-runtime seams (M67 candidate)

[authoring-runtime.js](examples/authoring-runtime.js) is an executed public-API
example for configured projection/behind points, layout snapshots/atomic mutation,
independent prefab initial properties/state/world velocity and stale handles. It
uses registered assets from [World Workshop](../../projects/world_workshop/README.md).

<a id="paired-input-and-batched-queries-m69-candidate"></a>

## Paired input and batched queries (accepted M69)

[paired-stick.js](examples/paired-stick.js) keeps render-frame reads separate from
authoritative fixed-step observation processing. Its `state` exposes raw and
circular values, net frame delta, twelve recent delivered samples, monotonic
cursor, reset/overflow counts and per-phase read counts. Author `flick_stick` as
shown in [input reference](input.md#circular-vector-binding). Hardware absence
means neutral readings; synthetic focused checks are not human controller testing.

[ray-fan.js](examples/ray-fan.js) builds a 10×10 fan from the owner's local
orientation, with ordinary `maximum`, `spread` and `height` script properties.
It records all 100 result positions, including null misses, in `state.results`.
Bind `compare_rays` to a convenient logical action (the small fixture uses C) to
perform 100 scalar calls on the unchanged world and record `state.matched`.
That comparison is optional game-side evidence, not the production batch path.
The example implements no trick recognizer or ledge-choice policy.

The final M69 gate executes these two cases only through
`judasjs_examples_tests <fresh-output> paired-stick` and
`judasjs_examples_tests <fresh-output> ray-fan`, using private copies of ordinary
project content. API/types/live enumeration uses the existing `surface` case and
`scripts/check_judasjs_api.mjs`. See [M69 evidence](../evidence/m69/REPORT.md) for
the actual commands/results and any untested hardware claims; do not substitute
the older M50 runner, which would build and execute unrelated examples again.

### Small operator fixture

`python3 scripts/create_m69_example.py` creates a private ordinary project at
`.cache/m69/example/m69_example.judasproj` by copying the current character arena.
It preserves source projects and refuses an existing output. Launch with
`build/judas .cache/m69/example/m69_example.judasproj` (or open that project in the
editor). The final M69 gate creates this copy; it is not a required runtime tool.

The normal `.judasui` HUD displays raw/circular/frame values, six ordered samples,
cursor/reset/overflow counts and all-hit/miss totals. Right-stick diagonals and
out-and-back movement exercise observations; C compares the 100-ray batch with
100 scalar rays in the same world. Escape pauses/resumes, R reloads, Q quits.
No device is substituted for human controller feel: an absent controller reads
neutral. The source [generator](../../scripts/create_m69_example.py) and its small
JS display subclasses use only ordinary authoring and public APIs.

## Kinematic bodies (M71)

[kinematic.js](examples/kinematic.js) attaches to an ordinary supported rigid
body. `start` chooses kinematic authority; `fixedUpdate` replaces a persistent
world COM/angular velocity command with stop and a bounded complete pose target,
then reads actual point velocity and checks invalid quaternion rejection.
The example state reports queued commands, last resolved pose/velocities and
completion. It supplies no route, character or camera policy.

The focused `judas_kinematic_lab_tests` target executes this file through normal
asset registration and RuntimeWorld/QuickJS callbacks. The ordinary
[kinematic lab](../../projects/kinematic_lab/) composes similar public APIs for
interactive content. See [command semantics and lifecycle](entities.md#kinematic-motion-m71).

## Owned signals (M73 candidate)

[Copyable broadcast/target/UI script](examples/signals.js) is executed by
`judas_signal_tests` through the ordinary VM. Its shared registration helper supports
start and modern restore without resetting counters; see [signals](signals.md) for
bounds, transient-save semantics and paused-lane delivery.
