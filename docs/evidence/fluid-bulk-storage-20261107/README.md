# Bounded storage qualification

Source parent: `cc6c2bbb` on Experimental; geometry foundation: `eed2d0e3`.
The bounded-fill accelerator is an independently committed change, with its
original zero-fill entry point preserved. The geometry foundation is separate
from this private pressure/storage prototype. Main and the native Alpha lock
remain unchanged. [Implementation and boundaries](../../FLUID_BULK_STORAGE.md).

From the repository root:

```sh
bash tools/prototypes/fluid_volume_grid/qualify_bulk_water.sh /tmp/judas-bulk-normal
JUDAS_FLUID_SANITIZE=1 JUDAS_FLUID_BULK_BASINS=0 bash tools/prototypes/fluid_volume_grid/qualify_bulk_water.sh /tmp/judas-bulk-sanitized
```

The complete basin gate is expected to be **nonzero** until the recorded failures
are fixed. `JUDAS_FLUID_BULK_BASINS=0` runs controls only; it explicitly records
that authored basins were skipped and is not a full-backend pass. Sanitizers
must run natively for leak checking, outside a process tracer.

The qualification compiles dependencies afresh, runs all existing transport and
pressure controls, and then builds the new modules and focused tests. It records
individual exits, an invalid-command negative control, and separate original
level/rotated logs. The default authored target is ten complete intervals per
orientation; the timeout is an explicit nonzero failure, not a pass.

Final raw logs, host/compiler settings, source hashes and exact outcomes are
listed in `verification.json`. Timing scope must be read with the results:
compiler-overlap exploratory runs are diagnostics, and small two-cell tests are
not representative game throughput. All new code is private prototype code.

The retained dependency suites were rebuilt in both configurations before the
last admission-helper change. The four new modules and both new test executables
were rebuilt afterward against those unchanged dependency objects; the final
focused control adds the dry-support rollback negative case. `normal/` and
`sanitized/` combine those matching-source results. Sanitizers ran natively with
leak checking, not under a process tracer. The complete control total is 3,631;
the basin results are separately red (level: 3/10, rotated: 0/10). No native engine
integration or playable-backend qualification is implied.

`admitted-basins/` contains the final, sequential, 90-second-bounded original
fixture runs. The level run returns 1 on the fourth interval; the rotated run
returns 124 on timeout. Trial sources and outputs from earlier variants remain
in the archive rather than being substituted for these final results.

`prior-attempts.tar.gz` preserves trial sources and logs. Some historical logs
label the linear-operator application count “CG”; the actual implementation uses
flexible GMRES. The final driver labels this counter `KrylovApplications`.
`rotated-strict-linear.INVALID.txt` identifies the compile-failed trial that linked
a prior object, and that trial is excluded from conclusions.

`TangentProbe.cpp` differentiates independently recomputed compensated material
rows. The earlier rounded-residual finite-difference version and its failures
are retained separately; subtraction of two large, rounded binary64 residuals
was not a reliable small-derivative oracle. None of those failures was silently
reclassified as an engine correction.

Reproduce the private derivative controls after the normal build above:

```sh
out=/tmp/judas-bulk-normal
source_dir=tools/prototypes/fluid_volume_grid
proof=docs/evidence/fluid-bulk-storage-20261107
mapfile -t objects < "$out/controls/objects.list"
objects+=("$out/WaterBulkStorage.o" "$out/WaterBulkReconcile.o" "$out/WaterBulkCondensation.o")
c++ -std=c++17 -O3 -Wall -Wextra -Wpedantic -Werror -I"$source_dir" "$proof/TangentProbe.cpp" "${objects[@]}" -o "$out/tangent"
"$out/tangent"
c++ -std=c++17 -O3 -Wall -Wextra -Wpedantic -Werror -I"$source_dir" "$proof/TangentProbeNegative.cpp" "${objects[@]}" -o "$out/tangent-negative"
"$out/tangent-negative" # Expected exit 1, eight failures from the deliberate perturbation.
```

The normal driver intentionally returns nonzero on the outstanding basin failures;
its compiled objects still exist for the separate derivative controls. The
reference operator is included by the diagnostic source itself, so do not also
link `WaterBulkProjection.o`. These controls check the current nonlinear tangent,
not moving-body dynamics.

Verify source inputs from the repository root with:

```sh
sha256sum -c docs/evidence/fluid-bulk-storage-20261107/source-sha256.txt
```

Verify the evidence bundle from this directory with `sha256sum -c SHA256SUMS`.
