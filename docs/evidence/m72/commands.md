# M72 reproduction commands and receipt scope

All build/test commands below use CWD
`/home/conner/Documents/GitHub/Project-Judas`, except the explicitly relocated
standalone launch. These are the final focused invocations and a consolidated
equivalent build target list, not a claim that every target was built in one
invocation. The first fresh Release build and necessary incremental corrections
are retained in `development/`. Reproduction must use owned output directories;
do not overwrite accepted evidence or someone else's project.

## Release configuration and affected targets

```sh
cmake -S . -B .cache/m72-release -DCMAKE_BUILD_TYPE=Release
cmake --build .cache/m72-release --parallel 2 --target \
  judas judas_editor judas_export judas_scene_author \
  judas_runtime_render_tests judas_runtime_render_application_tests \
  judas_runtime_render_authoring_tests judas_runtime_render_retained_region_tests \
  judas_material_tests judas_material_application_tests judas_render_camera_tests \
  judas_shadow_tests judas_skeletal_animation_tests judas_model_runtime_tests \
  judas_scene_fingerprint_tests judas_resource_handoff_tests \
  judas_editor_widget_tests judas_editor_picking_tests judas_save_storage_tests \
  judas_export_tests judas_async_application_tests judasjs_examples_tests
```

Recorded compiler: `/usr/bin/c++`, GNU 15.2.0, Linux x86-64, Unix Makefiles,
Release flags `-O3 -DNDEBUG`. Final affected compiler receipts are warning-free;
dependency configure notices and original compiler/fixture failures remain visible.
The existing save-streaming target was built but not run; the new retained-region
proof and separate-process snapshot proof are the executed M72 lifecycle checks.

## Focused executions

The GL/application invocations use:

```sh
export SDL_VIDEODRIVER=offscreen
export LIBGL_ALWAYS_SOFTWARE=1
export SDL_AUDIODRIVER=dummy
export JUDAS_WORLD_STATE=none
```

```sh
.cache/m72-release/judas_runtime_render_tests .cache/m72/render-controls
.cache/m72-release/judas_runtime_render_application_tests .cache/m72/application
.cache/m72-release/judas_runtime_render_application_tests .cache/m72/application read
.cache/m72-release/judas_runtime_render_authoring_tests .cache/m72/authoring
.cache/m72-release/judas_runtime_render_retained_region_tests .cache/m72/retained-region
JUDAS_PROFILE=1 JUDAS_PROFILE_SCRIPT_ATTRIBUTION=1 \
  .cache/m72-release/judas_runtime_render_application_tests .cache/m72/live live

.cache/m72-release/judas_material_tests .cache/m72/materials \
  projects/material_lab/Assets/environment/studio.judasenv
.cache/m72-release/judas_material_application_tests .cache/m72/material-integration quick
.cache/m72-release/judas_render_camera_tests --output .cache/m72/cameras
.cache/m72-release/judas_shadow_tests
.cache/m72-release/judas_skeletal_animation_tests --output .cache/m72/skinning
.cache/m72-release/judas_model_runtime_tests .cache/m72/model-runtime
.cache/m72-release/judas_scene_fingerprint_tests
.cache/m72-release/judas_editor_widget_tests
.cache/m72-release/judas_editor_picking_tests .cache/m72/editor-picking
.cache/m72-release/judas_save_storage_tests .cache/m72/m61-storage
.cache/m72-release/judas_export_tests .cache/m72-release/judas
.cache/m72-release/judas_async_application_tests --output .cache/m72/async
.cache/m72-release/judasjs_examples_tests .cache/m72/examples materials
.cache/m72-release/judasjs_examples_tests .cache/m72/examples surface
```

Resource-handoff requires a **new, nonexistent owned output directory**:
`.cache/m72-release/judas_resource_handoff_tests .cache/m72/resource-handoff`.
That directory exists after the recorded run; choose a new equivalent on rerun.
Application `read` must follow its write invocation with the same output directory,
but starts a separate process. The ordinary `live` mode renders every frame;
the screenshot harness renders selected samples and is not an FPS benchmark.

`api-live.log` contains `EXAMPLE surface {"surface":[...]}`. The 321 enumerated
public symbols were compared with `symbols[].symbol` in
`docs/judasjs/api-inventory.json`; the 224 unique `call('...')` operations in the
registered library were compared with `nativeOperations`. The exact result is
`development/api-live-comparison.json`. This is **not** the TypeScript checker.
When an existing/authorized checker is available, the additional unexecuted check is:

```sh
node scripts/check_judasjs_api.mjs --typescript /absolute/path/to/typescript
```

## Ordinary editor and project harness

The editor operated on an owned recursive copy of `projects/render_control_lab`
at `.cache/m72/editor-fixture`, not the source project:

```sh
JUDAS_EDITOR_AUTOTEST=/home/conner/Documents/GitHub/Project-Judas/docs/evidence/m72/application/editor \
JUDAS_EDITOR_AUTOTEST_AUTHORING=1 XDG_DATA_HOME=/tmp/judas-m72-editor-data \
  .cache/m72-release/judas_editor .cache/m72/editor-fixture/render_control_lab.judasproj
```

The ordinary standalone source run used the source project and this harness:

```sh
JUDAS_TEST_SCRIPT=/home/conner/Documents/GitHub/Project-Judas/docs/evidence/m72/lab.harness \
JUDAS_PROFILE=1 JUDAS_PROFILE_SCRIPT_ATTRIBUTION=1 \
JUDAS_PROFILE_OUTPUT=/home/conner/Documents/GitHub/Project-Judas/docs/evidence/m72/application/lab-profile.json \
XDG_DATA_HOME=/tmp/judas-m72-lab-data \
  .cache/m72-release/judas projects/render_control_lab/render_control_lab.judasproj
```

Both supplied harnesses contain absolute evidence screenshot paths. For a new
reproduction, copy the harness and redirect those paths to new owned output;
do not replace the original receipts. `lab-startup-final.log` records the final
260-frame run and completed scene/save service waits.

## Export and actual relocation

An owned recursive copy of the lab was created at `.cache/m72/export-source`.

```sh
.cache/m72-release/judas_export \
  .cache/m72/export-source/render_control_lab.judasproj \
  /tmp/judas-m72-render-control-package-20261009 \
  .cache/m72-release/judas
```

After export, **only that owned source copy** was renamed to
`.cache/m72/export-source-hidden`. The package was renamed to
`/tmp/judas-m72-moved-render-lab-20261009`. Original project/assets remain intact.
The final package launch CWD was `/tmp/judas-m72-unrelated`, using the same
offscreen/software/dummy-audio variables above:

```sh
JUDAS_TEST_SCRIPT=/home/conner/Documents/GitHub/Project-Judas/docs/evidence/m72/moved-package.harness \
XDG_DATA_HOME=/tmp/judas-m72-package-data \
  /tmp/judas-m72-moved-render-lab-20261009/judas
```

`moved-package-final.log` names the relocated `game.judasproj`, records 260 frames,
all four captures and completed modern save/load waits. The first repository-CWD
run is retained separately and is not substituted for relocation proof.

Native Windows and desktop interaction/audio review are outstanding. No full
production-suite repeat or protected historical evidence edit occurred.
