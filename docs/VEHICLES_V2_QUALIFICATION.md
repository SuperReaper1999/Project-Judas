# Vehicles V2 qualification

Work begins at Experimental `cd4cb87e41f0fc6bc1b7928f6649de3274ba94a4`.
Finite-width contact checkpoint: `a896950554688699c51fa47b7ce179d6d8a67a6d`.
MK2 coupling / support lifecycle / authoring checkpoint: `da4678d2dcef240b2b56d154ace3e96294c21d16`.
Final tested engine implementation: `1afc3ff23833c72ede30e86d2d8ae1a9b70d7adb`. Subsequent handover documentation changes do not alter that implementation.
This is source-preview engineering, not a release or operator acceptance.
Main and the Advanced Fluids lock remain unchanged. The existing Terrain Workshop edits and RPG SDK are excluded.

## Confirmed physical corrections

- Wall/floor selection: 11 native contact checks, including both surfaces inside one concave collider, finite tread width, filters and authored identity.
- Production reversed component registration: 0 m trajectory difference after ten seconds. Independent deliberately unsorted/reversed constraint sweeps: 0.00327 m versus the investigation's 2.532117 m one-pass discrepancy. This is host reproducibility within the declared 1 cm control, not universal bit-identical determinism.
- 81 dynamics/integration checks: natural MK2 sleeping, torque wake, removed/moved/new supports, six/eight-wheel loads and drive, prefab authoring, passive energy, articulation, rolling speeds, overflow rejection, native terrain, excavation and origin shifts.
- Passive control: 6937.5 J initial and maximum, 6123.94 J final. Damping/friction dissipate energy; there is no artificial energy clamp.
- Actual articulated towing: maximum ball-hitch separation 0.000558079 m, with unpowered trailer travel.
- 5/15/30/50 m/s level rolling controls retain support and independently expected distance.
- Native Terrain V2 supports a sleeping chassis; after a 1024 m origin shift, the contact/chassis frame error is below 1e-5 m. Excavation under the chassis republishes collision, invalidates support and wakes/falls the vehicle.
- Structural subdivision controls (1, 2, 4): four vehicle preparation intervals and identical integrated scalar drive effort.
- Genuine writer/reader process restoration: driven body position error 0 m; velocity, wheel phase/spin and rotor momentum satisfy 0.001 tolerances. This uses durable SaveStorage plus normal WorldPersistence reconstruction.
- Existing native suites pass: wheels 30, wheel lifecycle 51, wheel scripting 18, powertrains 58, powertrain lifecycle 97, powertrain scripting 17, joints 29; MK2 suite passes. Broader coordinated qualification is recorded separately below when complete.

## Authoring and actual game checks

The 15-check editor workflow creates a fresh project and real vehicle, compiles its script, verifies one-step Undo/Redo, persists bindings, authors a prefab, creates child truck/bus/trailer layouts and rejects scaled parents atomically. The exact unmodified starter camera is superseded; user scripts are preserved.

The GTX 970/OpenGL 4.6 Workshop runs ordinary project scripts. Car/truck/bus/tow rig each accelerate and steer; measured four-second driven distances are 21.32 / 32.41 / 24.18 / 18.93 m. After two seconds of braking their speeds are 0.0014 / 0.1432 / 0.0034 / 0.0017 m/s. Initial shared 600 N m brakes were underpowered for heavy layouts; the game-side recipe now scales effort with supported weight/radius. Native friction limits remain unchanged. These are automated input/capture results, not human acceptance.

A separate rendered process restores the saved tow rig and trailer through SaveService. HUD confirms restoration; physical continuation has the independent native oracle above. The course includes a native Terrain V2 off-road patch without hidden box support underneath it.

## Runtime cost

Ryzen 5 5600, GCC 15 Release, 60 Hz. Three fresh processes, each 300 settling steps and 1200 workload steps, first 120 excluded: 1080 measured samples per case/run. No compilation ran during these measurements. Reproduce with `cmake --build build --target judas_vehicle_benchmark` followed by `build/judas_vehicle_benchmark` after other compiler jobs finish. Numbers below are the median of per-run percentiles, with the largest observed worst. Profiler enabled; whole-step times include capture overhead and ordinary world orchestration, not rendering.

| Workload | p50 ms | p95 ms | p99 ms | Worst ms |
| --- | ---: | ---: | ---: | ---: |
| One parked vehicle | 0.0430 | 0.0474 | 0.0534 | 0.1319 |
| One driven vehicle | 0.2123 | 0.2191 | 0.2316 | 0.4109 |
| 25 parked vehicles / 100 wheels | 0.3813 | 0.3928 | 0.4072 | 0.8159 |
| Six driven vehicles / rough strips | 1.4247 | 1.4809 | 1.4987 | 2.0972 |

