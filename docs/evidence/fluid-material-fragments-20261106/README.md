# Material-fragment evidence

Source parent: `c9f39d46959ce800d06b9f628623843cc808da8d`, Experimental.
`SOURCE_SHA256SUMS.txt` identifies the exact final prototype, tests, scripts and
original fixture. See [the engineering report](../../FLUID_MATERIAL_FRAGMENTS.md).
No production backend, Alpha unlock or main promotion is part of this checkpoint.

## Reproduce

From the repository root with the existing Linux C++/GLM prerequisites:

```sh
bash tools/prototypes/fluid_volume_grid/qualify_water_fragments.sh /tmp/judas-material-fragments
```

**Expected overall exit: 1.** The 34 control suites pass; the new 240-interval
fragment flow fails at interval two; both complete authored-basin intervals reject.
Every failed gate retains its actual nonzero exit, raw log and process resource
receipt. Rejected unpublished state fields are not measured zero physical error.

```sh
JUDAS_FLUID_SANITIZE=1 bash tools/prototypes/fluid_volume_grid/qualify_internal_transport.sh /tmp/judas-material-sanitized
```

Expected exit 0: **3,532 checks in 34 suites**, with address, undefined-behaviour
and leak checks. Leak detection was run natively outside the tracing wrapper.
The new integration failure remains separate from passing memory-safety checks.

Additional preserved controls:

```sh
mapfile -t objects < /tmp/judas-material-fragments/controls/objects.list
/tmp/judas-material-fragments/controls/CutMassMomentumTests --reference
g++ -std=c++17 -O3 -Wall -Wextra -Wpedantic -Werror -Itools/prototypes/fluid_volume_grid tools/prototypes/fluid_volume_grid/CutFastResidualOracle.cpp "${objects[@]}" -o /tmp/judas-material-oracle
python3 tools/prototypes/fluid_volume_grid/qualify_fast_residual.py /tmp/judas-material-oracle
g++ -std=c++17 -O3 -Wall -Wextra -Wpedantic -Werror -Itools/prototypes/fluid_volume_grid tools/prototypes/fluid_volume_grid/WaterVolumeFlowProbe.cpp "${objects[@]}" -o /tmp/judas-preserved-flow
/tmp/judas-preserved-flow
```

These pass 50 reference checks, 842 independent residual-oracle checks, and the
**older single-prism path's** 240 complete flow/energy intervals. The latter is
not a pass for the new fragment coordinator.

## Receipts and failed experiments

- `normal-controls/`, `sanitized-controls/`: exact raw control results. The two
  hydroacoustic logs use lossless `.log.gz` storage to preserve their original
  trailing spaces without introducing patch-whitespace errors.
- `integration/`: final unmodified physical gates, including nonzero exits.
- `measured/`: serial, trace-disabled failed-interval costs after compilation.
  One sample per orientation; no median/p95 or speedup claim is made.
- `preserved-flow.log`, `reference-mode.log`, `oracle.log`: independent controls.
- `prior-attempts.tar.gz`: raw exploratory logs and source variants, including
  rejected discretization/numerical strategies, event-growth trace, and the
  inconclusive 100-digit diagnostic. Some early probes exited zero even when their text reported a rejection; only
  the final gate exit files are acceptance results. Some early variants predate private type
  changes; the archive is not a claim that every variant builds against today's
  headers. Their original binaries remain in the local cache and have separate
  hashes. The final gate scripts above are the reproducible supported entry point.
- `verification.json`: exact outcomes, host/compiler, source parent, platform
  boundary, work/phase costs and caveats.

Exploratory timing rows are shared-host diagnostics; compiler jobs overlapped
some of those runs. Final serial failed-interval receipts are separate. Neither
establishes real-time performance, native MK2 fluid coupling, moving solids,
radial gravity, cold runtime saves, streaming, rendering, export or portability.

Independent fluid research directories were not modified. No historical evidence
is rewritten; the older checkpoint receives only a forward link.
