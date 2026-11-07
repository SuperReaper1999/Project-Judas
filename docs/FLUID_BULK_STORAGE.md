# Bounded fluid storage and pressure prototype

This is a **private numerical prototype**, not a completed Advanced Fluids V2
runtime. It does not change native gameplay, Basic Water, the Alpha fluid lock,
main, the SDK or independent research. The original sustained-basin gate remains
red. No human acceptance or runtime readiness is claimed.

The operator approved conservative subgrid storage on 6 November 2026, with
below-resolution geometry allowed to be approximate and believable game physics
as the objective. [The preceding exact-retiling checkpoint](FLUID_MATERIAL_RETILING.md)
showed that preserving every interface fragment did not bound practical work.

[Commands, exact source hashes and raw results](evidence/fluid-bulk-storage-20261107/README.md).

## Implemented scope

The new `WaterBulkStorage` retains independently owned water volume, air volume
and air mass in a fixed containing-cell array. Tiny quantities are retained
instead of being reconstructed by subtracting two binary64 capacities. Cell and
face counts do not grow with interface history. The immutable source chart still
contains its native solid cuts; this checkpoint does not pretend that those cuts
are a finished, simplified runtime control mesh.

`WaterBulkProjection` couples water transfers, hydraulic inertia, gravitational
storage, isothermal gas storage and finite pressure-driven air exchange. It is a
frozen-solid, frozen-inertia pressure/quantity stage. It is **not yet a compatible
advective momentum integrator**. In particular, a wet face becoming dry can make
an earlier momentum state unrepresentable; that condition rejects rather than
silently deleting the momentum.

For a containing-cell capacity C, water W, air G and air mass M, the reduced
storage model uses W + G = C and P G = R T M. The chart-gravity-aligned water
potential is a W + k W² / 2. Water head is a + k W + P - P₀. Actual measured wet
face support supplies the liquid hydraulic response; air has storage and finite
resistance, without an inertial gas-wave solver. Air pressure differences drive
paired mass transfers. The beginning opening is measured by the existing
rectilinear geometry compiler; its reduced within-interval conductance shrinks
with the donor's remaining dry support. Incoming atmospheric gas is supplied by
an explicitly accounted exterior reservoir. This support law is a declared
low-order approximation, not a claim of exact moving-interface aperture geometry.

The nonlinear solve switches its local coordinates between pressure in bulk air
and positive relative mass for vanishing air layers. Coordinates change without
changing the physical state or the final admission equations. Hysteresis applies
only to this numerical coordinate choice. It is not a physical phase threshold.
Matrix-vector products use binary64; quantity ledgers, accumulated corrections,
local roots and sensitive scalar coordinate conversions use compensated pairs
of binary64 values. No runtime multiprecision library, exact fractions or
113-bit pressure solve is added.

`WaterBulkReconcile` reconciles only solver-sized component ledger defects.
It works within actual material connectivity, preserves positive phases and
refuses changes beyond the existing absolute and relative material budgets.
Published quantities are then checked against the original local transfer
rows and an independently recomputed model-energy gate: kinetic energy,
gravitational potential and isothermal gas free energy relative to the exterior
reservoir. This is not a qualification of a full thermodynamic gas model.
Those publication checks determine convergence: a candidate inside the local
residual tolerance must also pass the independent component ledgers and energy
audit. A remaining solver-sized ledger defect requests further refinement;
unsupported momentum or energy failure rejects. The implementation no longer
demands extra internal digits after all physical admission checks have passed.
It cannot turn a real physical failure into a pass. There is no arbitrary damping
or energy clamp.

`WaterBulkCondensation` eliminates selected linear gas-storage rows algebraically
inside the accelerator, reconstructing every eliminated correction afterward.
It also removes accelerator-only dummy rows. Narrow connections and finite
storage remain in the Schur complement. The external Krylov solve still checks
all original rows. Bounded degree and fill-work limits affect acceleration, not
physical ownership. The existing pressure factor retains its original zero-fill
entry point; a separately requested one-level fill mode improves the reduced
accelerator. Failed factor replacement remains transactional.

## Qualification and outstanding failures

