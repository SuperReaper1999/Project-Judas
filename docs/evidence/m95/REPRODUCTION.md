# Reproducing M95

Start at the M95 candidate on Experimental, repository root, with the existing
Linux dependencies and an actual desktop OpenGL context. Windows/Android were not
run. Use fresh output paths; scripts deliberately retain failures rather than
silently replacing previous results. No full production-suite sweep is required.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target judas_editor judas judas_export judas_script_check judas_model_import_cli judas_animation_workflow_tests judas_animation_workflow_application_tests judas_animation_cold_save_tests judas_save_storage_tests judas_save_application_tests judas_save_streaming_tests judas_character_motor_tests judas_motor_runtime_tags_tests judas_skeletal_animation_tests judas_pose_composition_tests judas_physical_animation_tests judas_animation_retarget_cook_tests judas_model_reimport_tests judas_retarget_editor_tests judas_animation_application_tests judas_script_developer_tools_tests judasjs_examples_tests -j 3
python3 scripts/m95_validation.py --output .cache/m95/reproduction-validation
python3 scripts/m95_delivery.py --output .cache/m95/reproduction-delivery
python3 scripts/m95_cook_check.py --output .cache/m95/reproduction-cook
```

The API check uses the already available developer-only TypeScript installation
at `.cache/m90/types/node_modules/typescript`. Set the checker’s `--typescript`
argument to another standard local TypeScript installation if needed. TypeScript
is not a Judas runtime or game dependency.

`m95_validation.py --start-at <test-target>` supports a narrow continuation after
fixing a failed test/driver; new output is still required. The final evidence
selection records the combined closing candidate; earlier failures and follow-ups
remain under development/.

`m95_delivery.py` copies the project to its private output directory before editor
marker edits. It uses the ordinary editor import/Play-Stop autotest, then ordinary
export, byte-verifies a relocated package and launches it from an unrelated
working directory. The timeline check closes the Asset Browser before recooking.
The final readable screenshot is `delivery/timeline.png`.

## Native cold save, separately

```sh
SDL_AUDIODRIVER=dummy build/judas_animation_cold_save_tests write .cache/m95/reproduction-cold
SDL_AUDIODRIVER=dummy build/judas_animation_cold_save_tests read .cache/m95/reproduction-cold
```

These are distinct processes using normal checksum-protected save storage and
WorldPersistence. They compare the real endpoint and first resumed boundary.

## Determinism / invalid authoring

The cook checker makes two private copies, removes only those copies’ generated
models, cooks independently and compares SHA256. It then submits an invalid
marker time, requiring rejection and byte-preservation of the previous model.

## Accepted-source trim isolation

`development/trim-isolation/ModelCook.cpp` is the exact accepted M94 source,
identified by SHA256 and base commit in `IDENTITY.json`. Compile it with the
`judas_model_import` target flags, replace only `ModelCook.cpp.o` in a private copy
of `libjudas_model_import.a`, and relink the unchanged retarget cook test using its
normal link command. Do not replace the working build or accepted artifacts.
The preserved isolated result and original M95 result both fail the same exact
schedule comparison. Corrected retarget/reimport runs retain the original test
fixtures and tolerances. This isolates cooker changes, not an entire historical
engine binary.

## Source art regeneration

Normal testing does not regenerate source art. `scripts/m95_lab.py --overwrite`
explicitly resets the original lab’s source/project/scene/recipe content. It is
intended only for deliberate regeneration, not iteration on an edited lab. Cook
its recipe afterward using the ordinary model import CLI.
