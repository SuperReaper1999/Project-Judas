# M81 reproduction

Run from repository root. Linux Release is the automated acceptance platform.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j2 --target judas judas_editor judas_export \
  judas_wheel_tests judas_wheel_script_tests judas_wheel_lifecycle_tests \
  judas_wheel_application_tests judas_physics_tests judas_collision_tests \
  judas_physics_cast_tests judas_joint_tests judas_joint_authoring_tests \
  judas_kinematic_tests judas_character_motor_tests judas_physics_continuation_tests \
  judas_save_storage_tests judas_save_exclusion_tests judas_save_streaming_tests \
  judas_bending_tests judas_structural_tests judas_gravity_selection_tests \
  judas_uniform_gravity_scene_tests
python3 docs/evidence/m81/RUN_FINAL_CHECKS.py.txt
```

The sequential driver uses isolated writable user data, offscreen SDL and dummy audio. The exclusion test requires a **fresh** scratch directory; change its path on repeated runs. Cold write precedes cold read in a separate process. The actual restored commands/spin/travel bytes, tyre settings and resumed physical movement are checked. Do not combine preserved old engine archives with current changed C++ headers.

## Editor, normal export and moved package

```sh
cp -a projects/wheel_lab /tmp/judas-m81-editor-project
env SDL_VIDEODRIVER=offscreen SDL_AUDIODRIVER=dummy \
  XDG_DATA_HOME=/tmp/judas-m81-editor-data \
  JUDAS_EDITOR_AUTOTEST="$PWD/docs/evidence/m81/linux/editor" \
  build/judas_editor /tmp/judas-m81-editor-project/wheel_lab.judasproj
build/judas_export projects/wheel_lab/wheel_lab.judasproj \
  /tmp/judas-m81-wheel-lab-20261013 build/judas
```

The export uses normal project registration, assets and runtime. Move the package to an unrelated path (including spaces) and launch its `judas` with no project argument from `/tmp`. The existing generic `JUDAS_TEST_SCRIPT` harness can replay `export-smoke.txt`; adapt screenshot paths to your checkout. Packaged `game.judasproj` can also be passed to the application probe/cold write/read modes. Omit offscreen/dummy/harness variables for desktop play.

## API and source identity

```sh
node scripts/check_judasjs_api.mjs --typescript /tmp/judas-m80-typecheck/node_modules/typescript \
  --runtime /tmp/judas-m81-final-script/surface.json
/tmp/judas-m80-typecheck/node_modules/.bin/tsc --noEmit -p projects/wheel_lab/jsconfig.json
sha256sum -c docs/evidence/m81/FINAL_SOURCE_SHA256.txt
sha256sum -c docs/evidence/m81/BINARY_SHA256.txt
sha256sum -c docs/evidence/m81/EVIDENCE_SHA256.txt
```

TypeScript 5.9.3 is temporary developer tooling, not a Judas build/runtime dependency. Source/evidence hash paths are repository-relative; tested binary paths are local absolute paths and binaries/packages are excluded from Git. `CHANGED_FILES.txt` is the exact intended checkpoint inventory. Evidence manifests exclude themselves to avoid recursive hashes.

See `RESULTS.md` for final versus development results and `HUMAN_CHECKLIST.md` for controls.
