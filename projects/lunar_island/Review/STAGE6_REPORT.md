# Lunar Island — Stage 6 qualification and handover

Status: implemented and Linux-automated; final operator gameplay acceptance pending.
Parent Experimental: `bda1c77c655479564e09c82892f8d5b708d72556` (Stage 5).
Main remains `c2cdd376f1c20a9d6a8a75d8257f9ac44327ec09`.

The operator reviewed Stage 5, reported a slow/jittery helicopter, and explicitly
approved proceeding into Stage 6. This report does not invent Stage 6 human testing.
All new implementation is ordinary Lunar Island game code/tools/docs. No native
engine, API, research, historical evidence or other project changed.

## Helicopter defect and correction

The body-free vehicle shell is already a child of its native interpolated rigid
parent. Vehicle.presentationUpdate nevertheless assigned the parent's presented
world pose back to that child each frame. A world transform write is converted to
local coordinates using the **authoritative** parent. Rendering then applies the
**presented** parent again. In transform notation the unintended result was
`P_presented * inverse(P_current) * P_presented` instead of `P_presented`.
The rotor mount suffered the same double application. The camera itself correctly
used the presented parent; this exposed the disagreement visually.

The correction leaves the authored shell child alone. Native hierarchy rendering
now interpolates it once. Only rotor local animation is updated, by constructing
its world write from the authoritative parent so local conversion cancels exactly.
Blade rotation uses the native presentation callback's alpha and the previous
fixed-step rotor angle/increment; it never drives physics. This applies to plane
propellers and body-free vehicle shells as well as the helicopter.

The old helicopter requested only `atan(0.18)` forward tilt with linear drag 0.15,
limiting useful cruise to about 41 km/h. Its former climb command was 3 m/s.
The revised project-owned assisted controller requests an attitude based on the
difference between desired horizontal travel and actual horizontal velocity,
bounded to 0.48 radians. Native torque, shaft-powered lift, gravity, drag and
collisions still move the body. Releasing controls requests an opposing attitude
and then hover. There are **no flight pose/velocity assignments**.

The vertical controller accounts for tilt's loss of vertical lift. Cruise/response,
tilt, drag, climb/descent and hover input are data in Authoring/vehicles.json,
vehicle_manifest.json and generated vehicle-data.js. Default cruise request is
45 m/s; with drag, measured equilibrium is about 38.55 m/s. Climb/descent requests
are 5/3 m/s. These are deliberately assisted game controls, not an industrial rotor model.
Fuel, health, native powertrain and momentum remain physical/persistent participants.

### Preserved experiments

The same actual application scenario runs 90 rendered Hz against unchanged 60 Hz
physics, boards through ordinary E/menu input, climbs, flies forward for 20 seconds,
then releases controls for 10 seconds. There is no vehicle teleport or velocity
injection after boarding. Native presentation measurements use the renderer's
actual interpolation fraction, not an alpha chosen to make the result agree.

| Measurement | Original | First correction | Final |
| --- | ---: | ---: | ---: |
| Peak cruise speed | 11.305 m/s | 43.751 m/s | 38.546 m/s |
| Shell position disagreement | 125.6 mm | 0 | 0 |
| Rotor mount disagreement | 125.6 mm | <0.1 mm | <0.1 mm |
| Speed after release/hover phase | 2.582 m/s | 16.410 m/s | 0.102 m/s |
| Focused flight checks | 22 / 3 failed | 22 / 1 failed | 22 / 0 failed |

The first correction fixed the presentation bug and speed, but lower drag left an
unpleasant long glide. Its failed hover check is preserved. The final correction
uses a physical braking attitude rather than forcing speed to zero or adding an
arbitrary high damping phase. Original/intermediate/final logs, frame traces,
physical endpoints and screenshots remain in stage6/flight-*.

## Usability and handover

- Shorter first-play welcome gives gathering, home and travel next steps.
- Controls separates on-foot actions from a Vehicles & travel page, retaining all
  other menu tabs and controls. The sensitivity slider reflects saved sensitivity.
- Boarding hints and the footer now distinguish road/boat/helicopter/plane controls;
  flight no longer tells the player to use a tool or brake with its climb control.
- Stop tracking clears the map guide and restores the progression/quest journal.
- PLAYER_GUIDE.md explains a first expedition, vehicles, survival, saves and common
  refusals. HANDOVER.md records content extension paths, generation boundaries,
  ownership rules and unfinished content. Existing animation work remains explicit.
- tools/package_game.py calls the ordinary exporter, adds the player guide and a
  path-safe `Run Lunar Island.sh`. Existing output directories are refused.
- Save metadata now identifies Stage 6 rather than the stale Stage 3 label.

Stage 6 has its own save identity. Earlier-stage slots and the old Stage 5 export
are preserved. No automatic save migration is claimed. Matching source/content is
required within a write/read pair; the inherited native compatibility policy is
not bypassed. There is still one manually saved expedition slot per stage identity.

## Qualification

`stage6/FINAL_TEST_RESULTS.json` records 17 distinct final scenarios: **559 checks,
zero failures**. Individual counts below avoid adding repeated development reruns
as new coverage. Final narrow flight/UI reruns followed the last contextual text
change; remaining combined scenarios ran with final runtime content. Additional
observer-only modes do not change the native engine or shipped gameplay.

