# M51 — engine primitives, project behaviour

**Status through M69:** M51 is operator-accepted and checkpointed. Candidate/pending
statements and measurements below record the original milestone review state,
not a current outstanding acceptance gate. Later contracts take precedence;
start with [Architecture](ARCHITECTURE.md) and [JudasJS](JUDASJS.md).

Candidate based on M50 checkpoint `74da7b534831d7cb7043ed1c02970283f7078daf`.
No milestone beyond M51 is implemented here. Human review is pending.

Judas supplies physical, presentation, resource and lifecycle machinery.
Projects compose it through the public `judas` module. A demo's meaning is not
an engine law.

## Execution boundary

Project format 1 gains an optional `legacy-gameplay "true|false"` setting.
Missing means **true**, preserving existing projects, historical harnesses and
saves. **Create New Project sets false**. The editor exposes and labels this
compatibility setting; it is copied into each new world before Build, including
editor Play, standalone startup and queued scene replacement.

With false, the GameSession player is only a diagnostic observer. It has no
legacy collision shape, locomotion, interaction targets, vehicle control,
spawn/delete/reset shortcuts, torch, heater or automatic gameplay menu. No
script view is needed to keep these adapters inactive. The renderer's no-view
fallback is an identity view, rather than a first/third-person game camera.
CharacterMotor, gravity, contacts, joints, fluids, animation, audio, UI,
resources and rendering continue on their normal authoritative paths.

Scripts request pointer capture through `input.pointerCapture`; modal UI can
release actual capture temporarily. New projects do not implicitly capture the
mouse because a window exists. Editor viewport navigation remains editor-owned.

The project's execution setting changes the effective runtime authored baseline
in scripted mode using a domain-separated hash. Legacy baselines remain byte
compatible; canonical scene fingerprint **schema 5 is unchanged**. Export uses
normal project serialization, so execution mode and input map travel together.

## Actual migrations and compositions

Current recommended project: [boundary_demo](../projects/boundary_demo/boundary_demo.judasproj).

| Behaviour | Before | Current project implementation |
|---|---|---|
| Locomotion, launch, swimming | PlayerController policies; M49 already had a JS replacement | copied M49 `controller.js` + CharacterMotor + logical input + existing fluidSample |
| Interaction decision | GameSession / SelectInteractable / C++ Interactable meanings | ordinary raycast, tags and safe Entity handles in `rules.js` |
| Physical door | Door's kinematic open/close panel | ordinary dynamic panel + M45 hinge, JS chooses spring target |
| Acquire/drop/throw | ObjectManipulation, pickable whitelist, G/H decisions | JS tag/query acquisition, finite mass-scaled PD holding force, tensor torque, mass-scaled impulse |
| Vehicle control transfer | PilotControl / secured pilot policy | JS selects tagged ordinary body, disables its motor while seated, resumes motor on exit |
| Spacecraft thrust/rotation/SAS | FlyingPrimitiveControl constants and state | JS force, torque and inertia-based attitude control on an ordinary rigid body |
| Camera modes | PlayerView/GameSession first/third-person toggle | JS V choice + world.setView; craft-local camera is project policy |
| Pause/resume/quit | fallback PauseMenu C++ | authored M41 UI + JS events/modal state |
| Reset/transition/spawn/delete | GameSession shortcuts | scenes.reload/load, spawnPrefab and Entity.destroy in project JS |
| Cursor ownership choice | Window initialization / unconditional Play capture | project request + generic UI/device reconciliation |

`rules.js` has no internal dispatcher calls and no engine-name conditions.
Project entity IDs, tag names, forces, speeds and menu meanings are ordinary
content. Holding never repeatedly writes a rigid body's transform. The seat
policy deliberately positions a **disabled non-rigid motor entity** relative to
the craft; the craft itself moves only through real physics.

## Small missing primitives

The M50 API already covered queries, tags, forces, joints, camera poses, UI,
scene/session operations, spawning and motor intent. Only these additions were
needed:

- `Entity.mass`: current dynamic-body mass in kg.
- `Entity.inertiaWorld`: current world inertia tensor in kg m²; Vec3 columns
  `x`, `y`, `z`. It includes normal body mass/inertia mechanisms.
- `input.pointerCapture`: boolean runtime-world capture intent, distinct from
  actual device state and UI ownership.

