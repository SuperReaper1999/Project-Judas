# Retiling and aperture qualification

Source parent: `3d03de09347d1c4fd2b06460f4048206e0d6d5b9`, Experimental.
The exact final inputs are in `SOURCE_SHA256SUMS.txt`; outcomes, compiler/host,
failed-gate disposition and benchmark samples are in `verification.json`.
[Engineering report](../../FLUID_MATERIAL_RETILING.md).

From the repository root:

```sh
bash tools/prototypes/fluid_volume_grid/qualify_water_fragments.sh /tmp/judas-retile-normal
JUDAS_FLUID_SANITIZE=1 bash tools/prototypes/fluid_volume_grid/qualify_internal_transport.sh /tmp/judas-retile-sanitized
```

Expected exits are **1 and 0**, respectively. The 36 controls pass 3,559 checks.
The full fragment-flow and both original authored-basin advances still reject;
their exact nonzero exits, logs and process receipts are retained. A local accepted
prefix does not mean that a complete interval was published. The sanitized failing
flow also exits one because its physical tests fail, with no sanitizer findings.

The old 240-interval flow, 50-check reference mode and 842-check independent residual
oracle remain separate controls, reproduced with the current objects. Use the
commands in the preceding material-fragment evidence README; substitute this
qualification's `controls/objects.list`. Their success is not a new-backend pass.

`prior-attempts.tar.gz` contains exploratory source and raw logs, including rejected
pressure-owner coarsening, event-accuracy, adjacent-time and endpoint variants.
The coarse trial failed three existing ownership controls. No fixture was weakened.
Early variants can depend on private types at that time; the archive is not a
claim that every intermediate variant builds with final headers. The final scripts
above are the supported reproduction entry point.

`regrid-cost.log` records all five samples at each input count; `RegridCost.cpp`
contains the exact driver. Compile it with the qualification object list, adding
`-Itools/prototypes/fluid_volume_grid`, C++17/O3/no fast-math. It includes the
reconstruction control fixture directly. This is a geometry cost measurement,
not real-time gameplay evidence or a before/after fluid speedup.

Raw logs with original trailing spaces are stored losslessly as `.log.gz`.
Historical evidence, independent research, main and the Alpha lock are untouched.
The operator explicitly approved the finite-resolution/subgrid model on 6 November
2026. This evidence qualifies the preceding corrections, not that future backend.
