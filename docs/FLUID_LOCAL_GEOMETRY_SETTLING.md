# Local wet geometry and coarse-pool settling investigation

**Current checkpoint disposition:** [Experimental fluid progress](FLUID_PROGRESS_CHECKPOINT.md). The report below preserves the findings and uncommitted status of that earlier investigation; it is not the current unlock status.

2026-11-01. Experimental, based on `90a97547e7148f2ba179fb5d6a547d1e94a21188`.
**Uncommitted work. Main and the Alpha Advanced Fluid lock remain untouched.**
This continues the authorized [window/decay contract](FLUID_WINDOW_DECAY_CONTRACT.md).
It does not add damping, change coefficients or relax acceptance conditions.

## Confirmed observation defect

`ProductionFluidTests::ObservedSurface` takes a median height from remote exterior
columns and treats it as a horizontal plane across the body. Its exclusion uses
both axis-aligned strips around the initial half extents, not the current rotated
footprint. This remains a useful calm-reservoir/release control, but is not an
independent measurement of arbitrary late local wet geometry.

A manufactured plane through the center of a box bisects its volume exactly.
Using the unchanged observation predicate, a slope of -0.2 gives an inferred
flat-plane immersion of **0.828125**, against the analytic **0.5**. A flat positive
control gives 0.5. Slopes -0.4 and +0.4 produce inferred values 1 and 0 respectively,
although their true submerged volume remains 0.5. Independent dense volume
quadrature agrees with the analytic value within 0.0015 (grid boundary aliasing).
These are instantaneous geometry controls, not claims of static sloped water.

The real final snapshots likewise have nonplanar surfaces. Outside the *actual
rotated* coarse box footprint, the nearest half-spacing exterior band has median
height 1.5423 m, versus about 1.7188 m from the old remote estimate. This does not
establish an exact corrected immersion: extrapolation through a displaced-body
hole remains a modelling decision. It does establish why the flat-plane mismatch
cannot, by itself, prove the body's actual wet volume is wrong.

**All four existing late immersion assertions remain active and their failures
are retained.** No replacement oracle has been installed merely to obtain green
results. A valid replacement must handle local surface shape, rotated solid
geometry and unresolved particle gaps, and pass known-geometry negative controls.

## Confirmed reconstruction discontinuity and repair

The pending vertical-layer implementation had two ownership rules:

- Its continuous-column shortcut excludes a donor column if its *whole sample
  slab* overlaps the solid.
- Its layered path excludes donors separately in each height band.

Finding a resolved gap in any relevant column could switch the rule for every
other column. This is not merely physical water movement. In the preserved coarse
run, steps 651 and 652 use identical particle geometry; moving/rotating only the
body changes evaluated immersion from **0.4931953 to 0.2912455**. Cross-replaying
both body poses against both particle snapshots reproduces those exact values.

A smaller manufactured reproduction makes the mechanism unambiguous. A split
column sits at search reach 0.75 m. As the radius moves by +/-0.00001 m across
that boundary, its kernel weight tends to zero; it cannot physically change
ownership of the other columns. The old path jumps by **0.0469199** in wet fraction.
The repaired path's maximum change over the 32 fixture configurations is
**0.0000055433** and shrinks with the radius bracket. The regression compares
shrinking brackets using the float representation floor; it does not introduce
another physical settling threshold.

The repair requires displaced-solid queries to use the same height-dependent
ownership whether or not a gap was found. Non-displacing continuous queries keep
their established column-volume mean. Bulk motion donor exclusions remain as
before. This is a correction to the existing approximation, not a claim of exact
cut-cell geometry or conservative two-way fluid coupling.

## Field-owned frame and actual motion donors

The lattice origin already belongs to the input particle field, but the tangent
used by body sampling came from the receiving body's axes. Rotating that body
therefore also rebinned the water. An equivalent physical box authored as a
rotated parent or a rotated compound child could receive different loads.

`ReconstructionTangent` now chooses a well-conditioned projected edge from the
same deterministic particle reference used for the lattice origin. Body axes
remain a fallback for a degenerate cloud. The explicit `Query` tangent API is
unchanged; rigid hydrostatic evaluation requests the field's reference frame.
No timestep, force coefficient or numerical precision mode changed.

