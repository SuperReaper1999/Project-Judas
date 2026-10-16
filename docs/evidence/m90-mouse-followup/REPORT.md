# M90 mouse-look input-map correction

Base: `1f37b6b1b91c86d17646a4f0e8165f8112af9e08`. The operator reported that
mouse look did not work in the current desktop tour. This is a game/content fix
on Experimental; main promotion is not inferred from automated checks.

## Demonstrated cause

The project and `tools/CreateTravelLab.cpp` bound look to `mouse:DeltaX` and
`mouse:DeltaY`. Those arbitrary strings pass the generic map grammar but are not
controls emitted by Window. Its actual SDL motion path publishes `mouse:dx` and
`mouse:dy` (`Window.cpp`), and InputSystem treats those canonical names as
unclamped relative deltas, cleared each frame (`InputSystem.cpp`). The JS correctly
reads logical `look_x` / `look_y`; no engine API was missing.

Both authored bindings and their generation recipe now use the canonical names.
No engine implementation, camera mechanics, physics, input policy, public binding,
save schema, cadence or origin handling changed. The earlier third-person fix
remains intact. No unrelated game, protected evidence or fluid work changed.

## Actual before/after proof

An owned copy of the exact old project is exercised by the updated ordinary
application check. It injects **physical canonical mouse controls**, not logical
look axes or private script state. The original has **six failures out of 23**:
neither axis reaches JS, delta magnitude/one-shot behaviour cannot be observed,
actual orbit heading remains effectively zero, driving look fails and look after
pause fails. The correct pause suppression and existing physical/save checks
still pass. Original authored bytes are preserved alongside the failure log.

The corrected project passes **23/23**, including:

- Positive X / negative Y gives yaw +0.24 rad and pitch -0.12 rad for 120/-60 units.
- Opposite directions reverse intent, and deltas do not persist to the next frame.
- Actual third-person camera heading changes by about 0.0913 rad in the fixture;
  original heading is approximately 1.6e-8 rad. Settling alone cannot satisfy it.
- Both axes also work while driving the same rover.
- Pause blocks look changes; closing pause restores normal mouse look.
- Initial and dismounted third-person views, physical dismount, save/load and
  reload remain passing. No full production suite or long-drive performance
  benchmark was repeated for a binding-only correction.

The actual exported runtime also passes raw `CONTROL mouse:dx/dy` and
`EXPECT_AXIS` checks from unrelated `/tmp`, including full 120/-60 values and
zero on the next frame. Export contains 8 assets / 2 scenes. The desktop watch
tour is reopened from a private launch preset, leaving committed defaults intact.

```sh
cmake --build build --target judas_origin_travel_application_tests -j4
build/judas_origin_travel_application_tests projects/travelling_lab/travelling_lab.judasproj .cache/m90-mouse/repro --camera-only
build/judas_export projects/travelling_lab/travelling_lab.judasproj .cache/m90-mouse/package-repro build/judas
# Run that package's own executable with standalone.input as JUDAS_TEST_SCRIPT.
```

Human check: move the mouse on foot and while driving, verify both directions,
then pause/resume and repeat. Human retest of this correction is pending.
Windows/Android are untested for this content follow-up. Existing M90 restrictions
and baseline-stale save-bound assertions remain documented and unchanged.
