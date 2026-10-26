# Developer Workflows — four editable lessons

Copyright (c) 2026 Conner / SuperReaper1999. Original example code/content uses the
repository MIT terms. DejaVu Sans is redistributed with its font licence.

Open **developer_workflows.judasproj** in Judas Editor and Play. It works in the
source checkout and as a copied SDK example, without another game project's files.
The example is deliberately small, with visible primitive collision/visual geometry.

- WASD / mouse / Space: walk, look, jump. V changes camera distance.
- E pushes the orange crate. Walk near the blue vehicle and E boards it; WASD
  drives/steers. Space brakes; E exits below 2 m/s.
- R requests/releases the blue crate's `yard` region. The text reports native state,
  visual resources and current script registration independently.
- Escape pauses/releases the mouse. Region requests and save UI are deliberately
  available while paused. Workers can prepare, but installation waits for resume;
  callbacks on already-published entities can still initialize in UI/presentation.
- F6 saves the lesson. Quit the whole application, reopen and F9 to cold-load.
  Observe counters, actual bodies and vehicle state, then continue interacting.

Open **View → Diagnostics and extensions → Runtime Inspector**. Pick a root entity,
then the streamed crate. Compare owners, references, retention reasons, physical
publication and script phases. Save diagnostics use isolated EditorSlots during
Play; your exported standalone uses Slots. The tool does not mutate the world.

Read only one script at a time:

1. `Assets/scripts/crate.js`: small persistent state, real impulse, start/restore/destroy.
2. `registry.js`: transient per-VM registrations, deliberately not serialized.
3. `vehicle.js`: native wheel/powertrain input; a body-free visual inherits interpolation.
4. `player.js`: CharacterMotor, a script-owned presentation camera, capability-aware
   interaction, region requests and ordinary save/load requests.

Try disabling the crate's script in the authored yard scene: native collision can
exist without its registered gameplay action. Restore the script afterward. Do not
"fix" readiness by adding a delay or assuming a valid entity implies a ready script.

The manifest and yard scene are independently editable. Root and yard have distinct
ownership even when close together. A root crate does not unload just because it
moves farther away. Existing region safeguards still decide whether unloading is safe.

Scope: flat/uniform gravity, basic third-person offsets without boom collision,
assisted boarding with a simple clear-space exit, one four-wheel vehicle, no audio
or game campaign. This teaches ownership/timing; it is not a reusable game framework
or a claim that every gravity/vehicle/streaming combination is universally supported.
See the guide two directories above for deeper examples and compatibility recovery.
