# Connected material ownership — bounded correction

**Status: representation correction implemented; authored basins still reject.**
This is not a runtime fluid backend or a completed Advanced Fluids V2 time step.
Main and the Alpha lock remain unchanged. Source parent:
`61c741a7a7b75011edead7d2e3a0d334dbf3a739`.

## What changed

`CutConnectedOwners` separates material ownership from immutable integration
fragments. An owner contains a connected union of actual fragments, its summed
capacity and their indices. Its bounding box is used only to bound local work;
it is never filled with water or used as a replacement solid boundary.

Fully open coplanar integration patches are summed before classifying a seam.
The first attempt classified each patch separately and incorrectly retained
small owners along a seam that was wide in total. Partial openings, closed
walls and microscopic throats do not acquire instantaneous shared ownership.
The bounded merge policy remains conservative: it can leave small owners when
no eligible wide seam exists. It does not promise universal agglomeration.

`MeasureCutOwnedDeparture` partitions the actual face footprint and follows a
normal departure through multiple members. A gap or solid terminates that
sub-column; integration never resumes behind it. Initial wet geometry is
retained per fragment, including minority-phase reconstruction. Analytic plug
controls show the outlet switching from gas to water after crossing several
internal fragment boundaries. A connected U-shaped-owner control proves that
connectivity alone cannot authorize water through its missing central region.

`CutOwnedProjection` uses owner water/gas ledgers with the retained boundary
pressure operator restricted to those owners. Boundary faces and finite gas
vents remain explicit. This is a pressure/transfer proposal, not a complete
advection or momentum integrator. It explicitly rejects nonzero retained
internal velocity rather than dropping it. No project/editor option selects it.

The pressure graph now also accepts per-owner row tolerances. The old scalar
entry point retains its original contract. The inner linear gate uses the same
accuracy budget as the independently audited final nonlinear equations; an
earlier attempt demanded another factor of ten, causing unnecessary linear
rejection. The final physical EOS and geometric publication guards were not
relaxed. Diagnostic failed attempts remain preserved.

## Early authored-fixture results

The preserved level/rotated basin fixtures retain their original float-authored
walls, body pose, water parcels, native float timestep/gravity and top gas vent.
Only the material-owner representation changes. The body remains frozen for
this admission gate; no live MK2, radial-world scene or new gameplay is claimed.

| Fixture | Integration fragments | Connected owners | Maximum members per owner |
| --- | ---: | ---: | ---: |
| Level | 20,344 | 17,231 | 32 |
| Rotated | 35,840 | 30,930 | 19 |

**Both now pass the pressure stage and reach geometric/material transfer
checks. Neither completes a fluid interval.** Rejection preserves the previous
proposal, and the authored probe exits **1**.

The remaining mechanism is no longer just independently owned tiny fragments:

- **Level:** the failing owner initially contains gas and no water. It receives
  about `3.88084e-6 m³` water during the interval. Its proposed gas outflow is
  below its total initial gas inventory, but the straight starting-state
  departure prism cannot establish the required interior path while water
  enters elsewhere. Enough material in the owner is not proof of a valid path
  or of the outlet phase at each instant.
- **Rotated:** the owner has only about `6.39488e-17 kg` initial gas. The
  unsupported trial's starting-face continuation assigns about `3.27636e-10 kg`
  to gas outflow, with only `9.80599e-11 kg` gas inflow. It overfills water and
  proposes negative gas mass. These are **unpublished trial values**, caught by
  the guards, not a state accepted by the engine.

The coarse pressure/quantity solve does not reconstruct a conservative interior
velocity field. Its discarded internal faces therefore do not supply an
arrival-time-aware route for incoming material. Start-of-interval wet planes
can measure a supported source prism; they cannot determine every outlet phase
when incoming material moves or turns through an owner during that same
interval. The fallback used to continue unsupported pressure trials is not a
physical transport rule and cannot be accepted as one.

The independent straight-tube oracle makes this distinction explicit. A gas
volume of 1 m³ receiving water at 2 m³/s exhausts its gas after 0.5 s. At 1 s,
the outlet must have carried 1 m³ gas and 1 m³ water. Keeping it gas for the
whole interval gives 2 m³ water inside a 1 m³ owner and negative gas mass,
while the algebraic EOS residual can still be exactly zero.

**Next required correction:** reconstruct conservative internal fluxes and
advance phase fronts through the interval, including changes of outlet phase,
then couple those transfers back to pressure. This must retain per-boundary
footprints and finite gas ownership. Merely allowing all incoming phases to
borrow any owner inventory, filling bounding boxes, or deleting the geometric
guard would repeat the failure. No further solver optimization or broad
numerical investigation was undertaken after identifying this boundary.

## Verification and preserved evidence

- 33 new ownership, seam, footprint, normal-continuation and local-pressure
  controls pass, including an AddressSanitizer/UndefinedBehaviorSanitizer run.
  Leak detection was disabled; no leak qualification is claimed.
- All 149 preceding admission/negative-control checks still pass.
- All 1,030 retained moving-grid checks across 26 suites still pass.
- Scalar pressure results, residuals and iteration counts match the parent
  implementation byte-for-byte over 48 deterministic graph cases.
- The original two authored proposals still fail transactionally. Neither that
  expected failure nor the analytic oracle is counted as a passing fluid step.

[Commands, source hashes and raw results](evidence/fluid-connected-owners-20261105/README.md)
include the rejected pre-seam and over-strict inner-budget attempts. Earlier
[Stage 1 evidence](evidence/advanced-fluids-v2-20261105/README.md) is untouched;
its source hashes refer to its original checkpoint, not this modified source.
Recorded elapsed times are diagnostics of admission attempts, not optimization
claims or useful runtime benchmarks.

No new native force coupling, water renderer, SDK API, save format, streaming
participant, exporter or playable V2 build is delivered by this bounded pass.
Existing Basic Water and production fluid behavior are unchanged.
