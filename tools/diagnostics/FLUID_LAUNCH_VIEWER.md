# Fluid launch viewer

Diagnostic-only native driver for the original Terrain planet and the flat
comparison. It links the existing Experimental engine without modifying physics,
configuration, precision, ordering or release locks. The driver has debug symbols;
this does not turn the linked Release engine libraries into a Debug build.

## Build and launch

From the repository root, using a matching native CMake build:

```sh
python3 tools/diagnostics/build_fluid_launch_viewer.py \
  --build-dir .cache/2d-stage1-build --output-dir .cache/fluid-launch-debug
JUDAS_ENGINE_ROOT="$PWD" .cache/fluid-launch-debug/fluid_launch_viewer \
  assets/scenes/terrain_flat_comparison.judas .cache/fluid-launch-debug/desktop.jsonl
```

Use `judas_tech_demo.judasproj` instead for the original radial scene.
No saved-world overlay is loaded. The original authored scene is not changed.

## Behaviour

- Advance real `StepPlayedWorld` fixed steps at the existing 1/60 s timestep.
- Stop immediately after the first fixed step applying any nonzero production
  buoyancy/drag load. This is a deliberately early diagnostic trigger, not a
  general detector of unphysical behaviour or a force cutoff. It may also stop
  legitimate buoyancy in another scene. No catch-up steps follow the trap.
- Q key-down advances exactly one complete fixed step and remains paused.
  OS key-repeat is ignored. Gameplay keyboard/mouse input is claimed, including
  the old Q move-down binding. The diagnostic observer camera stays fixed.
- Between requests, no gameplay, UI-script, presentation-script or simulation
  updates execute. Only host resource pumping and rendering continue.
- Render the completed endpoint at alpha=1, not an interpolated earlier state.
- Close the window to exit. There is intentionally no menu/resume path here.

"Exact frame" means the first completed *physics step* with fluid loading, not
an arbitrary display refresh or a breakpoint inside a partially executed solver.
Both sides of that step are logged, so its prior state remains inspectable.

## Recorded evidence

JSONL is flushed after every step. Metadata contains UTC, engine source revision,
project, scene, timestep, particle count and exact trigger semantics. Each step has:

- Before/after body position, orientation, linear/angular velocity and mass.
- Player position, velocity and orientation.
- Every particle's current/previous position, velocity, acceleration and mass.
- Before-step body-volume quadrature: sample position, gravity/up, footprint,
  density, wet fraction, reconstructed fluid motion and double force contribution.
- Recomputed force/torque and nearest particle-to-oriented-box clearance including
  the configured particle radius. The shadow field follows the fixture's initial
  empty-field and subsequent particle-endpoint publication ordering.
- Actual production buoyancy, drag, wet volume, immersion and contact statistics.
  Recomputed buoyancy must match the production float force exactly, or the viewer
  fails instead of presenting the reconstruction as authoritative evidence.
- A screenshot of the trap endpoint at `<log path>.trap.png`.

This bounded quadrature trace supports the current box-body fixtures without
fluid cavities. It explicitly rejects unsupported dynamic shapes/cavities.
It is not a general-purpose replacement application or a fluid repair.

## Verification

Append `--self-test` to run the short hidden-window checks. They assert the
pause leaves logged body/particle/player state and simulation time unchanged,
Q advances exactly once and remains paused, OS repeat does not advance, and the
final logged physical endpoint exactly matches a fresh uninterrupted control.
The control runs the same original engine path without shadow field queries.

Native Linux NVIDIA GTX 970 checks pass for both fixtures. Each traps at step 2,
about 0.033333335 s; test Q advances to step 3. Final raw tests are under
`.cache/fluid-launch-debug/*-final-self-test.*`. These are automated diagnostic
checks, not human acceptance or general fluid qualification.

The default runtime, editor, Alpha lock and independent research are untouched.

## Delayed-coupling diagnostic (not a repair)

`--suppress-water-before-60` suppresses water-to-rigid-body force and torque,
two-way particle contact reaction, and player fluid samples during steps 1–59.
Water advances at its original cadence; gravity, ordinary body contacts and
other game logic continue. Step 60 restores ordinary coupling with no backlog,
then pauses even if no nonzero water load exists. Q steps normally afterward.
This option and `--water-only-start` are mutually exclusive.

The Linux diagnostic executable uses GNU linker wrappers, not modified engine
sources. It scopes force interception to ProductionFluidCoupling preparation;
ordinary gravity and other force sources pass through. Logs distinguish computed
loads from their suppression flag and count intercepted calls. Self-tests assert
zero interceptions from step 60 onward and exact matching endpoints against an
uninterrupted run with the same deliberate diagnostic suppression policy.
The flag-off original path is independently retained as a negative control.

Raw first test: flat-suppress60.jsonl / radial-suppress60.jsonl in the local
`.cache/fluid-launch-debug/` directory. Both freeze at step 60. Flat ship speed
rises from 0.00199 m/s at step 59 to 12.8564 m/s at 60, with computed buoyancy
78,850.74 N; radial ship buoyancy is zero at 60 and speed remains ~0.0452 m/s.
These bounded observations do not certify later stability or justify a delay fix.
