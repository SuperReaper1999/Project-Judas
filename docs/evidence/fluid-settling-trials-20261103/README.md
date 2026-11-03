# Reproduce the settling trials

See the [summary](../../FLUID_SETTLING_TRIALS_20261103.md) for the dispositions.
These are experiments against the baseline identified in `provenance.txt`.
The originals and the shipped implementation were preserved. Rejected patches
are archived for inspection.

Use a native Release GNU make build at `.cache/2d-stage1-build` with
`judas_production_fluid_tests` and `judas_player_fluid_integration_tests` built.
Use the current test objects and engine headers together. Several ad hoc links
with old test objects failed because their layout differed. Their results
are excluded. The baseline and validated comparisons use matching objects.

From the checkout root, copy the top-level Python scripts here into a fresh
ignored directory two levels down, for example
`.cache/fluid-settling-repro`. The original archived run used the existing
`.cache/input-devices-20261103/production-final/small_mass_125x.csv` for the
calm-water control's recorded-force comparison. If that file is absent, create
it with the unchanged native production fixture and the corresponding output
directory. The other scripts compile scratch copies and leave the test and
engine sources untouched.

```sh
python3 .cache/fluid-settling-repro/small-body-control.py
python3 .cache/fluid-settling-repro/small-body-extension.py
python3 .cache/fluid-settling-repro/pool-extension.py
python3 .cache/fluid-settling-repro/kernel-lattice-control.py
python3 .cache/fluid-settling-repro/rescaled-poly6.py
```

The original strict four-second or warmup comparisons are expected to fail on
the baseline. The rescaled pressure trial remains a rejected prototype.

The [pressure-kernel notes](pressure-kernels/README.md) describe the quartic
scratch build. The full local trajectory bundle and its digest are recorded
in `pressure-kernels/local-archive-sha256.txt`. The compact repository
snapshot preserves the clean patch, build and footprint controls, and full
production and player logs. Decompress the patch before use. To rebuild the
scratch variants, use `git show 2fccc514:src/FluidWorld.cpp` for `base.cpp`.
Keep the source and objects in the paths in the kernel notes.
The optional `bulk-pressure-trace.py` and `swim-timing-control.py` consume
the clean scratch object layout produced there. The trace columns after
`BULK` are substep number, then vertical momentum in N·s at substep start,
after gravity, after pressure, after smoothing and after contact, then the
number of submitted contact rows. The observation is passive. The swim
entry control changes the input trigger only in its separate scratch driver.

The [joint-support notes](joint-support/README.md) contain independent
reproduction instructions for the column and coupled pressure prototypes.
The compressed CSVs are the original column outputs. Their uncompressed
digests appear in that directory's `raw-sha256.txt`.
