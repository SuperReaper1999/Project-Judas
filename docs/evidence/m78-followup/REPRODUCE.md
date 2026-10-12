# Focused reproduction

All commands run at repository root unless specified. Pinned dependencies and original local imported rig fixtures are prerequisites; `docs/evidence/m78/LOCAL_ONLY_INPUTS.json` records local licensed assets, which are not redistributed. Reuse the existing frozen observer inputs and normal authored projects; do not regenerate an easier mesh or retune rigs.

```sh
cmake -S . -B .cache/m78-followup/release -DCMAKE_BUILD_TYPE=Release -DFETCHCONTENT_SOURCE_DIR_JUDAS_FREETYPE="$PWD/build/_deps/judas_freetype-src" -DFETCHCONTENT_SOURCE_DIR_JUDAS_HARFBUZZ="$PWD/build/_deps/judas_harfbuzz-src"
cmake --build .cache/m78-followup/release --parallel 3 --target judas judas_editor judas_export judas_navigation_bake judas_navigation_dependency_tests judas_rigid_motion_tests judas_collision_geometry_tests judas_prefab_tests judas_streaming_tests judas_scalability_tests judas_physical_balance_controller_tests
```

The recorded clean-build log, not this abbreviated target list, defines the executed affected build scope. Existing per-case JSON contains exact final test/probe arguments. `probes/compile_final_probe.py` compiles a named source against clean engine/library objects and refuses to overwrite receipts; use a new owned output prefix for a deliberate rerun. Final concave/free/inert sources are `concave_final.cpp`, `free_final.cpp`, `inert_final.cpp`. Their final-build receipts preserve exact flags and source/library hashes. Ordinary performance reproducers must run without simultaneous host/guest builds.

```sh
.cache/m78-followup/release/judas_physical_balance_controller_tests
.cache/m78-followup/release/judas_production_fluid_tests rotated_half
.cache/m78-followup/release/judas_navigation_bake .cache/m78-followup/dependency-final-project/landscape_fork.judasproj --check-dependents
```

Expect current unchanged stepping and rotated-half checks to fail; do not modify thresholds or call them passes. For navigation mutation tests, make a fresh owned copy of frozen `.cache/m78/mod-replacement-authoring-hidden` first; recorded `linux-navigation-negative-transactions.json` supplies exact invocation. Existing modified test fixture directories should not be reused as a clean baseline.

Windows: use the preserved source transfer manifests, MSVC/CMake/VCPKG installed dependencies and `probes/windows_candidate_build_retry.ps1`. `-DVCPKG_MANIFEST_INSTALL=OFF` prevents the unintended duplicate dependency download; no installed tools were removed. Native source check overlays the later portable geometry test, original project fixtures, spacecraft momentum contracts and 38-check navigation harness. Build/test recipes and failed attempts are retained under `native-final/`. Current guest disk headroom is too small for assuming another full build; inventory/estimate and reserve first. No VM enlargement or broad shared-cache deletion is authorized by this handoff.
