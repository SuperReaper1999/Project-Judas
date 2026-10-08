# M65 — Developer integration

**Status through M69:** M65 is operator-accepted and checkpointed. Candidate/pending
statements and measurements below record the original milestone review state,
not a current outstanding acceptance gate. Later contracts take precedence;
start with [Architecture](ARCHITECTURE.md) and [JudasJS](JUDASJS.md).

Current candidate from `19a53a42a9b363818e67c07d8b63e9fddcea2845`. Human review pending.

Judas provides reusable geometry, pose and ownership mechanisms. Parkour, skating,
checkpoint progression, board motion and attachment policy remain project JavaScript.

## Open and use

Open `projects/m65_integration/m65_integration.judasproj`. Its integration scene is
original CC0 geometry/rig content. F2 opens the permitted local Rooftop Run copy.
The original consumer projects are untouched. This is **not** validation of the
third-party skate character; the simple rider reproduces its reported integration problem.

- WASD, mouse, Space: move/look/scripted departure.
- F1/F2: integration lab / Rooftop Run.
- G: impulse the physical articulations; P: spawn another shared-asset articulation.
- B: rider enters/leaves ragdoll. J/K: create/release/re-anchor a normal connection.
- M: select physical material and apply a small impulse.
- T: request/release streamed annex. Y: adopt its socket assembly into root ownership.
- F6/F7: normal game save/load. Backspace: reload. Escape: authored modal pause.

## Changes

### Movement and events

Departure uses relative motion against the **current support separation normal**;
positive motion against gravity alone does not release inclined support. A small
tangential-travel/skin-scaled separation tolerance permits support-follow curvature
and step-down; normal-only gentle acceleration is not swallowed by a fixed deadband.
Departure includes this step's effective gravity/external acceleration, integrated once.
Real outward motion still departs. Gravity, support and attachment remain distinct.

Motors register massless query capsules before fixed physics. Existing narrowphase
geometry and M39 pair filtering supply sensor endpoint overlaps; observed motor
sweep/support contacts join M42's coalesced pair lifecycle after all motors move.
No motor rigid mass or motor-to-motor blocking was added. Endpoint sensors do not
promise continuous detection when a capsule crosses an entire thin volume in one step.

The copied Rooftop Run's environment probes exclude their own motor using the
normal `QueryFilter.ignored` entity list. Its old unfiltered rays/capsule casts hit
the new query capsule at distance zero, producing false walls and obstructed
headroom. Wall-run, wall-jump, climb, vault and slide-to-stand follow-up checks now
exercise normal logical input; original consumer content remains unchanged.

`supportNormal` is the capsule's contact/separation normal. At a box edge it need
not equal a face normal. A separately fabricated/smoothed “surface normal” is not
provided. Avoid projecting intended velocity onto every changing corner normal.
Let the motor resolve movement; use support state for game decisions.

### Skeleton consumers

`animation.jointTransform(key, space, presented)` reads the final resolved pose.
`local` is parent-relative, `model` is skeleton space, `world` composes entity pose.
Presented world roots use render interpolation in presentationUpdate. Bone poses
use the same evaluated pose skinning uses; there is no invented bone interpolation.

Two-bone contributors run through M47, in `(order,id)` order (1–999), after clips
and before the M48 physical contributor at 1000. Positive uniform chain scale and
direct root→middle→end hierarchy are required. Targets/poles are world positions.
Unreachable endpoints stay at reach; segment lengths do not stretch. No foot
orientation, whole-body balance or dynamic-bone pose motors are implied.

Visual sockets compose final joint pose and authored offset, target-first, and share
presented root timing. They cannot own rigid bodies, motors, ragdolls or deformables.
Missing/unpublished joints return unavailable rather than bind-pose substitutes.
Cycles are rejected. Physical attachments use M45 joints.

### Sleeping

Connected dynamic contacts/joints form islands. Quiet thresholds: linear/angular
speed <0.03, per-step position change <0.002 m, orientation dot >0.999999,
anchor/contact error <0.02 m, locked angular/limit error <0.01,
per-step spring-coordinate change <0.002 (allows gravity-balanced equilibrium); quiet duration 0.75 s.
Only supported/anchored islands or exactly still unforced zero-gravity islands sleep.
Nonzero drives prevent settling. Sleeping skips active rows/integration, retains
broadphase/query/sensor registration, and exposes M56 counters and Entity.sleeping.

Impacts, force/torque/impulse, changed gravity/geometry/support/filter/material,
constraint changes and two-way fluid/deformable loading wake connected bodies.
Ordinary shared static floors do not connect independent dynamic islands. Persistent
sleep state is an optional version-1 M61 participant; old saves reconstruct awake.
Stream revisit reconstructs awake and settles normally. No camera-distance policy.

### General access and product details

- `physics.gravity(position)`: the same position-based Judas resolver as physics.
- `.judasphysmat` resources share body friction/restitution identity. Optional
  instance overrides persist independently. No per-triangle material fiction.
- `physics.createJoint(owner, config)` / `joint.configure` / `joint.destroy` use
  normal authored references and lifetime. Configure retains participants; replace
  participants by destroy/create. Validate before mutation; no new solver.
- Scene `background-color` / `world.setAppearance({backgroundColor})`; environment
  background wins when present/enabled. Linear-render colour passes through exposure
  and tone mapping, so it is not an exact display-RGB promise.
- Instance `material.set({uvScale,uvOffset})` reuses normal material overrides.
- Project export include/exclude lists select registered scenes explicitly; startup
  and world-manifest scenes must remain. Registered dynamic assets remain packaged.
- Runtime window uses project title; inapplicable universal save hints are omitted.
- Audio startup diagnostics distinguish invalid clip/settings, group and capacity.

## Authoring

Ctrl-click hierarchy selection, search, batch delta transforms, common field editing
with explicit mixed state, group/reparent (keep world), component copy/paste and subtree
duplicate all use scene snapshot undo. Skeleton pickers use stable keys; selected
skeletons show rest-pose local axes. M64 collider cooking/preview remains available.

Script properties support `{type:'entity', default:null}`. Authored value is null or
`{"entity":"stable-decimal-id"}`; the inspector picks entities. It resolves to a
normal safe Entity wrapper, remaps through prefabs/regions/duplication and saves,
and does not reinterpret ordinary numeric/string properties as references.

[Named code-first authoring](CONTENT_AUTHORING.md), [harness](TEST_HARNESS.md),
[JudasJS reference](JUDASJS.md), [findings/proof](evidence/m65/REPORT.md).

## Limitations and review

No full trigger CCD, motor blocking, FBX/multi-mesh import redesign, humanoid fitter,
retargeting, physical IK or streaming scheduler rewrite. Existing animation/rigid
scale/shape limits remain. Socket state is visual; actual physical ownership stays
with physics. Performance evidence describes this machine/workload, not free crowds.

Human checklist:

1. Slope/jump/edge/platform movement and Rooftop checkpoints behave correctly.
2. Look and pause work; harness captures follow camera and paused modal content.
3. Feet track the tilting board; hand prop tracks layers, crossfades and ragdoll.
4. Articulations/debris settle, then respond again to G/impacts.
5. J/K/M produce real attachment/material behaviour.
6. Multi-edit/undo, copy/paste, skeleton and typed entity pickers are useful.
7. Save/quit/load and streamed revisit/adoption retain appropriate state.
8. Moved exported standalone behaves equivalently.
