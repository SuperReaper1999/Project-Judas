# Hearthworld validation

Native Linux functional qualification of the staged Hearthworld consumer candidate.
No human acceptance is inferred. No engine runtime API or physics implementation
was changed by this validation work.

## Final reproduction

From the repository root, after the Release application observer is built:

```sh
env DISPLAY=:0 WAYLAND_DISPLAY=wayland-0 XDG_RUNTIME_DIR=/run/user/1000 SDL_VIDEODRIVER=x11 \
  python3 scripts/hearthworld_validate.py \
  --binary .cache/prealpha-doc-cleanup-20261019/build-make/judas_hearthworld_application_tests \
  --output .cache/hearthworld-game/another-fresh-result --performance
```

Use a new output directory. The runner refuses to overwrite a previous result.
The normal build target is `judas_hearthworld_application_tests`.

Final raw result: `attempt-04/RESULTS.json`. All source/binary inputs remained
unchanged across the four processes. Exact observer and runner source bytes are
preserved in `attempt-04/observer-source/`; `INPUTS.json` records engine, project
and executable hashes. Linux uses a real NVIDIA OpenGL context, not software GL:
GTX 970, driver 580.178.04, on the i7-4790 development host.

| Process | Assertions | Result | Wall time |
|---|---:|---|---:|
| Shipped gameplay + exact cold writer | 66 | 0 failures | 6.287 s |
| Independent fresh-process reader | 41 | 0 failures | 0.916 s |
| Actual save/menu controls | 20 | 0 failures | 1.367 s |
| Restored-world profiling | 42 | 0 failures | 2.920 s |

169 assertions cover these distinct behaviours; they are not 169 independent
engine certifications. The application loop owns scripts, resource publication,
fixed steps, rendering, UI and save services. Fixture placement changes only the
player pose/camera for repeatable targeting. It never sets resource quantities,
target poses, terrain density or construction results. Tool use, harvesting,
mining, gravity-gun control and construction execute the shipped JavaScript.

## Actual gameplay and negative controls

- Normal logical movement advances a supported CharacterMotor. A second actual
  walking case on the planet's side follows radial gravity and the planetary
  collider, with no world-Y gravity override.
- Three real E interactions collect the authored tools and retire their pickups.
- Wrong-tool impact yields no stone. Insufficient resources and a physically
  obstructed placement create no construction and debit no resources.
- Three axe hits release the static 60 kg trunk as a dynamic body. The test waits
  for its actual physical rotation before processing it. Processing creates three
  independent collidable logs; actual E pickups give exactly nine wood.
- Six valid pickaxe hits produce six stone and exhaust the finite physical node.
- The gravity gun grips the real finite-mass crate, moves it through forces as
  the camera turns, then releases it with actual forward launch momentum.
- A floor, connected wall and stone foundation are real prefab bodies with
  ordinary fixed joints and collision. The resource balance is nine wood minus
  eight spent, and six stone minus six spent.
- A shovel dab changes authoritative M97 density samples and the actual raycast
  endpoint. Initial measured edit latency is 37.995 ms, including 27.595 ms CPU
  preparation and 0.051 ms publication.
- An unrelated excavation retains the floor and attached wall. One additional
  shovel dab beside the stone foundation removes its real support, releases its
  joint and lets the body physically fall/tip. The independent floor and wall
  remain connected. No body pose is reset to obtain the result.
- Actual F5 saves while paused. Held movement/use/grab inputs do not leak through
  the modal. After collecting another tool, actual F9 restores both inventory and
  the collected/uncollected physical pickup entities. Tab opens the real inventory.
  A normal pointer click on Save and Quit exits only after a new durable home save
  exists; this is not a direct invocation of `onUI`.

## Cold-save boundary and preserved failed observations

The exact cold reader compares real physical state before the replacement world's
first fixed step, then compares live script state after its first ordinary
constructor/restore lifecycle. Six dynamic-body records and the motor endpoint
match the observed capture boundary exactly, including pose and momentum. The
reader also checks actual query-capsule publication, collision/joint ownership,
inventory, tools, harvest records, construction, counters, density bytes and an
excavated collision endpoint. Original durable save bytes remain unchanged.

