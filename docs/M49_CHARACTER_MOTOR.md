# M49 — Scriptable character motor (candidate)

**Status through M69:** M49 is operator-accepted and checkpointed. Candidate/pending
statements and measurements below record the original milestone review state,
not a current outstanding acceptance gate. Later contracts take precedence;
start with [Architecture](ARCHITECTURE.md) and [JudasJS](JUDASJS.md).

Current public JavaScript signatures, examples and lifetime rules: [JudasJS reference](JUDASJS.md). This document retains milestone architecture and evidence context.

Judas owns collision-aware motion primitives. JavaScript owns character behaviour.

## Ownership and motion

`CharacterMotor` is an ordinary optional authored entity component, not a player,
rigid body, camera, input consumer or animation state machine. Its capsule queries
use the authoritative PhysicsWorld tree, collider geometry and resolved motion
segments. Each instance has independent state. A world with no motors does no
motor work. **Historical M49 scope:** character blocking/events were not implemented.
M65 adds queryable motor observation capsules and collision/sensor callbacks through
the normal physics event path, without making motors dynamic rigid bodies.
See [current motor contracts](judasjs/character.md); motor-to-motor blocking
remains a separate limitation.

`SweepCapsuleMotion` generalizes the proven historical player query to explicit
per-instance radius and cylinder half-height. The legacy query is its wrapper.
Both legacy locomotion and modern motors use `ResolveCharacterSlide`; the motor
also reuses `TryStepMove`/`TryStepDown`. Old scenes retain their compatibility
controller and its original dynamic velocity-seeding behaviour. Modern motors
use finite impulses instead. No contact solver/impact policy changed.

Order: script `fixedUpdate` supplies intent, ordinary rigid physics advances once,
then motors sweep against resolved rigid motion. Pose publication precedes pose
animation and physics-event dispatch. Rendering interpolates motor endpoints;
it never controls motion. The final skeletal pose, ragdoll and motor remain separate.

Capsule local +Y is its model axis. Up derives from the existing GravityField
sample; negligible gravity retains the supplied orientation. Frame reorientation
has a configurable rate. Departure intent is measured in the orientation the
script used before that reorientation. Support normals are actual geometry
normals, not gravity directions. Support acceptance uses a configurable angle.

Bounded signed-depth recovery precedes support/motion. Sweep-and-slide has four
iterations. Steps use the existing up/forward/down queries, with a conservative
lip probe up to 0.08 m for explicit capsules; its extra displacement is reported.
Support following remembers the recent support height only within step-height
reach; explicit outward motion clears it. These are practical finite-tolerance
approximations, not arbitrary-distance continuous collision guarantees.

## Velocity and support

Script velocity is world-space, including the last support velocity. The motor
adds sampled gravity × `gravityScale` and one-step extra acceleration. Consumers
may set `gravityScale=0` and supply their own response to the same gravity sample.
Extra acceleration clears after each step. The motor removes blocked inward
velocity while preserving tangential/outward components. It reports actual
position change separately from velocity, including recovery, steps and carry.

Supported motion uses the body's actual previous/current transform once and
sweeps carry against obstacles. Velocity carry is `v + omega × offset`, not
parenting. Support changes replace the previous carry contribution. Departure
retains world momentum. Teleport/ResetBody writes are not continuous platforms.
Dynamic constrained translating/rotating supports are demonstrated. The motor
is not itself an attached reference frame.

Dynamic pushing applies a normal linear impulse based on configured interaction
mass and the body's finite mass, bounded by `maxPushImpulse` per slide operation.
It does not overwrite body velocity, create infinite-force pushing or claim exact
character/body momentum conservation. The script-driven capsule has no solved
mass of its own. Persistent rigid contacts/friction remain the ordinary solver's.

## JavaScript

`entity.character` returns a safe entity-backed facade or null. Retained facades
fail safely after entity destruction. Properties/methods:

- `velocity` (read/write in fixed callbacks), `accelerate(vector)` (one-step sum).
- `state`: `velocity`, `actualDisplacement`, `supported`, `supportNormal`,
  `supportVelocity`, safe `supportEntity`, `gravity`, `up`, `collided`.
- Convenience reads: `supported`, `supportNormal`, `supportVelocity`,
  `actualDisplacement`, `gravity`, `up`.
