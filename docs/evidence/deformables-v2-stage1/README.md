# Stage 1 evidence scope

Stage 1 remains open: the real-model severe-crash visual gate failed. Passing
mathematical or lifecycle assertions must not be cited as human visual acceptance.

`physical-controls.log` uses original procedural geometry only. `regressions.txt`
records the eleven focused executable results. `cold-process.txt` records a real
separate-process snapshot comparison. `pixel-parity.json` compares the same frontal
view, not general artistic quality. `performance.json` preserves differing workload
scopes and the invalid four-car GPU timestamp result explicitly.

The public evidence contains no downloaded geometry, textures, cooked models or
screenshots. External assets, archive hashes, complete local trial logs and visual
captures remain in the operator's private crash-proof directory. No model licence
or redistribution permission was inferred. The source in `tools/crash_proof` accepts
operator-supplied input; it is not a publicly bundled car game.

Rebuild and run the procedural checks:

```sh
cmake --build build --target judas_structural_tests judas_powertrain_lifecycle_tests -j 3
build/judas_structural_tests
build/judas_powertrain_lifecycle_tests
python3 tests/test_cmake_test_targets.py
```

For the actual local model launch, controls, scope and failed architectural gate,
see [the Stage 1 report](../../DEFORMABLES_V2_STAGE1.md).
