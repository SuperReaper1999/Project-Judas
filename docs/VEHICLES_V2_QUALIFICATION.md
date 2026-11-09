# Vehicles V2 qualification

Work begins at Experimental `cd4cb87e41f0fc6bc1b7928f6649de3274ba94a4`.
Finite-width contact checkpoint: `a896950554688699c51fa47b7ce179d6d8a67a6d`.
MK2 coupling / support lifecycle / authoring checkpoint: `da4678d2dcef240b2b56d154ace3e96294c21d16`.
This is source-preview engineering, not a release or operator acceptance.
Main and the Advanced Fluids lock remain unchanged. The existing Terrain Workshop edits and RPG SDK are excluded.

## Confirmed physical corrections

- Wall/floor selection: 11 native contact checks, including both surfaces inside one concave collider, finite tread width, filters and authored identity.
- Production reversed component registration: 0 m trajectory difference after ten seconds. Independent deliberately unsorted/reversed constraint sweeps: 0.00327 m versus the investigation's 2.532117 m one-pass discrepancy. This is host reproducibility within the declared 1 cm control, not universal bit-identical determinism.
- 74 dynamics/integration checks: natural MK2 sleeping, torque wake, removed/moved/new supports, six/eight-wheel loads and drive, prefab authoring, passive energy, articulation, rolling speeds, overflow rejection, native terrain, excavation and origin shifts.
- Passive control: 6937.5 J initial and maximum, 6123.94 J final. Damping/friction dissipate energy; there is no artificial energy clamp.
- Actual articulated towing: maximum ball-hitch separation 0.000558079 m, with unpowered trailer travel.
- 5/15/30/50 m/s level rolling controls retain support and independently expected distance.
- Native Terrain V2 supports a sleeping chassis; after a 1024 m origin shift, the contact/chassis frame error is below 1e-5 m. Excavation under the chassis republishes collision, invalidates support and wakes/falls the vehicle.
- Structural subdivision controls (1, 2, 4): four vehicle preparation intervals and identical integrated scalar drive effort.
- Genuine writer/reader process restoration: driven body position error 0 m; velocity, wheel phase/spin and rotor momentum satisfy 0.001 tolerances. This uses durable SaveStorage plus normal WorldPersistence reconstruction.
- Existing native suites pass: wheels 30, wheel lifecycle 51, wheel scripting 18, powertrains 58, powertrain lifecycle 97, powertrain scripting 17, joints 29; MK2 suite passes. Broader coordinated qualification is recorded separately below when complete.

## Authoring and actual game checks

The 14-check editor workflow creates a fresh project and real vehicle, compiles its script, verifies one-step Undo/Redo, persists bindings, authors a prefab, creates child truck/bus/trailer layouts and rejects scaled parents atomically. The exact unmodified starter camera is superseded; user scripts are preserved.

The GTX 970/OpenGL 4.6 Workshop runs ordinary project scripts. Car/truck/bus/tow rig each accelerate and steer; measured four-second driven distances are 21.32 / 32.41 / 24.18 / 18.93 m. After two seconds of braking their speeds are 0.0014 / 0.1432 / 0.0034 / 0.0017 m/s. Initial shared 600 N m brakes were underpowered for heavy layouts; the game-side recipe now scales effort with supported weight/radius. Native friction limits remain unchanged. These are automated input/capture results, not human acceptance.

A separate rendered process restores the saved tow rig and trailer through SaveService. HUD confirms restoration; physical continuation has the independent native oracle above. The course includes a native Terrain V2 off-road patch without hidden box support underneath it.

## Runtime cost

Ryzen 5 5600, GCC 15 Release, 60 Hz. Three fresh processes, each 300 settling steps and 1200 workload steps, first 120 excluded: 1080 measured samples per case/run. No compilation ran during these measurements. Numbers below are the median of per-run percentiles, with the largest observed worst. Profiler enabled; whole-step times include capture overhead and ordinary world orchestration, not rendering.

| Workload | p50 ms | p95 ms | p99 ms | Worst ms |
| --- | ---: | ---: | ---: | ---: |
| One parked vehicle | 0.0430 | 0.0474 | 0.0534 | 0.1319 |
| One driven vehicle | 0.2123 | 0.2191 | 0.2316 | 0.4109 |
| 25 parked vehicles / 100 wheels | 0.3813 | 0.3928 | 0.4072 | 0.8159 |
| Six driven vehicles / rough strips | 1.4247 | 1.4809 | 1.4987 | 2.0972 |

All 25 parked chassis sleep while retaining 100 contacts; all 24 rough-course wheels are supported at the final sample. Contact preparation dominates active-vehicle work. Fifteen finite-width probes and four coupled intervals intentionally cost more than one sphere/one-pass V1. This is a correctness improvement, not a claimed blanket speedup. Rigid-physics profiler scopes include nested vehicle callbacks; do not add them to wheel scopes or subtract their percentiles.

The investigation's original direct wheel/rigid phase timings remain historical baseline evidence, not matched end-to-end production timings. Further matched comparison and rendered measurements must be labelled by their actual scope.

## Qualification completion

Coordinated engine gates, exact-source SDK/export and final CI: pending. No pending result is counted as passing. Raw local evidence is retained in `.cache/vehicles-v2/`; investigation evidence remains in `.cache/vehicles-v2-investigation-20261109/`.

## Human checklist

1. Open `projects/vehicle_v2_workshop/Vehicle_V2_Workshop.judasproj` in the matching editor. Play and drive all four selections; steer, brake, reverse and tow.
2. Inspect wheels in Play: contact, travel/load, slip/grip, rpm and diagnostic. Enable collision/contact debug overlays.
3. Drive over ramps/rough strips and onto the native terrain to the right; sculpt/excavate the patch in Edit and try the changed route in Play.
4. Save with K, quit, reopen and load with L; verify the chosen rig and trailer pose. R restarts; Esc pauses.
5. In a fresh project create car/truck/bus/trailer, inspect/edit components and driver, Undo/Redo, make a prefab, save/reopen and export.
6. Judge steering, suspension, load transfer, collision and camera feel. Automation does not make those human acceptance decisions.

Current modelling limits and deferred capabilities are explicit in [Vehicles V2](VEHICLES_V2.md). Windows/Android/browser native execution is not yet qualified for this change.
