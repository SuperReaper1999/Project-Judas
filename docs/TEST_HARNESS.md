# Current application harness (M65)

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
supplies pointer position. `EXPECT_AXIS name value frame`, `EXPECT_HELD name 0|1 frame`,
`EXPECT_PAUSED 0|1 frame` examine post-frame state (consumed UI actions may read zero).

`FRAMES` and legacy `STEPS` count application frames. Frame seconds defaults to 1/60;
0–0.25 supports paused UI clocks / several fixed steps. `REALTIME n` uses wall time
and the normal render/swap path; simulation never receives profiler measurements.
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
