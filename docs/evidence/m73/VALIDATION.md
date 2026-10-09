# Reproducing the focused M73 checks

Release build directory used: `.cache/m72-release` (existing configured Release).
Affected targets: `judas`, `judas_editor`, `judas_export`, `judas_signal_tests`,
`judas_signal_application_tests`, `judas_script_tests`, `judas_runtime_ui_tests`.
The tests use actual QuickJS; application tests use shipping Application/InteractivePlay.

```sh
cmake --build .cache/m72-release --target judas judas_editor judas_export judas_signal_tests judas_signal_application_tests --parallel 2
.cache/m72-release/judas_signal_tests
.cache/m72-release/judas_script_tests
.cache/m72-release/judas_runtime_ui_tests
.cache/m72-release/judas_streaming_revisit_tests
node scripts/check_judasjs_api.mjs --typescript /home/conner/Documents/GitHub/Project-Judas/.cache/m73/typescript/node_modules/typescript
```

Actual application tests were run with desktop OpenGL against an isolated copy of
Signals Lab; the `load` invocation ran in a separate process after successful save.
Use `judas_signal_application_tests <absolute-project-path>` and then the same
command with trailing `load`. Editor normal Play/Stop used existing
`JUDAS_EDITOR_AUTOTEST` and `JUDAS_EDITOR_AUTOTEST_AUTHORING=1` instrumentation.
The existing test harness generated the screenshots and moved-runtime smoke.
No full production suite was run.

Exporter: `.cache/m72-release/judas_export` with project, destination, and final
runtime binary arguments. Moved package: `/tmp/judas-m73-signals-moved`; unrelated
working directory `/tmp`. A test-only application executable was temporarily copied
into the package for package-relative testing and removed after the checks.

Windows uses the existing Windows 10 KVM VM, VS2022 x64 Release toolchain, explicit
CMake regeneration for the new application target, and native executables. GUI tests
run as Conner in the interactive desktop session, non-administrator. The VM requires
its existing Mesa llvmpipe OpenGL fallback; its two app-local DLLs are test environment
additions, not the stock export contract. Hardware/input acceptance is outstanding.
Windows transfer scripts and final result receipts are preserved in `windows/`.

Final source hashes cover candidate files excluding evidence itself. Package hashes
exclude generated Saves. Historical failures retain the source/timing scope described
in REPORT.md; they are not claimed as final candidate results.