Normal and native address/undefined-behaviour/leak-checked controls each pass
**3,631 checks in 38 suites**: 3,567 retained dependency checks, 50 new storage and
pressure checks, and 14 new condensation checks. The original zero-fill path also
produces the same 241 physical output rows as the preceding checkpoint after
elapsed-time fields are removed. An independent derivative probe passes 128
checks; a deliberately perturbed derivative still fails eight checks. These
control results do **not** qualify the original basins.

Small sustained-flow tests cover all three negative chart-gravity axes,
independent water/air ledgers, adjacent-interval energy, deterministic reruns and
constant storage counts. Known-answer gas controls cover ordinary and narrow
rectangular openings. An isolated sub-coordinate-ULP air pocket and a tiny water
quantity remain owned. Invalid inputs and failed solves leave published state
unchanged. Negative controls preserve rejection of real conservation defects.

The original authored fixtures retain their rounded native geometry, carved body,
initial material and authored binary32 tick/gravity values. Successful prefixes
are recorded separately from the requested sustained interval count. A failed
interval does not publish. Historical exploratory logs using a double 1/60 tick
are diagnostic trials, not the final matched authored-fixture qualification.

**Remaining blockers:** sustained original-basin advancement, practical full-step
cost, compatible moving-water momentum transport, moving MK2 coupling and a
completed resolution-aware subgrid pressure/storage interface. Fixed arrays alone
do not make all of the inherited microscopic pressure constraints inexpensive.
The prototype still rejects the failing cases; no previous negative control or
physical tolerance was weakened to make the gate green.

Final isolated O3 original-fixture results on the Ryzen 5 5600:

| Fixture | Requested / accepted intervals | Result | Measured cost |
| --- | --- | --- | --- |
| Level | 10 / 3 | Fourth interval rejects invalid face momentum | Accepted intervals: 32.086, 11.450, 10.877 seconds; rejecting interval: 14.054 seconds |
| Rotated | 10 / 0 | 90-second timeout during first nonlinear solve | No completed interval or per-step throughput claim |

The level probe peaked at 326,992 KiB RSS; the rotated probe at 468,044 KiB.
Compilation did not overlap these final probes. These are small diagnostic
samples, not a matched full-step speedup claim. Two-cell 240-interval controls
take approximately 0.029 ms per interval, which plainly does not predict the
authored basin's cost.

The level fixture has 24,532 containing cells and 67,890 faces. The rotated
fixture has 34,588 cells and 96,326 faces. Minimum capacities remain approximately
1.12e-12 and 2.24e-12 m³. Fixed storage bounds history growth but retains these
microscopic pressure/storage constraints. Even after eliminating about half the
rotated accelerator rows, its actual residual stagnates around 0.006–0.007 rather
than satisfying the unchanged 1e-10 local gate. The next pressure representation
must put unresolved quantities into real subgrid storage with a coarser pressure
ownership model; the current containing-cell array alone does not do that.

The level momentum failure is separate: a source face rate survives an interval
whose water has left that face. Re-measuring wet support gives zero new hydraulic
inertance, but this pressure-only stage has no compatible dual-mass transport to
relocate the old momentum. Publication rejects. Resetting the rate, retaining an
arbitrary wet area or damping the state would hide this missing coupling; none
is adopted. A focused retreating-support negative control preserves rejection
and unchanged source/output state until a real momentum receiver is implemented.

No native scene save/load, streaming, live radial-gravity field, renderer,
standalone export, Windows or Android qualification is claimed for this backend.
The three axis tests are coordinate permutations of uniform gravity, not a
radial-field test. “Cold” in private probe output means independent recomputation
from published arrays, **not** a native cold-process save/load test.

## Preserved alternatives

The evidence archive retains unsuccessful coordinate, ownership, pressure,
aperture, timing and factorization trials. It also retains two invalid-oracle
cases: finite differences of already rounded large residuals, and a compile
failure that prevented a proposed strict-linear trial from being rebuilt. Their
outputs are labelled and are not promoted to engine-defect or performance claims.

The naive broad-air grouping probe is not a qualified pocket compiler. Its raw
“trapped” count means only “not in a directly broad-connected atmospheric group.”
Many such groups still have finite connections. Per-integration-face width can
also mistake a partition seam for a physical throat; those counts must not be
used to declare thousands of sealed pockets or to remove finite connections.