All 25 parked chassis sleep while retaining 100 contacts; all 24 rough-course wheels are supported at the final sample. Contact preparation dominates active-vehicle work. Fifteen finite-width probes and four coupled intervals intentionally cost more than one sphere/one-pass V1. This is a correctness improvement, not a claimed blanket speedup. Rigid-physics profiler scopes include nested vehicle callbacks; do not add them to wheel scopes or subtract their percentiles.

## Matched powered-vehicle comparison

Exact baseline cd4 and V2 1af were built independently with the same GCC 15 Release flags. Identical benchmark source/fixture hashes, layouts, commands, fixed steps and geometry were used. Three fresh process pairs alternated order on CPU 2, with compiler jobs stopped. The following whole-step measurements have **profiling disabled**, as in ordinary play. Each non-impact case retains 1080 steps per run; impact retains all 1200. These are shared-desktop measurements, not hard deadlines.

| Workload | Implementation | p50 ms | p95 ms | p99 ms | Worst ms |
| --- | --- | ---: | ---: | ---: | ---: |
| One parked powered vehicle | V1 | 0.0313 | 0.0333 | 0.0399 | 0.4854 |
| One parked powered vehicle | V2 | 0.0728 | 0.0774 | 0.0919 | 0.5590 |
| One driven powered vehicle | V1 | 0.0346 | 0.0371 | 0.0514 | 0.5010 |
| One driven powered vehicle | V2 | 0.2162 | 0.2261 | 0.2639 | 0.6683 |
| 25 parked powered vehicles | V1 | 0.4445 | 0.4840 | 0.7413 | 0.8332 |
| 25 parked powered vehicles | V2 | 1.6398 | 1.9545 | 2.0059 | 2.1185 |
| Six powered vehicles on rough strips | V1 | 0.1324 | 0.1449 | 0.2023 | 0.4524 |
| Six powered vehicles on rough strips | V2 | 1.5595 | 1.7626 | 1.8929 | 3.2906 |
| Two powered vehicles, head-on | V1 | 0.0472 | 0.0531 | 0.0724 | 0.4150 |
| Two powered vehicles, head-on | V2 | 0.2496 | 0.4516 | 0.5038 | 0.8539 |

V2 is more expensive than the old sphere/one-pass model; this pass does not claim a per-vehicle CPU speedup. Four coupled intervals and finite-width contact buy corrected support and load response. The input workload is matched, but intentional physical corrections change trajectories: on the rough course V1 advances only 0.392 m while V2 advances 35.115 m; on flat ground they advance 185.009 / 184.839 m. The old parked vehicle drifts 0.0568 m and never sleeps; V2 remains stationary and all 25 powered parked chassis sleep. Head-on minimum chassis height improves from 0.5743 to 0.7785 m. Those differences must not be disguised as equivalent physical work.

Normal-process peak RSS was 34,752 KiB baseline / 34,668 KiB V2. Diagnostic capture increases residency: 42,996 / 74,032 KiB with profiling enabled. All tested physical outcomes are identical between capture-on and capture-off runs. The benchmark reported zero incomplete frames; the separate live GPU trace also reported zero dropped or truncated records.

Profiled median phase costs expose the remaining work: one driven powered vehicle spends 0.1139 ms in contact preparation, 0.0431 ms in suspension/tyres and 0.0523 ms in mechanical graphs; six rough-course vehicles spend 0.9895 / 0.2655 / 0.3170 ms respectively. The 25 parked fleet still spends 1.3046 ms reprojecting mechanical graphs, despite cached support and sleeping rigid bodies. An exact idle-graph fast path is a useful future optimization. It is not implemented or counted as a speedup here. The enclosing rigid scope includes these callbacks, so its timing is not an independently isolated MK2 cost and percentiles must not be added or subtracted.

## Desktop rendering and memory

Actual GTX 970/OpenGL 4.6, 1280×720, 1800 REALTIME application frames with native presentation and driving. The retained steady window contains 119 rendered frames and six fixed steps; it is not a complete 1800-frame percentile distribution. Harness swap synchronization is disabled.

| Retained measurement | p50 ms | p95 ms | p99 ms | Worst ms |
| --- | ---: | ---: | ---: | ---: |
| Outer application work | 0.8096 | 1.9704 | 1.9885 | 2.5584 |
| CPU world render submission | 0.1894 | 0.2117 | 0.2552 | 0.3001 |
| GPU main-camera pass | 0.7836 | 0.7859 | 0.7921 | 0.7949 |
| GPU shadow pass | 0.1183 | 0.1199 | 0.1274 | 0.1292 |

