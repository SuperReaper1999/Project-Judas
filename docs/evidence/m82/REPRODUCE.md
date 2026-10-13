# M82 focused reproduction

Run from the repository root on native Linux. This is focused mechanical/lifecycle
validation; it does not request historical fluid/research or the complete
production-suite collection.

## Build and core contracts

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel 2 --target judas judas_editor judas_powertrain_tests judas_powertrain_lifecycle_tests judas_powertrain_script_tests judas_powertrain_application_tests
build/judas_powertrain_tests
build/judas_powertrain_lifecycle_tests
build/judas_powertrain_script_tests /tmp/judas-m82-script-repro
```

The pure shaft fixture covers independent inertia/torque expectations, source
curve interpolation, electric power limits, clutch saturation, exact gear and
open-differential relations, passive brakes/drag, rank redundancy, shared bearing
response, failure atomicity and durable continuation. Its printed performance
numbers are **core pre/post projection cost**, not whole application frame time.

The native lifecycle fixture supplies actual rigid/wheel endpoints, opposite
bearing reactions, arbitrary orientation, generation-safe removal/slot reuse,
serialization, prefabs, strict saves and private stream construction. The script
fixture executes the real virtual `judas` module, including the public cookbook
and live API enumeration.

The mechanical durable record carries internal rotor momentum, angular
presentation coordinates, initialization and held commands. Relative angular
velocity is an observation reconstructed from those coordinates and the actual
wheel/body/bearing state; it is not an independent competing save field.

## Ordinary application cases

Choose a fresh output and user-data directory. `write` and `read` must share the
same user-data location and run as separate processes. The application fixture
creates a hidden normal GL application and drives project-owned logical input.

```sh
XDG_DATA_HOME=/tmp/judas-m82-repro-data SDL_VIDEODRIVER=offscreen build/judas_powertrain_application_tests projects/powertrain_lab/powertrain_lab.judasproj contracts /tmp/judas-m82-repro/contracts
XDG_DATA_HOME=/tmp/judas-m82-repro-data SDL_VIDEODRIVER=offscreen build/judas_powertrain_application_tests projects/powertrain_lab/powertrain_lab.judasproj flat /tmp/judas-m82-repro/flat
XDG_DATA_HOME=/tmp/judas-m82-repro-data SDL_VIDEODRIVER=offscreen build/judas_powertrain_application_tests projects/powertrain_lab/powertrain_lab.judasproj oblique /tmp/judas-m82-repro/oblique
XDG_DATA_HOME=/tmp/judas-m82-repro-data SDL_VIDEODRIVER=offscreen build/judas_powertrain_application_tests projects/powertrain_lab/powertrain_lab.judasproj radial /tmp/judas-m82-repro/radial
XDG_DATA_HOME=/tmp/judas-m82-repro-data SDL_VIDEODRIVER=offscreen build/judas_powertrain_application_tests projects/powertrain_lab/powertrain_lab.judasproj lifecycle /tmp/judas-m82-repro/lifecycle
XDG_DATA_HOME=/tmp/judas-m82-repro-data SDL_VIDEODRIVER=offscreen build/judas_powertrain_application_tests projects/powertrain_lab/powertrain_lab.judasproj write /tmp/judas-m82-repro/write
XDG_DATA_HOME=/tmp/judas-m82-repro-data SDL_VIDEODRIVER=offscreen build/judas_powertrain_application_tests projects/powertrain_lab/powertrain_lab.judasproj read /tmp/judas-m82-repro/read
XDG_DATA_HOME=/tmp/judas-m82-repro-data SDL_VIDEODRIVER=offscreen build/judas_powertrain_application_tests projects/powertrain_lab/powertrain_lab.judasproj missing /tmp/judas-m82-repro/missing
XDG_DATA_HOME=/tmp/judas-m82-repro-data SDL_VIDEODRIVER=offscreen build/judas_powertrain_application_tests projects/powertrain_lab/powertrain_lab.judasproj phantom /tmp/judas-m82-repro/phantom
```

Use the ordinary desktop SDL driver if the local GL stack lacks offscreen
support. Offscreen frame-work measurements are not desktop FPS or physical GPU
acceptance. Keep CPU-intensive benchmark/build runs sequential.

### Deliberate independent controls

The free-rev interval opens the clutch **and applies ordinary brakes**. It proves
that a spinning engine does not drive those deliberately held wheel terminals;
it does not confuse already-existing vehicle coast with clutch transmission.
The load-response comparison observes the first engaged interval against the
immediately preceding free-rev state. It does not compare a later, still-
accelerating engine with an older RPM sample and call that a load failure.

The driving cases then release the brakes, engage the clutch and verify actual
chassis displacement, actual shaft ratios and differential kinematics. Separate
electric/machine phases test real physical endpoints. Cold-load observations
freeze the restored publication boundary before the first resumed physical step,
then verify subsequent physical endpoint response to new input.

The `lifecycle` case requests the actual registered `garage` region, captures a
deliberately changed durable crank spin and held command, unloads the region,
then requests it again. Exact durable bytes must match while runtime entity IDs
are remapped. A failed comparison emits paired readable state and small binary
records. The preserved first failure demonstrated a derived-observation cache
being treated as durable state; see [the diagnosis](RESULTS.md#streamed-durable-state-versus-derived-observation).

The `missing`/`phantom` modes use the valid write result as an independent control.
Missing removes only its mechanical participant. Phantom removes only authored
Powertrain components from the entity-scene prefix, preserves the remainder of
the exact entity archive, and retains the mechanical participant. They write
isolated malformed slots using SaveStorage, request normal asynchronous loads,
and require the exact participant-mismatch diagnostic while the original world,
scripts and actual physical endpoints remain published.

## Editable and exported content

```sh
build/judas_editor projects/powertrain_lab/powertrain_lab.judasproj
build/judas projects/powertrain_lab/powertrain_lab.judasproj
```

Export through the existing editor Project export path. Move the complete
package to a folder with spaces, launch its runtime from an unrelated working
directory, and exercise the same input/scene/save controls. No source-tree path
or hidden demo flag should be required. The accepted package location, exact
assets and executed receipts are recorded in [RESULTS.md](RESULTS.md).

The retained export smoke inputs are [export-smoke.txt](export-smoke.txt). They
run one full three-assembly watch tour, pause/resume, ordinary scene switches
and reload. Packaging was actually exercised at
`/tmp/judas-m82-powertrain-lab-20261013`, with a moved copy at
`/tmp/Judas M82 Moved Package` launched from `/tmp`; the temporary packages are
not repository source assets.

`tools/CreatePowertrainLab.py` regenerates the ordinary authored lab assets.
It does not modify existing wheel/game projects and does not generate or alter
the engine implementation or acceptance evidence.

The V watch tour covers the three assemblies using ordinary project input. The
deck is finite and the assemblies retain actual motion between tours; steer and
brake for prolonged driving rather than expecting an endless race course.