These are reusable for unrelated physical controllers and pointer-driven games.
They do not encode pickup, doors, vehicles or first-person gameplay. Their safe
handle/error rules are in the current [JudasJS reference](JUDASJS.md).
Types, AST inventory, native coverage, live VM enumeration and the executed
physical-control cookbook example cover the additions.

## Compatibility, not a second recommended game framework

Historical PlayerController, PilotControl/PilotAttachment, Interactable,
ObjectManipulation, Door, LightSwitch, FlyingPrimitiveControl and fallback
PauseMenu remain for existing scene formats and protected tests. Their input and
behaviour execution is gated by the project setting. Door/vehicle/player-start
editor controls are labelled as historical adapters. New projects/demos should
use CharacterMotor, bodies/joints, authored UI and JS instead.

Historical light-switch lamp presets, torch/ship lighting presets, held radiant
heater, B water emission and M20 operator thrusters are **retired compatibility
policies**, not newly exposed gameplay APIs or promises that every old showcase
has been ported. Normal authored lights, combustion, fluid emission internals and
physical force application remain engine services. See the [audit](evidence/m51/AUDIT.md).

The accepted production fluid solver and `projects/fluid_demo` remain unchanged.
This project's optional pool copies existing ordinary content solely to show JS
swimming and force-driven container manipulation; it is not a new fluid quality
or performance signoff.

## Scenes and operator controls

- `Scenes/flat.judas`: steps, slopes, physical supports, prop and hinged panel.
- `Scenes/flight.judas`: same motor/UI machinery plus an ordinary zero-gravity
  craft; no SceneVehicle component or privileged pilot entity.
- `Scenes/planet.judas`: existing radial environment, same controller script.
- `Scenes/pool.judas`: existing production pool/container geometry and JS swimming.

WASD/controller and mouse: move/look. Space: JS launch/swim propulsion.
G: targeted door or pickup; G while holding: drop. H: throw. Y: remove a tagged
pickup. V: camera choice. F: select/leave targeted craft. X: toggle SAS while
controlling it. Craft Q/E translate vertically; I/K, J/L, U/O rotate.
Z: spawn an independent motor prefab. R: reload. Escape: authored pause menu;
its scene button cycles workshop → flight → planet → pool. F1/F2/F3 also select
flat/planet/pool. Pause and scene switching remain JS content.

## Behavioural differences and limits

- Door response is now a finite physical hinge spring, not an infinitely strong
  prescribed panel angle; contacts can resist it.
- Pickup eligibility is a project tag; normal ray geometry blocks selection.
  Holding applies attitude control to any tagged held body, not only compounds.
- Craft selection is a query policy rather than the historical supported-only F
  boarding rule. Its deliberate seat placement is not a generic attachment API.
- This simple craft demonstration does not implement obstacle-aware dismount,
  atmospheric piloting, gameplay flight modes or active physical animation.
- The old C++ compatibility APIs still exist. Choosing compatibility true is an
  explicit request for that historical behaviour, not the modern default.
- Older executables reject the new project key; reading old projects remains
  compatible, but this does not promise forward format compatibility.
- Existing CharacterMotor, shape-cast, UI, fluid and animation limitations remain;
  M51 does not revise their physics or fidelity guarantees.

## Human review

1. Walk/launch around the workshop, steps and physical supports.
2. Aim at the panel, G open/close; confirm contact/joint response.
3. Pick up, carry, drop and throw a prop using G/H; confirm physical motion.
4. V changes the camera; R reloads; Escape opens the authored menu.
5. Choose flight, aim at the craft and F; test forces, rotation, X SAS and F exit.
6. Visit radial/pool scenes; check local-gravity controls and existing JS swimming.
7. Inspect `controller.js` / `rules.js`; change a force or speed and reload without
   recompiling the engine.
8. Spawn another motor, reload/transition/Stop, and check independence/cleanup.
9. Test the exported package and responsiveness. Human acceptance is authoritative.

Evidence/results/fingerprints: [M51 evidence](evidence/m51/).
Proposed commit after approval: `Move game behaviour from engine C++ to JudasJS`.

## Human gravity-review correction

The radial demonstration's local flat-platform area now authors a bounded
uniform gravity zone before its broad radial region. The original scene omitted
this zone; the motor, JS and resolver required no change. Outside the zone,
normal planet gravity remains radial. See [focused reproduction and results](evidence/m51/gravity-followup/README.md).
