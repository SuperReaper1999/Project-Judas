# Reproduce this bounded developer-workflow pass

Use a Linux GL desktop, existing Release dependencies, and a fresh proof/data path.
Do not use the personal game-save directory. No Windows/Android result is implied.

```
cmake -S . -B build
cmake --build build --target judas judas_editor judas_export judas_script_check judas_script_tests judas_save_evolution_tests judas_editor_preferences_tests judas_workflow_application_tests -j1
./build/judas_script_tests
./build/judas_save_evolution_tests /tmp/new-workflow-evolution
./build/judas_editor_preferences_tests /tmp/new-workflow-preferences
node scripts/check_judasjs_api.mjs
./build/judas_script_check docs/judasjs/examples/developer_workflows/developer_workflows.judasproj
SDL_AUDIODRIVER=dummy XDG_DATA_HOME=/tmp/new-workflow-data ./build/judas_workflow_application_tests docs/judasjs/examples/developer_workflows/developer_workflows.judasproj write /tmp/new-workflow-proof
SDL_AUDIODRIVER=dummy XDG_DATA_HOME=/tmp/new-workflow-data ./build/judas_workflow_application_tests docs/judasjs/examples/developer_workflows/developer_workflows.judasproj read /tmp/new-workflow-proof
```

Negative controls: copy the complete lesson to two new temporary directories.
Disable script.0.enabled on the yard scene's crate, then run `missing-script`
with fresh data. It must still publish native collision but never report the
project crate registration. In the other copy add a comment to crate.js, retain
the project save identity and run `incompatible` against the writer's data.
It must reject the old content fingerprint and retain the current world. Hash
lesson.save before/after: neither successful reads nor failed loads rewrite it.

Export with the normal judas_export, move the complete package and launch from
an unrelated working directory. Final evidence records those actual paths and hashes.
Editor verification uses JUDAS_EDITOR_AUTOTEST and the optional
JUDAS_EDITOR_AUTOTEST_RUNTIME_INSPECTOR on an isolated project/preferences copy.
The existing hook observes/draws the tool; it is not a gameplay flag.
