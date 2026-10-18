# Reproduction

Use a separate checkout of parent `146a0727eab63d9555c732bf73e6093be5bc489d`
for BEFORE. Its engine source matches the recorded prebuilt baseline. Preserve
its failures; do not overwrite the final candidate. Existing Linux dependencies
and build setup apply; no new dependency is needed.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DJUDAS_ENABLE_VALIDATION=ON
cmake --build build --parallel 2 --target judas_impact_mesh_cost_tests
build/judas_impact_mesh_cost_tests 60 "" 1.2
```

Parent: 372 checks / four failures. Final: same 372 checks / zero failures.
Original source, launches, 90-step horizon, independent oracle and thresholds
are unchanged. The repeated-search-only ablation also fails four.

```sh
cmake --build build --parallel 2 --target judas_native_mesh_growth_tests judas_clip_polygon_tests judas_stream_origin_precision_tests
python3 scripts/prealpha_validate.py --build-dir build --output /tmp/judas-ccd-review --test native-mesh-growth --test clip-polygon --test stream-origin-precision --test impact-mesh-continuation
```

Use a new output directory. Gate provenance requires reconfiguration/rebuild after
source/checkpoint changes. Recorded pre-commit binaries embed their actual dirty
parent identity; per-file fingerprints bind their engine source to the candidate,
not an invented later commit identity. The same portable precision test linked
against the parent fails seven strict checks and against the correction passes.

The existing public application/cold-save test requires project, mode and output:

```sh
XDG_DATA_HOME=/tmp/judas-ccd-saves SDL_AUDIODRIVER=dummy build/judas_save_streaming_tests projects/streamed_range/streamed_range.judasproj write /tmp/judas-ccd-save-results
XDG_DATA_HOME=/tmp/judas-ccd-saves SDL_AUDIODRIVER=dummy build/judas_save_streaming_tests projects/streamed_range/streamed_range.judasproj read /tmp/judas-ccd-save-results
```

Research archives preserve exact sources, failed/positive controls, raw three-pair
timings, actual receipts, private-bound probes and the 8,192-row comparison.
Their raw commands retain original host paths. Isolated replay needs a rebuilt
parent archive and adapted root paths; compiled objects/executables are excluded.
The normal CMake gates above need no archived binaries.

The ordinary Physics Stress Lab package was exported, every relocated file hash
verified, its original export path made unavailable, and runtime launched for 60
fixed frames from unrelated /tmp. This is an application/export smoke, not a
visual curved-trough oracle. Windows/Android and fresh manual editor Play/Stop
were not claimed; editor/runtime Release builds and identity probes passed.

The research manifest maps 5,156 unchanged copied headers/sources to exact paths
and hashes at the parent commit instead of duplicating them. Restore those paths
from that checkout when replaying isolated variants; all modified files are
complete archived payloads. Every archived payload hash was verified.
