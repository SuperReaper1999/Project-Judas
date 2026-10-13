# Reproduce the M83 structural crash follow-up

From the repository root, with its normal Release configuration/dependencies:

```sh
cmake --build build -j2 --target judas_vehicle_damage_application_tests judas_mechanical_damage_application_tests
```

The vehicle helper takes `SCENE MODE OUTPUT_DIRECTORY`; modes are `watch`, `rest`,
`write`, `read`, `lifecycle` (`probe` is diagnostic, not acceptance). Use separate
fresh output directories. Native GL requires a working desktop/display. Run
CPU-heavy native applications sequentially. Example:

```sh
SDL_AUDIODRIVER=dummy XDG_DATA_HOME=/tmp/judas-my-vehicle-check build/judas_vehicle_damage_application_tests projects/damage_lab/Scenes/vehicle_crash.judas write /tmp/judas-my-write-results
SDL_AUDIODRIVER=dummy XDG_DATA_HOME=/tmp/judas-my-vehicle-check build/judas_vehicle_damage_application_tests projects/damage_lab/Scenes/vehicle_crash.judas read /tmp/judas-my-read-results
```

`read` starts a fresh process and uses the preceding ordinary slot, checks actual
physical state before resumed simulation, then applies the same 20 N s impulse.
Use the same temporary data root for the pair. Do not overwrite original failure
evidence or mix the bearing and vehicle slot roots.

Retained helper takes `PROJECT MODE OUTPUT_DIRECTORY`: `write`, `read`,
`lifecycle`, `oblique`. It explicitly selects original `Scenes/flat.judas` rather
than the current project startup; rebuild it before use. The original fixture
inputs, material quality, thresholds and timestep remain.

Recreate ONLY new vehicle payloads on a disposable copied project:

```sh
python3 tools/CreateMechanicalDamageLab.py /tmp/my-copied-damage-project build/judas_deformable_bake --vehicle-only
```

The source recipe, explicit fracture `.source`, baked assets and prefab use normal
Judas mechanisms. `--vehicle-only` preserves the older project payloads.

Export normally:

```sh
build/judas_export projects/damage_lab/damage_lab.judasproj /tmp/my-fresh-damage-export build/judas
```

Copy/move the package, start its executable from an unrelated working directory.
`moved-package.harness` records the executed ordinary logical controls. Replace
capture paths for a fresh run and use `JUDAS_TEST_SCRIPT=ABSOLUTE_HARNESS_PATH`.
This harness proves routing/services, not human responsiveness; the native
application helper independently proves structural endpoints with actual drawing.

Editor smoke uses `JUDAS_EDITOR_AUTOTEST=ABSOLUTE_OUTPUT_PREFIX build/judas_editor
COPIED_PROJECT`; run only on a disposable copy because it exercises editing/save.

The exact original rejected save and independent read-only decoding procedure
are in `development/cold-analysis/REPORT.md`. Trial parameters/sources are in
`development/`; their failure logs remain unchanged. No protected research or
historical experiment is needed for these checks.
