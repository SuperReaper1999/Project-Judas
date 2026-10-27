# Fluid contact-stability follow-up evidence

Baseline: `0dab8e5db6e2ff1dc94283091247a097de610eb5`, unchanged Release
Ubuntu 24.04 build, fixed step 1/60 s. The reproduction reads the repository's
historical terrain scene/project without saving either. `FullLoopReproduction.cpp`
uses RuntimeWorld, GameSession and StepPlayedWorld, with no input or save overlay.
Its three cases are advanced-off, advanced-on and advanced-on with fluid-volume Y
raised 10 m. The original +3 m run is separately retained.

The Python helpers preserve the **actual executed commands and task layout**:
`.cache/m100/alpha-source`, `.cache/m100/alpha-linux`, and
`.cache/prealpha-elastic-linux/ubuntu24`. They require that original build and
source snapshot; they are not clean-checkout turnkey test targets. Restore the
baseline source with the source-archive/build workflow in `docs/RELEASING.md`, and
use the original repository evidence fixture for FluidHydrostaticsTests. The
source archive intentionally excludes protected historical evidence.

For the original task layout:

```sh
python3 .cache/m100/legacy_full_startup.py
python3 .cache/m100/fluid-bounded/prototype.py
python3 .cache/m100/fluid-bounded/full-prototype.py
```

`prototype_reproduction.py` compiles the unchanged hydrostatic test twice, first
with the scratch `LocalFootprint.cpp` linked ahead of the original static engine,
then with the original engine alone. This is a numerical comparison, not a merged
fix. `prototype_full_reproduction.py` applies that same rejected object to the
full-loop diagnostic. `rejected-LocalFootprint.cpp` preserves the exact prototype.

The matching captured-pool fixture is:
`docs/evidence/stabilization/ftft9/body-development/resume-neutral/neutral_020_release_particles.csv`.
The baseline passes 93 checks; the prototype fails 8. No threshold was changed.
Raw output and original failure details are retained beside this file.

Current-policy validation uses ordinary CMake targets:

```sh
cmake -S . -B BUILD -DCMAKE_BUILD_TYPE=Release -DJUDAS_BUILD_CHANNEL=Alpha -DJUDAS_ENGINE_VERSION=
cmake --build BUILD --parallel 2 --target judas judas_editor judas_export judas_project_water_tests judas_project_settings_editor_tests judas_project_fluid_policy_tests judas_player_swimming_tests judas_basic_water_application_tests
BUILD/judas_project_water_tests
BUILD/judas_project_settings_editor_tests
BUILD/judas_project_fluid_policy_tests
BUILD/judas_player_swimming_tests
BUILD/judas_basic_water_application_tests --profile NEW_WATER_OUTPUT
BUILD/judas_basic_water_application_tests --cold-cycle NEW_COLD_OUTPUT
python3 tests/test_release_packaging.py
```

GUI/application tests need a working display and graphics driver. Repeat the
policy targets with `JUDAS_BUILD_CHANNEL=Experimental` to exercise the retained
opt-in path. Do not use Alpha's deliberate runtime rejection as a numerical pass.
The release qualifier records advanced-runtime exclusions as `not-exercised`.

The follow-up is not a fresh Windows or Ubuntu-baseline SDK qualification.
Earlier sealed SDKs lack the guard and are superseded. No publication or main
promotion follows from these tests.
