# Fast transport evidence

Parent: `574fa22f5a6d93205c69db24da593628d41797e0`, Experimental.
No native engine, main, release or fluid-lock modification.

## Reproduce the matched first-interval comparison

From the repository root, with the existing C++/GLM prerequisites, Bash, GNU
time, tar and Python 3:

```sh
bash tools/prototypes/fluid_volume_grid/qualify_fast_pressure.sh /tmp/judas-fast-pressure 3
```

The script verifies and independently builds the archived slow reference,
runs existing and new focused controls, runs the exact rational audit oracle,
and alternates reference/candidate original-basin runs. It records raw logs,
exit statuses, resource use, complete field dumps, determinism and comparisons.
The prototype timestep, geometry and physical acceptance conditions are shared.
A 180-second process timeout is an experiment budget, not a quality setting.

The archive contains the exact original prototype sources and source checksum
manifest. Reference headers must come only from its extraction. Copying the
common benchmark driver away from current headers is intentional: quoted C++
includes search the source directory before `-I`. A failed initial build that
mixed headers was corrected before timing; no physical result came from it.

## Reproduce the known non-green sustained case

After the preceding build:

```sh
timeout 180s /tmp/judas-fast-pressure/continuation-basins --fast
```

Expected at this checkpoint: level basin admits four intervals; rotated admits
three then rejects the fourth. Exit status **1** is a retained failure, not a
passing sustained-physics gate. The physical timestep is unchanged and no
failed result is published. This must be corrected before runtime integration.

Sanitizers (run outside a tracing wrapper for LeakSanitizer):

```sh
ASAN_OPTIONS=detect_leaks=1 JUDAS_FLUID_SANITIZE=1 \
 bash tools/prototypes/fluid_volume_grid/qualify_internal_transport.sh /tmp/judas-fast-sanitized
```

`qualified-*` and `sanitized-*` logs contain 3,135 checks each; `reference-controls-*`
contains the original 299. The exact rational oracle contributes 842 additional
checks. The same old negative controls remain active.

## Evidence scope

- `comparison.json`, raw matched logs/resources and dump hashes record the
  final-source first-interval comparison. Three samples per fixture/backend;
  p95 is the maximum at this sample size. No broad worst-case claim.
- `continuation-final.*` records the known original-basin sustained failure.
- `continuation-reference.*` and `ContinuationReferenceProbe.cpp` retain the
  slow replay from the same third-step state. Its combined 180-second budget
  expired during a stagnating fourth-step retry (exit 124), not a completed
  slow-path failure or success. These timings are diagnostic only.
- Compressed first-sample field dumps preserve complete comparison states;
  `DUMP_SHA256SUMS.txt` also lists identical repeated dumps left in the cache.
- Profile logs include nested costs, so overlapping phases must not be added.
- Rejected trial data and source snapshots preserve the intermediate failures.
  Diagnostic captures can predate final interfaces. They are historical
  experiments, not extra successful physical intervals or a claim that each
  snapshot compiles against final headers.
- No platform, moving-body, native runtime, cold-save, streaming or visual
  qualification is inferred from these numerical fixtures.

[Report and limitations](../../FLUID_FAST_TRANSPORT.md).