| Scenario | Checks / failures |
| --- | ---: |
| helicopter-cruise | 22 / 0 |
| onboarding | 30 / 0 |
| land-helicopter | 17 / 0 |
| land-plane | 17 / 0 |
| drive-plane | 19 / 0 |
| drive-car | 18 / 0 |
| worldwrite | 38 / 0 |
| worldread | 25 / 0 |
| write | 133 / 0 |
| read | 42 / 0 |
| cargo | 55 / 0 |
| cargoread | 44 / 0 |
| harvestwrite | 37 / 0 |
| harvestread | 17 / 0 |
| performance | 10 / 0 |
| flightwrite | 15 / 0 |
| flightread | 20 / 0 |

The flight cold-save pair verifies actual airborne body position, quaternion,
velocity, spinning native shaft, fuel and disabled pilot motor, then resumes real
climb under input. Other cases exercise real resource harvesting, tree falling,
construction/joints/doors, enemy death, cargo/services, modified-region suspension
and genuine fresh-process restores. Test-only player placement/aim reaches remote
fixtures; it does not substitute inventory, damage, successful physical outcomes
or ordinary driving. The flight probe's WorldOverlay only observes presentation.

Pure inventory/crafting rules: 83; encounters: 37; vehicle/cargo rules: 20. Native
script preflight: 30 modules, zero compile/link failures. Native engine binaries
are byte-identical to the previous accepted Stage 4/5 tests, so no ceremonial
engine rebuild or full production-suite rerun was performed.

Editor test: isolated project copy and preferences, duplicate/undo, Play/Stop,
authored scene **IDENTICAL**, save succeeds, resources zero failures, jobs zero
failures. Controls/travel screenshots were visually inspected for readability at
the tested 1024×768 size. This does not claim exhaustive resolution/accessibility QA.

### Performance

Ryzen 5 5600, GTX 970 4 GB, 8 GB RAM, Linux. Hidden rendered 1024×768 application,
vsync disabled, dummy audio; supplied simulation clock. No other Judas game was
present in process inspection, though normal desktop applications remained running.
These measure workload throughput, not human monitor FPS or isolated GPU benchmarks.

| 600 representative root frames, 323 bodies | Mean | Median | p95 | Worst |
| --- | ---: | ---: | ---: | ---: |
| Frame | 8.537 ms | 8.246 ms | 10.065 ms | 14.334 ms |
| Physics | 0.147 ms | 0.143 ms | 0.175 ms | 0.327 ms |

The matched Stage 5 root mean was 8.451 ms. This small difference is not a claimed
performance regression or speedup; Stage 6's meaningful improvement is corrected
presentation and useful flight control, not an engine optimization claim.

Flight trace: all 2,700 recorded cruise/hover frames mean 5.104 ms, p95 8.232 ms,
worst 72.553 ms. The first recorded frame and frame +1800 include explicit test
GPU screenshot readback/PNG writes in beforeFrame, which the native application
clock includes. Excluding **only these two named interventions**, 2,698 frames
mean 5.065 ms, p95 8.226 ms, worst 20.880 ms. Both summaries and every raw frame
are preserved in FLIGHT_PERFORMANCE.json/flight.csv. No other slow frames are
dropped and no universal hitch-free frame-time guarantee is claimed. Previous
startup navigation validation remains a synchronous loading limitation.

### Shipping package and provenance

Ordinary exporter: 176 assets, 26 scenes, 241,608,799 bytes, 7.984 seconds, followed
by the small launcher/player-guide additions. The complete package was moved to
`/tmp/Lunar Island Stage6 Relocated`, launched through its new shell launcher from
unrelated `/tmp`, and loaded the genuine modified-world save. Exit zero, no script,
resource or harness failure. The original package is:
`.cache/lunar-island-stage6/Lunar Island/`.

Native runtime/editor/export tools retain their actual embedded prior Release
build identity (`5424d85` with its recorded dirty fingerprint), including the
already accepted Stage 2 persistence repairs. Stage 6 did not rebuild or relabel
that native source as a fresh clean engine checkpoint. Exact native bytes and
package BUILD_PROVENANCE.json accompany game source/evidence hashes.

## Remaining boundaries and readiness

This is ready for final Linux operator review and continued game development,
not certification of the whole long-term commercial vision. Important remaining
content/quality work is explicitly listed in HANDOVER.md:

- Coarse resident island shell and authored quarters, not dense development of a
  verified 64 km² land area; no digging/water-flow gameplay in this project.
- Basic enemy/survivor policy and authored quests, not a systemic economy, deep
  survivor routines or dynamic raids. Clothing/medical/progression systems are a
  bounded subset, not complete commercial RPG systems.
- Reused modular houses and sealed landmark façades; industrial/city art needs
  further authored variety. Human/animal combat animation gaps remain; physical
  corpse proxies are not full articulated ragdolls.
- Assisted vehicles, simple flotation and collision proxies; cargo does not alter
  chassis inertia, driven vehicles remain resident, no traffic or AI pilots.
- Manual single-slot content-strict saves, separate stage identities, no published
  upgrade migration policy. No new platform/controller/listening certification.

No new native engine defect was established. The helicopter failure was a project
presentation mistake and gameplay tuning issue; the bounded native persistence
repairs remain historical Stage 2 work. Protected research, all Stage 1–5 evidence,
existing games, unrelated untracked work and main are unchanged. No M100, release
channel change, tag or engine milestone was started.

Use stage6/HUMAN_CHECKLIST.md for acceptance. Exact changed-file inventory and
source/binary/evidence/package fingerprints are adjacent. The final game checkpoint
is pushed to Experimental only; main promotion still requires explicit approval.
