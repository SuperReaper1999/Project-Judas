# Current application harness

This is the bounded `JUDAS_TEST_SCRIPT` application driver implemented in
[`src/TestHarness.cpp`](../src/TestHarness.cpp), originating in M65 and retaining
legacy compatibility commands. It is not the entire regression suite. Use the
[current validation manifest](../tools/validation/suites.json) and
[Pre-Alpha gates](PREALPHA.md#focused-validation-from-a-clean-checkout) for the
authoritative selected regression configuration.

`JUDAS_TEST_SCRIPT=SCRIPT build/judas PROJECT.judasproj`.
Outputs/captures should use fresh caller-owned paths. Errors exit nonzero.

```text
FRAMES 120
ENTITY 10
FRAME_SECONDS 0.033333333
AXIS move_y 1 5 25
LOOK 80 0 28
SCREENSHOT 30 /tmp/my-new-run/view.png
ACTION pause 1 45 46
EXPECT_PAUSED 1 47
SCREENSHOT 50 /tmp/my-new-run/menu.png
ACTION pause 1 65 66
EXPECT_PAUSED 0 67
```

Actions/axes inject the **first actual binding's physical control**, including scale
and deadzone conversion. InputSystem produces normal edges/fixed-step consumption;
no script-private intent mutation. `CONTROL token value begin end` selects an explicit
physical binding; `LOOK dx dy frame` supplies relative mouse motion, `POINTER x y frame`
supplies both the legacy UI pixel position and the normalized gameplay pointer
position, retaining its current held state. `EXPECT_AXIS name value frame`, `EXPECT_HELD name 0|1 frame`,
`EXPECT_PAUSED 0|1 frame` examine post-frame state (consumed UI actions may read zero).

### Gameplay pointer and touch injection

`POINTER_DOWN x y frame` and `POINTER_UP x y frame` move the pointer in window
pixels and press/release its primary button. The public `input.pointer` snapshot
and all logical actions bound to `mouse:Left` observe the same injected state.
`CONTROL mouse:Left ...` and mouse-bound `ACTION` commands also synchronize the
primary held state. A down/up pair on the same frame retains both logical edges;
its final pointer snapshot correctly has `down: false`. UI ownership can consume
the logical bindings without inventing a held gameplay pointer.

`TOUCH_DOWN id x y pressure frame`, `TOUCH_MOVE id x y pressure frame` and
`TOUCH_UP id x y frame` drive independent gameplay contacts. Positions/pressure
are normalized [0,1]; the caller's nonnegative ID identifies one synthetic native
finger, while public contact IDs remain engine-generated. Move/up require an
active matching contact; repeated down and malformed values fail. Touch commands
do not synthesize another mouse: use pointer commands when testing UI mouse input.
This is the main-input harness, not a per-player hardware assignment driver.

`EXPECT_POINTER x y down frame` checks normalized gameplay coordinates/held state;
`EXPECT_TOUCHES count frame` checks active contact count after the frame.

```text
FRAMES 8
POINTER_DOWN 80 60 0
POINTER 160 120 1
POINTER_UP 160 120 2
TOUCH_DOWN 7 .2 .3 .8 3
TOUCH_MOVE 7 .4 .5 .6 4
TOUCH_DOWN 8 .7 .5 1 4
EXPECT_TOUCHES 2 4
TOUCH_UP 7 .4 .5 5
TOUCH_UP 8 .7 .5 6
EXPECT_TOUCHES 0 6
```

Pointer/touch snapshots expose current state, not an ordered event history.
A complete touch lifetime between observations is therefore absent from the
snapshot. Keep a contact across an observed frame when exercising snapshot-based
gesture code. For short mouse taps, bind a logical mouse action and use its
`pressed`/`released` edges alongside the current position; polling only `down` can
miss a complete press/release between fixed updates.

`FRAMES` and legacy `STEPS` count application frames. Frame seconds defaults to 1/60;
0–0.25 supports paused UI clocks / several fixed steps. `REALTIME n` runs **n
application frames**, using wall time for each frame and the normal render/swap
path; n is not seconds. Simulation never receives profiler measurements.
`LOG_EVERY n` (0 disables), `ENTITY stable-id` select position/velocity/support telemetry.
CSV preserves legacy step/time/pose/up/velocity/gravity/control and objN body columns,
adding frame/fixed-step-count/paused. The first 19 fields are stable; runtime body
births may append objN values beyond the initial header. ENTITY changes the primary pose/motion columns
to that entity; gravity is sampled there. Without ENTITY, the legacy observer is used.
`SAMPLE_GRAVITY x y z` (repeatable) prints `sample,x,y,z,gx,gy,gz` from the world's
gravity resolver before the first frame and before the CSV header; it does not step.

Legacy HOLD W/A/S/D/Q/E/I/K/J/L/U/O begin end, TAP SPACE/R/F frame and LOOK remain,
using both legacy compatibility action state and normal physical input. Invalid/unbound
named input and expectations, wrong action/axis kinds, non-boolean expectations
and malformed commands fail. Test mode isolates desktop devices; ordinary
interactive mode retains normal input.

Captures run **InteractivePlay's normal renderer** after script presentationUpdate,
active view, interpolation, world and authored UI. Multiple captures and captures
while paused work. Fast deterministic frames skip only drawing when no capture is
requested. Simulation and all frame/UI/presentation callbacks continue normally.
The harness proves input/temporal routing, not human responsiveness or visual acceptance.

## Reproducibility and parsing limits

Launch with an explicit project or registered scene path. The no-argument legacy
test launch selects the old classic/terrain demonstration rather than an arbitrary
project's startup scene. Commands and file paths are whitespace-delimited; quoted
paths containing spaces are not parsed. Choose caller-owned paths without spaces
for harness scripts/captures, or use a different application's structured driver.
`#` starts a comment. Input intervals are `[begin,end)`; frame indices start at 0.
Frame count is 1–1,000,000 and frame seconds are finite 0–0.25.

Deterministic input injection deliberately does not poll desktop devices. It
cannot validate physical pointer confinement, IME, controller/touch hardware or
multi-player device assignment. A successful exit also does not prove every
physical endpoint: add an explicit oracle for the behaviour under investigation.
Any script diagnostic fails the normal harness run; output alone is not a pass.

Synthetic frames without drawing are not interactive FPS measurements. For dense
physics comparisons use the [measurement tool and recorded workload](PREALPHA_DENSE_SUPPORT_FOLLOWUP.md)
and distinguish per-step cost, simulated time, real-time catch-up and rendered FPS.

## Shared application services (post-M65 correction)

The Application harness now uses the shipping outer boundary after each frame:
streaming, save/load publication, then queued scene replacement. A request issued
inside a callback never destroys that callback's world. Paused frames still advance
these services. Each scripted frame has its own M56 frame boundary; startup is a
separate record. Isolated callers that omit the outer callbacks remain isolated
InteractivePlay tests, not full application-service tests.

`WAIT_SERVICES scene|save|stream timeout-ms frame` waits at the specified scripted
frame for pending scene replacement, save/load work, or streaming transactions.
The timeout must be 1–60000 ms. Waiting uses zero-clock application frames with
normal UI callbacks and the same resource/service boundary; it does not accumulate
fixed steps. Each wait frame is separately profiled and reported. A timeout names
the service and scripted frame. Use this after requesting asynchronous work rather
than assuming an arbitrary synthetic step count gives workers enough wall time.

`EXPECT_SCENE Scenes/example.judas frame` verifies the current registered scene after an earlier bounded scene-service wait.

Frames that skip drawing still publish/update ordinary audio voices so save prefill can complete. Bounded service waits use zero-clock frames and do not invent fixed steps.

## Window size and rotation-layout tests

The desktop runtime accepts `JUDAS_WINDOW_SIZE=WIDTHxHEIGHT` at startup. Without
it, the default remains 1024×768. Dimensions must be integers from 1 to 8192;
malformed values fail before window creation. This development/test override
also works for ordinary standalone desktop launches; it does not affect the
editor's saved workspace or Android's native surface size.

`RESIZE width height frame` changes the real desktop window before that frame's
input injection and gameplay callbacks. `EXPECT_VIEWPORT width height frame`
checks both the actual window and published gameplay viewport after the frame.
No real input devices are polled. Pointer coordinates remain window pixels;
touch coordinates remain normalized. Resize commands execute before the frame's
pointer commands, independently of their order in the file.

```text
FRAMES 6
LOG_EVERY 0
EXPECT_VIEWPORT 800 360 0
SCREENSHOT 1 /tmp/hinge-landscape.png
RESIZE 360 800 2
EXPECT_VIEWPORT 360 800 2
SCREENSHOT 3 /tmp/hinge-portrait.png
RESIZE 800 360 4
SCREENSHOT 5 /tmp/hinge-return.png
```

```sh
JUDAS_WINDOW_SIZE=800x360 JUDAS_TEST_SCRIPT=/tmp/aspects.txt \
  build/judas path/to/hinge.judasproj
```

These exercise desktop UI/camera layout changes, not Android sensors, density,
IME, cutouts or physical-device acceptance. A window manager may refuse a size;
the harness reports that as a failure instead of silently substituting dimensions.
Android rejects `RESIZE`; its activity owns surface dimensions. SDL's offscreen
backend also rejects mid-run resize because its GL buffer retains the startup
size; use a desktop backend or Xvfb for resize tests, or separate startup-size runs.
