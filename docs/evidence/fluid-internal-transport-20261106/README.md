# Internal-flux and phase-front evidence — 6 November 2026

**Overall result: BLOCKED. Neither original basin advances a complete interval.**
This is prototype evidence, not native MK2, SDK, gameplay or fluid-unlock
qualification. The working pieces and the failed alternatives are retained.

Source parent: `b0b2abc852ebd9544a6b55cd0341d4083367155a`, Experimental.
Main remains `1094d64515604fe9d03fdab479efc7f880b112a8`.
The containing commit and `SOURCE_SHA256SUMS.txt` identify the actual tested
source. Prior evidence is unchanged. Independent fluid research was not edited.

## Reproduction

From the repository root, with GCC, the existing GLM headers, Bash and GNU time:

```sh
bash docs/evidence/fluid-internal-transport-20261106/reproduce.sh /tmp/judas-internal-transport
bash tools/prototypes/fluid_volume_grid/qualify_moving_grid.sh /tmp/judas-retained-grid
sha256sum -c docs/evidence/fluid-internal-transport-20261106/SOURCE_SHA256SUMS.txt
```

The first command currently exits **1**, preserving the original basin failure.
Its three scoped control suites pass; `authored.log` and `coface.log` explain the
unresolved admission failures. Do not relabel that command as a successful fluid
qualification because the control suites are green.

## Passing controls

- `CutPhaseFrontsTests.log`: **50 checks**, zero failures.
- `CutConnectedOwnersTests.log`: **33 checks**, zero failures.
- `CutDynamicTransportTests.log`: **149 checks**, zero failures.
- `regressions/`: **26 retained suites / 1,030 checks**, all pass.
- `controls-sanitized.log`: **50 checks**, AddressSanitizer, UBSan and leak
  detection pass. Compile the core geometry/topology/graph/owner/flux/front/projection
  modules and the control driver with `-O1 -g -fsanitize=address,undefined
  -fno-omit-frame-pointer`; run with `ASAN_OPTIONS=detect_leaks=1`.
- The initial tracing-wrapper LeakSanitizer failure is retained separately.
  It was not called a pass; rerunning outside that wrapper completed leak
  detection. The address/undefined-only run is also retained.

The passing controls cover actual connected geometry, internal right-angle
flow, finite front arrival, outlet phase changes, both flow directions,
conservative quantities, independent ledgers, deterministic repeats, pressure
source rejection, gas-only ports, invalid input and transactional failure.

## Negative control

`CutPhaseFrontsFrozen.cpp` changes only the internal face phase to gas, leaving
normal boundary evaluation in place. Compile it instead of `CutPhaseFronts.cpp`
and run the same control driver. The mutant exits **1**, with **2 failed
controls**; conditional follow-up assertions mean 40 checks execute rather than
50. This distinguishes a real moving phase front from a frozen initial trace.
No failing test or original fixture was weakened.

## Original authored basins

`AuthoredTransportProbe.cpp` uses the same `BasinFixture.h`, source import,
geometry, connected-owner mapping and native tick as the prior reproduction.
It first records the old pressure/admission result, then feeds that **diagnostic,
unpublished** pressure proposal to the new transport. The body remains frozen.
This isolates transfer admission; it is not an EOS-coupled accepted timestep.

Both pressure proposals close but fail their old admission checks. Both new
transport attempts also fail, after 128 attempted subdivisions. The physical
pressure output and the transport output sentinels remain unchanged. Failure
strings include internal rejection counts and the first inadmissible fragment.
Nothing from those rejected proposals is published as physical state.

Observed transport rejection costs are roughly three seconds per basin on this
Ryzen 5 5600 host. These are exploratory failed-call timings, **not** completed
fluid-step performance or a before/after speedup claim. Compiler and hardware
metadata, process resource measurements and exact raw values are preserved.

## Minimal front-representation counterexample

`CofaceFrontDiagnostic.cpp` builds three genuine regions: a thin bottom region
and two upper regions. Two disjoint apertures share the bottom region's upper
plane. Water enters through one and material exits the other. The diagnostic
includes the prototype implementation to measure the private integrator
without changing its public API.

For `h=1e-8, 1e-10, 1e-12, 1e-14`, measured thin-region values are:

| Quantity | Result |
|---|---:|
| One Heun step | `h/2` |
| Two half steps | `h/4` |
| Thin-region error / h | `0.25` |
| Owner L1 error / h | approaches `0.5` |

The error therefore fails to converge with subdivision. The full call rejects
transactionally after 64 attempts. This isolates an inadequate single-plane
normal-selected front closure, independently of large pressure systems, basin
size, moving solids and compiler precision.

## Rejected experiments

`attempts/` retains exploratory source snapshots and raw logs for instantaneous
front evaluation, finite swept-prism evaluation, an outgoing phase-flux budget,
Euler versus Heun, endpoint canonicalization and pressure-topology freezing.
The source snapshots are **rejected variants**, not selected runtime code.
Some earlier exploratory logs predate the final source; they are not claimed as
reproducible against the final checksum manifest. The final authored/coface
results above are reproduced with the recorded final source.

No tested variant completes both basins. In particular the swept/budgeted
variants reach nonrepresentable time increments or repeat rejection without
meaningful advancement. They were not adopted as fixes. The accepted channel
primitive remains explicitly bounded and incomplete.

## Required disposition

See [the engineering report](../../FLUID_INTERNAL_TRANSPORT.md). Internal flux
recovery is implemented; the remaining blocker is unsplit intra-owner phase
transport with actual aperture/path ownership, followed by coherent pressure
restarts when a **whole owner** changes constraint state. Fragment gas birth
must not be mistaken for that event. Native body momentum, wrench/EOS closure,
moving geometry, original-basin gameplay and performance remain unqualified.

Advanced Fluids remain locked. No main promotion, tag, publication, SDK switch
or independent-research change was performed.
