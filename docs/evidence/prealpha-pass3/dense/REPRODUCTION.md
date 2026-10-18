# Reproduce the dense-workload comparison

All commands below run from the repository root. Use a fresh output directory for every run. Do not run CPU-heavy builds, other owned benchmarks or Windows validation simultaneously with a timing window.

## Current-source functional/performance run

Build the ordinary existing targets in Release:

```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DJUDAS_ENABLE_VALIDATION=ON -DJUDAS_BUILD_CHANNEL=Experimental
cmake --build build --target judas_physics_stress_live_benchmark judas_physics_scaling_diagnostics -j2
python3 docs/evidence/prealpha-pass3/dense/reproduction/run_workloads.py --live build/judas_physics_stress_live_benchmark --scaling build/judas_physics_scaling_diagnostics --output /tmp/judas-dense-new-run
```

The runner records executable and input fixture hashes, exact commands, timestamps, raw fixed-step CSV, M56 profiler JSON and endpoint quality. Its controls retain their original physical acceptance checks. `--only-dense` and `--skip-dense` permit serialized workload scheduling.

A runnable desktop graphics/audio environment is needed for the ordinary application benchmark. A hidden window is not a headless physics-only simulation. Do not use an unrelated driver/backend comparison to claim a physics speedup. Linux is the measured platform here; Windows is not certified by these results.

## Matched baseline and changed-object experiments

The baseline is exact accepted source `f647d7fd95f60d3a37dba70a51f4e45dc41c2922`; `FROZEN_BASELINE.json` identifies its clean native build and frozen binaries/archive. To rebuild it, use an isolated checkout of that commit with its own build/output directories. Preserve all fixture bytes and arguments.

Experimental source snapshots are retained under `reproduction/`. The three `Contacts.*.cpp` files distinguish the unchanged baseline, one-context dispatch, and selected-normal stage. The two ordinary `PhysicsWorld.*.cpp` snapshots distinguish the unchanged baseline and sorted support-membership lookup. The observer file is a separate diagnostic baseline, not the shipping implementation.

`BUILD_VARIANTS.json` preserves original compiler/link commands and source/archive/binary hashes. The original helper scripts are execution receipts using scratch paths; adapt those paths into a fresh owned directory when repeating them. Their old cached parent archive is not included as a build artifact. A full isolated source rebuild is an equivalent way to reproduce the actual code.

## Exact primitive contact-field comparison

From an isolated source checkout, compile the same observer with each retained source snapshot:

```
c++ -O3 -DNDEBUG -std=c++17 -Isrc docs/evidence/prealpha-pass3/dense/reproduction/PrimitiveWitnessProbe.cpp docs/evidence/prealpha-pass3/dense/reproduction/Contacts.baseline.cpp -o /tmp/judas-primitive-before
c++ -O3 -DNDEBUG -std=c++17 -Isrc docs/evidence/prealpha-pass3/dense/reproduction/PrimitiveWitnessProbe.cpp docs/evidence/prealpha-pass3/dense/reproduction/Contacts.selected-normal.cpp -o /tmp/judas-primitive-after
/tmp/judas-primitive-before /tmp/judas-contact-before.bin
/tmp/judas-primitive-after /tmp/judas-contact-after.bin
cmp /tmp/judas-contact-before.bin /tmp/judas-contact-after.bin
```

The complete explicit contact/diagnostic byte streams must match; padding is never serialized. The original stream SHA-256 is recorded in `PRIMITIVE_RESULTS.json`. The binary byte streams are reproducible output and are intentionally excluded from the evidence package.

## Verify portable evidence

`MANIFEST.json` records each stored receipt's size/hash and its original uncompressed size/hash. Decompress `.gz` files before inspecting CSV/JSON. No executables, libraries, object files, secrets, unrelated assets or build outputs are included.

This workstream does not close the dense-contact tail. Use the coordinator's final combined evidence for the final source/build identity and regression qualification.
