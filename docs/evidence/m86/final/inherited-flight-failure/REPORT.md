# Atmospheric-flight regression: baseline isolation

Baseline and current HEAD: `7d1110ee323e3f7d6a7b5d84be338edf144546dc`; branch `Experimental`.
This audit only read existing dependency receipts and source bytes. It did not build or run any native test.

- Existing failure: `manual torque builds a tumble that persists with SAS off in gas` (one assertion).
- 26 compiled-unit dependency receipts, including linked profiler and app-icon static libraries.
- 79 repository source/header dependencies; 79 match baseline SHA-256 exactly.
- Changed-source intersection: [].
- CMake isolation checks: {"global_configuration_prefix_unchanged": true, "no_judas_engine_or_weather_link": true, "physics_source_set_unchanged": true, "target_definition_unchanged": true}.
- Generated embedded icon matches exact generation from baseline template/PNG: True.
- Recorded failing binary SHA-256: `f9cb890f7e0df1e0ab50c5005d05832cc981edafadea993ae417a445219f74aa`.

The target directly compiles its existing flight, atmosphere, rigid-body and collision sources. It links only the existing profiler/app-icon libraries and SDL2, not `judas_engine` or any M86 weather/renderer/world implementation.

**Classification:** inherited/source-isolated failure; do not report this target as passing. This evidence rules out an M86 source/dependency change in the target, but does not replace an independently run baseline executable or diagnose the original test contract.

Exact dependency SHA-256 pairs and copied flags/link/receipt metadata are in `dependency-hashes.json` and `captured/`.

Reproduce this read-only comparison: `python3 /tmp/m86-atmospheric-baseline-isolation/audit.py`.
The existing failing test command was `build/judas_atmospheric_flight_tests`; this audit deliberately did not repeat it.