The earlier failures are retained:

1. `attempt-01`: the project scene and an asset metadata file changed during the
   run. The content-identity guard correctly rejected the old save in the new
   content. The runner independently detected those changed input hashes. This
   run cannot qualify a frozen candidate.
2. `attempt-02`: the original observer incorrectly equated save-request time with
   save-capture time and attempted to inspect live JS instances before their
   first lifecycle boundary. Its original failure logs and snapshots remain.
3. `attempt-03`: instrumentation established the distinction. SaveService refreshes
   the asynchronous content identity when queuing a request, and may wait before
   capturing while the ordinary world continues. Request frame 822 preceded the
   observed capture boundary at frame 839. Comparing the capture-boundary dump
   with the independent restore dump gives exactly zero difference for every
   saved dynamic pose/momentum value and for the motor endpoint. Actual restored
   JS state passes after normal instance initialization. The four remaining failed
   assertions in that run compare against the demonstrably wrong request endpoint.

The corrected observer preserves both `expected-pre-request.json` and the first
`writing`-state boundary after actual capture. It inserts no settling steps,
changes no thresholds and makes no production correction. Original failed-run
observer bytes were not separately snapshotted before editing; their hashes are
retained, and later source is not misrepresented as a byte-identical copy.

## Representative performance

`attempt-04/application/PERFORMANCE.json`, raw CSV and the M56 JSON profile contain
600 normal application frames at a controlled 1/60 s simulation clock, after
loading the actual harvested/excavated/constructed world. The player walks for
400 frames and rests for 200. There are 124 physical bodies, six dynamic, with
most world content static. This is the intentionally modest consumer workload,
not a large dense-pile scalability claim.

| Measurement | Median | p95 | Maximum |
|---|---:|---:|---:|
| All 600 observed frame intervals | 3.206 ms | 4.102 ms | 152.093 ms |
| Physics step | 0.0309 ms | 0.4462 ms | 0.7424 ms |
| M56 JavaScript phase scopes | 0.4174 ms | 0.5575 ms | 2.3687 ms |

Peak observed M56 process RSS is 222.6 MiB, including profiling. The retained 117
completed main-camera GPU samples have median 0.7854 ms, p95 1.0657 ms and maximum
1.1636 ms. All M56 overflow/incomplete/drop diagnostics are zero. The controlled
clock allows simulation to run faster than real time; these costs are not a
claim about observed human-play FPS.

### Cold-load hitch remains visible

The 152.093 ms value is the first restored-world frame interval. It includes the
previous outer frame's cold replacement work. M56 frame 34 (startup adds one to
the application-frame index) records 104.002 ms in World build, 34.943 ms in Save
private preparation (including 34.128 ms Save restoration), and 1.013 ms in Save
activation. Its main-camera GPU cost is 1.693 ms. This is demonstrated CPU cold
world reconstruction/restore cost, not evidence of an expensive physics step or
of GPU upload being the dominant cause. No unrelated engine optimization was
attempted.

For the separately labelled 599 intervals after that one cold transition, median
is 3.205 ms, p95 4.087 ms and maximum 10.595 ms. The original 600-sample maximum
remains authoritative and retained. `HITCH_CLASSIFICATION.json` maps the raw
observations to the actual M56 scopes. The profiler's final observer frame also
contains screenshot/report work and is not silently presented as normal gameplay.

## Evidence and boundaries

Useful native captures: `write-initial.png`, `tree-fallen.png`, `gravity-held.png`,
`write-completed-gameplay.png`, `read-restored-gameplay.png`, `controls-pause.png`,
`controls-inventory.png`, and `performance.png` in the final application directory.
These are actual game frames. No synthetic image substitutes for runtime proof.

Windows and Android were not freshly qualified. Full production suites were not
rerun for this ordinary project. The M97 bounded volume remains the only diggable
region; the surrounding planetary collider is not secretly excavatable. Physics
objects and joints are real, but this small game does not claim unlimited building
counts, planetary terrain mutation or a fluid-filled swimming-pool simulation.
Human gameplay/visual acceptance remains pending. Protected history, independent
fluid research, existing games and unrelated untracked assets were untouched.
