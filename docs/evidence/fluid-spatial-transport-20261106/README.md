# Spatial conservative transport evidence

**Original basin pressure/transport qualification remains failed.**
Source parent: `9a651f31a9857ccc24372b96104cf383b203e6ea`, Experimental.
Use the containing commit and source hashes for the selected implementation.

From repository root:

```sh
bash docs/evidence/fluid-spatial-transport-20261106/reproduce.sh /tmp/judas-spatial
sha256sum -c docs/evidence/fluid-spatial-transport-20261106/SOURCE_SHA256SUMS.txt
```

The reproduction currently exits 1. Its four scoped suites pass (22 new spatial
controls, 50 phase/front, 33 connected-owner, 149 dynamic controls). The original
basin source, coarse pressure admission failure, spatial build, transport
rejection and seven individual timing samples are printed in the raw log.
This script does not turn a failed basin qualification into a green result.

The coface reproduction and its independent analytic chain are in the actual
selected test source, `CutSpatialTransportTests.cpp`. Its normalized timestep
error converges; gas mass is independent and nonnegative; paired physical
quantity ledgers agree with the spatial fields. The field is retained between
half steps instead of being reconstructed as an instantly mixed owner plane.

Both original frozen-body authored basins build (150,360 and 264,800 nodes).
Neither has a qualified coupled physical timestep. The measured costs are
rejected fixed-field solve costs, not game framerates or a claimed speedup.
Seven samples give nearest-rank p95 equal to the worst sample; the separate
SPATIAL_TRANSPORT call is warmup; COST samples 0 through 6 are the seven samples. Whole-process RSS
includes both fixtures sequentially and diagnostic arrays, not just the solver.

`sanitized.log` records 22 passing address/undefined/leak controls. Sources
were compiled with `-O1 -g -fsanitize=address,undefined
-fno-omit-frame-pointer`, and run with `ASAN_OPTIONS=detect_leaks=1` outside the
tracing wrapper that prevents LeakSanitizer from operating.

`attempts/` contains rejected exploratory coupling logs and a source archive.
The archive includes coordinator drivers, source snapshots and the experimental
callback pressure implementation. They are not adopted code. The fine-pressure
attempt fails its original row-balance gate on both basins; constant-owner and
local-correction variants fail complete coupled closure. The material-weighted
local correction run was stopped by its declared 90-second timeout (exit 124).
Earlier complement-gas results are not qualified physical passes, even where
a diagnostic printed `ok=1`; the admitted checks were incomplete and that gas
formulation was replaced by independent mass transport. Exploratory variants
predate the final selected code and are not asserted to reproduce identically
against its final source manifest.

The selected pressure coordinator is unchanged from the source parent.
`CutOwnedFlux` gained prescribed spatial divergence with finite/balance audits,
and tighter internal pressure tolerance before disaggregation. No final
physical audit was weakened. Native engine/runtime, main, SDK selection,
independent research and the fluid lock remain untouched.

[Engineering report](../../FLUID_SPATIAL_TRANSPORT.md) describes the confirmed
pressure-coupling problem, limitations, and the required next architecture.
