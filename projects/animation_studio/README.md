# Motion Studio

An ordinary editable Judas project demonstrating the M95 animation workflow.
Original CC0 multipart character source art and performances are included. The
character is intentionally simple enough to inspect in a DCC or the timeline.

Open `animation_studio.judasproj` in `judas_editor`, or run it with `judas`.

## Controls

| Control | Action |
|---|---|
| WASD | Move through extracted Walk motion and CharacterMotor collision |
| Left Shift | Run with a different authored stride/clip |
| Space | Script-driven jump with an airborne animation |
| E | Animated strike; its contact marker applies a real hit-point impulse |
| Q | Animated interaction; marker changes the beacon material |
| T | Toggle the watch tour (start from the initial scene for its target alignment) |
| F1 / F2 | Load flat / rotated-gravity studio |
| F6 / F7 | Save / restore ordinary game state |
| R | Reload the scene |
| Escape | Pause / resume |

The amber crate is the physical strike target. Approach it and face it before
pressing E. Repeated strikes move it farther away; this is physics, not a reset
animation. R restores the demonstration. The tour walks toward the crate,
strikes it, interacts, returns and jumps; it is ordinary project JS input policy.
The third-person camera is independent and follows the presentation pose.

## Timeline

**View → Animation timeline → performer.judasimport → Load / reimport selected
recipe**. Choose Walk, scrub its pose, select/add/edit a marker, Save the event
track, then reimport. The source model and recipes live in `Sources/` and
`Imports/`; runtime uses `Assets/models/performer.judasmodel`.

`Assets/scripts/lab.js` owns clip selection, movement intent, jumping, marker
policy, hit queries/impulses, UI and camera behaviour. `math.js` is small project
quaternion/vector math. The engine owns the actual animation sampling, collision,
physical target and resource lifecycle. No animation state machine lives in C++.

See [workflow, API and limitations](../../docs/M95_ANIMATION_WORKFLOW.md),
[animation reference](../../docs/judasjs/animation-ragdolls.md) and the
[copyable motor example](../../docs/judasjs/examples/animation-motion.js).

## Human checklist

1. Walk/run; observe different strides and blended transitions.
2. Walk into the wall, corner and steps; movement resolves against geometry.
3. Jump and land; animation never teleports the character or resets momentum.
4. Approach the amber crate and press E; it moves at contact time, once per action.
5. Press Q; the beacon changes when the interaction marker crosses.
6. Open the timeline, preview/scrub and edit a marker; preview emits no actions.
7. Save/reimport and test the changed timing in Play.
8. Pause/resume, save/load and reload; no duplicated action or stale handle.
9. F2: repeat movement/action under rotated gravity; F1 returns to the flat lab.
10. Run a relocated standalone and compare behaviour.

Morph targets and bone-key editing are not implemented by M95. Camera/controller
polish and commercial-quality locomotion art are outside this compact example.
Windows/Android M95 acceptance is outstanding. Human acceptance remains pending.