GPU passes can nest; do not sum them into a frame cost. Startup is recorded separately at 199.864 ms. Process residency in the retained window was 169,881,600 bytes, including a 70,537,152-byte profiler reservation estimate. No accumulator caps, dropped events, truncated records or GPU drops occurred. Portable OpenGL 3.3 also completed the tow-rig driving test. This small course is not a claim about arbitrary city-sized scenes or hardware.

## Completed qualification

- Native coordinated coverage: **140 cases**, with five environment failures preserved and unchanged-binary/source-hash targeted retests passing on desktop X11 with loopback socket access. The initial restricted invocation could not perform native offscreen resizing or listen on sockets. All **20 supplemental commands** passed, including genuine cold readers, joints, wheels, powertrains, characters, gravity and tooling controls.
- Independent [Ubuntu 24.04 CI](https://github.com/SuperReaper1999/Project-Judas/actions/runs/42691676009): Experimental **178 passes**; Alpha **174 passes and four correctly unavailable locked-fluid cases**, zero failures. Both jobs also passed their SDK ABI/loader checks with the glibc 2.39 ceiling. Current documentation and tooling jobs passed.
- Clean host-native SDK: all **15 tools** verified; **39 tool-operation commands** and eight orchestration stages passed, including fresh project creation, authoring, editor Play/Stop, imports/cooks, saving and moved editor/CLI exports.
- The SDK was read-only and the entire development checkout was hidden in the qualification namespace. A separately relocated Vehicle Workshop saved and loaded in different processes with **both SDK and source project hidden**; its runtime hash matched the packaged SDK. Restored tow-vehicle Z was 3.909492 m, rather than the initial 10 m.
- Namespace runs used software graphics and the host Linux libraries; they are not Windows, Android, clean-machine or physical GPU acceptance. The local preview SDK is host-native **glibc 2.43**, not the Ubuntu 24.04 CI SDK.
- SDK source revision and payload are authoritative in its BUILD_INFO.json / SDK_MANIFEST.json. Final documentation-only checkpoints can refresh those identities without changing the qualified engine implementation.

[Compact receipts, raw matched rows and provenance](https://github.com/SuperReaper1999/Project-Judas/tree/Experimental/docs/evidence/vehicles-v2) are tracked. Full local records remain in .cache/vehicles-v2, including failed attempts. No pending CI is counted as passing.

The extended controls include head-on vehicle collisions and 20 seconds of driven travel over 316 m with an actual 512 m origin recenter: absolute trajectory error 0.0000626 m. A deliberately opposite-direction shift moving a near-origin car to −1024 m quantized tiny lateral float increments and differed by 0.0256 m over 20 seconds; a proposed local-lever rewrite did not repair that and was rejected. This is a precision-chart limitation, not a justified vehicle rewrite. The recenter test respects the existing 512 m anchor contract and retains its original 2 cm tolerance.

## Human checklist

1. Open `projects/vehicle_v2_workshop/Vehicle_V2_Workshop.judasproj` in the matching editor. Play and drive all four selections; steer, brake, reverse and tow.
2. Inspect wheels in Play: contact, travel/load, slip/grip, rpm and diagnostic. Enable collision/contact debug overlays.
3. Drive over ramps/rough strips and onto the native terrain to the right; sculpt/excavate the patch in Edit and try the changed route in Play.
4. Save with K, quit, reopen and load with L; verify the chosen rig and trailer pose. R restarts; Esc pauses.
5. In a fresh project create car/truck/bus/trailer, inspect/edit components and driver, Undo/Redo, make a prefab, save/reopen and export.
6. Judge steering, suspension, load transfer, collision and camera feel. Automation does not make those human acceptance decisions.

Current modelling limits and deferred capabilities are explicit in [Vehicles V2](VEHICLES_V2.md). Windows/Android/browser native execution is not yet qualified for this change.

## Human review and towing follow-up — 2026-11-09

The operator completed hands-on Vehicles V2 review and accepted the tested
capabilities except towing. The connected tractor could turn to face the trailer
while decelerating in a turn. The operator authorized conditional acceptance after
that correction and successful qualification; no second human test is implied.
See [the towing follow-up](VEHICLES_V2_TOWING.md) for the reproduced mechanism,
current controls, regression gates, measurements and qualification scope.
The original measurements above remain scoped to their recorded checkpoints.