- `enabled`, `configure(settings)`, `ignore([entities])`.
- Configuration includes geometry, step/probe/skin/slope, gravity multiplier,
  reorientation, interaction mass/impulse, offset, project collision layer name,
  collision-mask names and required/excluded tag names.

Physics masks remain bilateral. Tag/ignored-body requirements further restrict
queries; sensors never block motors. Ignored handles retain slot generation.

`world.fluidSample(point, up, halfHeight, radius, tangent)` exposes the existing
approximate liquid field read-only; a dry world returns a dry sample. No fluid
solver, pressure exchange, cadence or accepted fluid content changed.

`world.setView(pose, fov)`/`clearView()` let scripts supply an independent runtime
camera. The motor owns no camera. Main rendering, audio and view-ray consumers
use that view. Legacy interaction/diagnostic anchoring becomes an observer;
its second locomotion update is suppressed. Secondary cameras remain independent.

## Authoring, identity and lifecycle

Inspector → Add Character motor. Settings are metres/physical tolerances, not
walk/jump/swim speeds. Add an ordinary script for intent. Scene serialization and
prefab field overrides use the existing component path; runtime SpawnPrefab
creates normal motor entities. Root rigid bodies/ragdolls cannot simultaneously
own the same entity transform. Visual animation is compatible. Entity scale must
be one; author capsule dimensions explicitly. Children remain ordinary hierarchy.

A conditional `Judas.CharacterMotor.1` fingerprint extension includes every
motor-authored setting; canonical schema remains 5 and motor-free hashes are
unchanged. Existing entity-state persistence carries motor position/velocity.
Under the legacy persistence contract, support/camera/one-step acceleration were
transient. Modern M61 slots preserve motor motion/support identity; presentation
is reinitialized and camera/input remain project policy. See [save contracts](M61_SAVES.md).
Disable clears support; destruction, reset, Stop and transitions invalidate state.
Script removal leaves an enabled component's last resolved velocity/normal gravity
active; games may explicitly disable it. No script owns the component's lifetime.

## Current project and workflow

Open `projects/character_demo/character_demo.judasproj` or run it standalone.

- F1: flat walls/corner, 25°/65° slopes, stairs, finite-mass pushable and actual
  M45 constrained translating/rotating platforms.
- F2: real radial spherical world, same generic systems.
- F3: accepted pool geometry/content copied into this new project. JS drives
  buoyancy, drag and bounded propulsion via the existing field query. The approved
  `projects/fluid_demo` and its evidence are untouched.
- WASD/controller + mouse/stick: project JS movement/look.
- Space: project JS launch; in liquid, project JS propulsion.
- Z: independent prefab motor with a separate scripted motion pattern.
- R: scene reconstruction. Escape: authored pause menu.

`Assets/scripts/controller.js` contains all movement rates, input mapping,
acceleration/deceleration, launch and swimming decisions. The motor's C++ API has
none of those gameplay states. Animation/ragdoll integration is deliberately
limited to the independent architecture; no automatic animation, root motion,
IK, ragdoll activation or active physical animation is added.

The pool retains the documented coarse production-fluid limitations and 20 Hz
liquid setting. This milestone does not improve its numerical/visual quality.

## Human checklist

1. Move, look and launch using keyboard/controller.
2. Push into walls/corners; climb/descend the stairs.
3. Compare the gentle and steep slopes.
4. Ride translating/rotating platforms; leave/launch and inspect carry.
5. Push the finite-mass box.
6. Switch to radial gravity and move around the actual sphere.
7. Spawn another motor and confirm independence.
8. Switch to the pool, enter/swim/exit; Space supplies script propulsion.
9. Reload and Play/Stop; inspect that no stale motor state remains.
10. Repeat in the moved exported standalone package.

Human interactive/visual acceptance is pending. Automated state checks are not
human acceptance. Results and fingerprints: `docs/evidence/m49/`.

## Candidate validation

Clean Release build: zero warnings. All 92 production suites and 246 real async
integration checks passed. M49: 32 motor + 39 application checks passed. Editor
Play/Stop restored identical authored state. Standalone, scene transitions/reload
and moved-package export smoke passed. Source fingerprints match the tested run.

Final simple-floor batch cost (including ordinary physics): 1 / 10 / 100 motors
= 0.030989 / 0.317179 / 3.195049 ms per step over 120 samples. Human interactive
validation remains pending; headless runs cannot confirm mouse capture or feel.