A field-frame-only prototype improved all four floating motion checks but failed
the existing recorded rotated-force covariance check. The cause was another
frame-dependent ownership decision: background-motion donors were excluded using
their rounded *column centers*. The repair tests their **actual horizontal
positions** against the full-query solid geometry instead. Pressure occupancy
continues to use the height-dependent rule; it does not borrow the solid's own
contact reaction through that narrower exclusion. Existing traversal order is
retained. The actual-position prototype restores all 124 hydrostatic checks.

New regression coverage includes 24 assertions comparing displaced volume, force
and torque for equivalent parent/compound-child representations, in addition to
the 32 vanishing-donor continuity controls. The original 261 vertical checks stay.

This remains a finite-resolution reconstruction. The field reference is input
order dependent, and choosing a different conditioned edge as geometry/gravity
changes is not a globally smooth spherical tangent atlas. It is not a new solver,
conservative pressure backend, free-surface oracle or fluid unlock.

Changed implementation: `src/FluidHydrostatics.cpp`, explanatory contract in
`src/FluidHydrostatics.h`; regression in `tests/FluidVerticalSupportTests.cpp`.
The earlier uncommitted geometry and contract changes remain present.

## Rejected or bounded experiments

1. Removing sampled water acceleration in a disposable coupling run does **not**
   eliminate late activity. Late peak speed remains 0.5368 m/s, and final angular
   speed remains about 7 rad/s. Removing that response would also compromise the
   intended moving-fluid/freefall model; this is not a candidate fix.
2. Slicing **every** query improves coarse motion but fails three established
   non-displacing column-mean tests. That version is rejected. Restricting the
   consistent ownership rule to displaced solids preserves those contracts.
3. A manufactured-flat fixture initially excluded a spacing-expanded solid,
   accidentally creating a real dry gap. Its presumed half-immersion expectation
   was invalid. Source/results are preserved as a rejected control. Corrected
   fixtures exclude only actual solid centers; both implementations then pass
   all 24 half-immersion orientation controls within the existing 0.10 tolerance.
4. Closed orientation-loop work on a frozen, nonplanar snapshot is diagnostic,
   not a hydrostatic conservation proof. The sampled acceleration need not be
   curl-free and the frozen displaced hole does not evolve with the body. Those
   results must not be represented as independent evidence of a conservative
   pressure field or as a standalone energy-defect verdict.

The original force-traced coarse run is physically identical to the previously
recorded 720-step candidate (all non-timing CSV columns). It reaches 3.14 rad/s at
8 s and 6.93 rad/s near 12 s. Relative point-drag work stays nonpositive. There is
still substantial rotational motion; existing single-point drag does not provide
full distributed viscous rotational resistance. No new angular damping was added.

## Final qualification

See the dated `FINAL_RESULTS.md` and machine-readable records in
[evidence/fluid-local-geometry-20261101](evidence/fluid-local-geometry-20261101/README.txt)
for final-source checks, preserved failures, negative control and fingerprints.

The window contract, original four-second mode, original physical settings and
independent-immersion assertions are unchanged. The reconstruction repair changes
trajectories; unlike the earlier test-only contract change, bit-identical body
motion is neither expected nor claimed. Before/after inputs and outputs remain
available. No independent research or protected historical evidence was edited.

The final combined run reduces the production qualification from six to three
mandatory failures. It does **not** qualify an unlock. The coarse late peak is
0.530423 m/s (limit remains strictly 0.5); its RMS and travel-range decay now pass.
The other failures are coarse-pool 3–4 s independent plane immersion 0.601932 and
rotated-pool late plane immersion 0.621468. The plane estimator's limitation does
not permit deleting these assertions without a separately qualified replacement.

The same coarse authored workload has median executed coupling time 18.23 ms in
the final run versus 10.88 ms before this task; particle timings are 8.33 versus
8.28 ms. These are diagnostic timings, not an isolated performance distribution,
and corrected trajectories differ. Consistent height ownership has a real cost.
Further optimization or a shared geometric representation is required before
claiming inexpensive advanced fluids at larger scales. Basic Water is unaffected.
