# Pressure support and occupied volume

Baseline `103f4bf5ce913ee41d398bea9b4ba1a0334945fa`. Production Advanced
Fluids remain locked in Alpha. The original engine, checks, thresholds and
historical evidence are unchanged. This pass advances the diagnosis and
the isolated conservative-volume prototype. [Reproducers and raw evidence](evidence/fluid-pressure-occupancy-20261103/).

## Pressure and contact must agree before advection

In the stationary two-particle witness, the top parcel is already nearly
stationary when pressure reconstruction finishes. The bottom parcel still
has a downward velocity of 0.34348 m/s pending its floor response. The
smoother transfers 0.01648 m/s of that motion to the top parcel. The
subsequent wall impulse stops the bottom only. This accounts for the
reported speed at a fixed upper position. A contact-feasible smoothing
predictor removes that transfer in the column. It increases the 0.25 m
pool warmup to 0.10206 m/s and introduces an early immersion failure. A
pairwise tangent-only exchange retains mandatory checks in the two focused
pool runs but fails both original warmup diagnostics. [Smoothing phase trace](evidence/fluid-pressure-occupancy-20261103/joint/).

An isolated pre-advection solve projects gravity and existing smoothing
onto the joint density and wall constraints in mass-weighted velocity
coordinates. It publishes a shared pressure/support solution for prescribed
walls. The independent aligned eight-particle column has a feasible
nonnegative pressure reaction with zero velocity and a 0.4395974 N·s floor
impulse per step. At step 600, maximum speed falls from 0.05413 to
0.00000108 m/s and displacement from 32.70 mm to 0.0044 mm. Free-fall
controls retain zero support impulse. The intentionally unconfined oblique
column improves at first, then buckles laterally. This is neither long-run
oblique equilibrium nor a native moving-body qualification. [Column data](evidence/fluid-pressure-occupancy-20261103/column/).

| Prescribed-wall trial | Original half_025 warmup | Disposition |
| --- | ---: | --- |
| Legacy production | 0.08685 m/s | Unchanged strict failure |
| Joint pressure/contact projection | 0.06526 m/s | Late independent immersion fails |
| Same equations, eightfold diagnostic sweep limit | 0.06510 m/s | Warmup still fails |
| Active-set conjugate search at the original prototype limit | 0.08281 m/s | More irregular residual and failed warmup |
| Joint solve with newly discovered pressure-driven box faces | 0.03173 m/s | Early immersion and neutral-body drift fail |
| Reusing dual multipliers when the same solve adds faces | 0.01301 m/s | Warmup-only result. No full acceptance claimed |
| Continue the same solver with prescribed moving exterior boxes after release | 0.03146 m/s | Five mandatory half-density failures and one neutral drift failure |
| Reflected pressure samples with equal/opposite stationary-wall reaction | 0.37213 m/s | Mandatory rigid and player regressions |

Increasing the joint prototype's budget from 64 to 512 sweeps lowers
its final row residual from 0.02328 to 0.000715 m/s, with nearly unchanged
pool motion. In the last 90 substeps of the higher-budget run, the median
of the maximum density-ratio violation predicted by the linear solve is
0.00000556. The median of the maximum density-ratio change caused by the
post-solve wall geometry projection is 0.006197. This explains why the
solver needs to discover supports created by the pressure motion itself.
The projected normal and the verified finite face must refer to the same
motion. A lower algebraic residual alone cannot ensure this. [Linear-versus-geometry trace](evidence/fluid-pressure-occupancy-20261103/dual/).

## Density at particle centres does not determine occupied volume

Rediscovering pressure-generated wall faces takes the unchanged 2,500
particle player pool below its 0.05 m/s warmup gate (0.02137 m/s), yet its
neutral player sinks 0.604 m. First immersion falls from 1.000 to 0.410.
The pool retains all 20,000 kg. The player volume even contains more
particle-centre mass, 728 kg instead of 664 kg. Independent footprint
quadrature explains the discrepancy.

- In the affected player volume, 91 of 91 particles have a neighbour
  within 2 mm despite the authored 200 mm spacing. The median nearest
  neighbour distance falls from 198.1 mm to 0.0514 mm.
- Nominal particle cube coverage falls from 85.9% to 29.4%. Equal-volume
  spheres give the same result to within three percentage points. Swapping
  spawn positions retains the difference.
- The Poly6 density at those clustered centres is 999.866–1000.168 kg/m³.
  The exact density constraint is nearly satisfied while particles
  overlap and leave gaps in the represented water.

An analytic lattice of four coincident parcels per site also gives
exactly 1,000 kg/m³ at all particle centres, with just 25.1% footprint
coverage. Its zero-gravity minimum pressure correction is zero. This is
a counterexample to using a smaller point-density residual as a volume
certificate. Broadening the wet query to fill the gaps would conflate
this state with actual dry gaps. [Independent geometry and analytic control](evidence/fluid-pressure-occupancy-20261103/geometry/).

There is also a bulk volume deficit in both states. At 1,000 kg/m³ the
20 m³ of water cannot have a centre of mass below 2.0661 m inside a
2.2 × 2.2 m basin. Filling its lowest slabs attains that bound. The
observed centres are 1.6419 m baseline and 1.5227 m with rediscovery.
Reconciling those moments with an in-basin continuous field would
require peak density of at least 1,258 and 1,357 kg/m³, respectively.
Independent quadrature of the raw kernel finds 3,381 and 3,457 kg of
kernel tails outside the basin despite every particle centre staying
inside. Both diagnostics identify a mismatch between the solver's
point-density estimate and water volume bounded by the actual walls.
[Bulk moment and kernel replay](evidence/fluid-pressure-occupancy-20261103/dual/).

## Conservative-volume control at the same scale

The existing staggered-volume prototype was exercised in a disposable
11 × 30 × 11 grid representing the same 2.2 × 6 × 2.2 m tank and 20 m³.
The original prototype rejected step 68 of the exactly flat rest case.
Tiny gravity/pressure cancellation errors had transported a full cell
to a fraction of 1.0000000000010012. The stricter pressure admission
caught that value. The full failed run remains preserved.

The corrected scratch variant factors the exact hydrostatic pressure
gradient out of the equations when density is identical within each
layer and uniform gravity aligns with a grid axis. Its face pressure
difference is `g_axis * h * (rho_left + rho_right) / 2`. A perturbation
still goes through pressure projection. The existing general solve is
used when those equilibrium conditions do not hold. No clamp, density
bound change or extra damping is used.
The new scratch centre-occupancy observation uses a 1e-12 tolerance for
interpolation roundoff. All existing engine and prototype gates are retained.

- Flat case, 120 updates at 30 Hz. Exact zero velocity and divergence.
  Volume error below 6e-14 m³, centre of mass 2.0661157025 m, centre
  immersed. A divergence-free velocity perturbation is preserved exactly.
- Disturbed surface, 120 updates. It moves, peaks at 3.02 m/s and
  conserves volume to within 1e-13 m³.
- All 57 older isolated volume-grid gates pass with the same patch,
  including the original oblique-gravity pressure check.

These controls use a closed air ceiling, fixed axis-aligned walls and
no rigid actors. They do not qualify cut cells, moving solids, native
MK2 coupling, unrestricted pours or runtime cost at comparable fidelity.
The useful next implementation gate is a moving solid in a bounded
volume grid with explicit occupied capacity and one owner for fluid
pressure/reaction, tested against the original dry-gap and buoyancy
fixtures before any integration. [Exact scratch patch and runs](evidence/fluid-pressure-occupancy-20261103/grid/).
