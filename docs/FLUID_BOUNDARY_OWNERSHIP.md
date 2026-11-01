# Owned water boundaries — 2026-11-01

Experimental engineering checkpoint, based on `7a377c8cad65cfce26dce4d4fc1d6313d3c1550e`.
**Advanced Fluids remain locked in Alpha. Main is unchanged.** These are Linux
automated results, not human acceptance or complete fluid qualification.

## Cause and correction

Removing water samples inside a solid destroys information: identical remaining
particles can describe displaced water or a genuine dry cavity. A wider donor
search cannot recover that distinction safely. Previously, conservative contact
admission correctly protected dry gaps but also rejected some displaced-water
support. Curved-surface reconstruction alone could not repair the missing data.

The field now retains *geometry ownership separately from fluid matter*:

* `FluidHydrostaticField::RetainDisplacedBoundary` accepts an explicit pre-cut
  water-domain snapshot, receiving solid geometry/pose and complete native
  `BodyHandle` identity (id and world token). This is useful when the caller
  actually possesses the pre-exclusion domain; it does not infer missing history.
* Native `FluidWorld` contact processing records bounded, solid-owned cut support
  cells, one marker per real particle. Their birth positions and orientations stay
  in the fluid chart. Current material and solid-contact witnesses prove lifetime;
  leaving contact, losing the owner, unsupported geometry or unresolved contact
  expires the data. Independently colliding owners are declined when ambiguous.
* Coupling groups marker indices by owner once per material revision, verifies
  actual material support and current solid geometry, and publishes them with the
  physical field. Cavity exclusions retain their original material indices.
* Queries accept owned support only inside both the captured and current solid,
  with matching identity/shape and connected compound component. Double-precision
  ray/cell interval unions avoid double counting. Current exterior surface fitting
  clips the retained coverage using the existing fit confidence. Unreliable fits
  fall back to observed birth geometry, not an imagined global water plane.
* Virtual geometry contributes **no mass, particles, momentum, velocity or
  acceleration**. Real exterior particles remain the velocity/acceleration donors.
  This adds no damping, energy clamp, solver iterations or timestep change.

The wet fraction blends occupied interval measures, not extra pressure impulses.
For ordinary occupied fraction `f`, extra proven coverage of length `L` adds
`L*(1-f)` to the interval measure. The fitted-cap version applies the same union
below the current cap. Existing confidence blends the two bounded models. This is
a finite-resolution reconstruction, not a new conservative pressure solver.

A separate lifecycle defect was reproduced: externally clearing `FluidWorld`
left the coupling's previous wet field alive until its next scheduled liquid
step. Revision-based refresh now removes it before another rigid force step.
The same new Clear control fails against the previous coupling and passes now.

## Changed implementation and tests

* `src/FluidHydrostatics.{h,cpp}`: owner-aware query and geometry publication.
* `src/FluidBoundaryOwnership.cpp`: validation, snapshots and interval ownership.
* `src/FluidBoundaryGeometry.h`: shared geometric predicates and cell intersections.
* `src/FluidDisplacementTracking.h`: passive native contact lifetime/capture.
* `src/FluidWorld.{h,cpp}`: marker storage and actual contact publication.
* `src/ProductionFluidCoupling.cpp`: per-owner publication and Clear invalidation.
* `CMakeLists.txt`: register the new implementation unit.
* `tests/FluidBoundaryOwnershipChecks.h`, `tests/FluidHydrostaticsTests.cpp`:
  independent wet/dry twins, stale/foreign owners, shape replacement, failed
  publication, mass/donor isolation, curved geometry, contact loss and dry layers.
* `tests/FluidLifecycleQualificationTests.cpp`: real wet/Clear/dry control.

No JavaScript API changed. Existing native Evaluate calls remain source-compatible
through a default owner argument, but native clients must rebuild against the
changed headers. Build clears retained field snapshots; failed snapshot replacement
preserves the previous publication. There is no new fluid save format: active
legacy-PBF cold saves, regional suspension and live origin shifts remain explicitly
unsupported. Existing Basic Water, experimental policy, locked production gate,
original scenes/fixtures and independent fluid research are unchanged.

## Known-answer evidence

| Case | Result |
| --- | --- |
| Original metadata-free 360-case fixture | 36 engine failures, 80 observer failures, unchanged |
| Same fixture copy with explicit pre-cut ownership | 0 engine failures / 360; worst error .035708; 0 frame failures; **80 observer failures remain** |
| Additional tilted-box/sphere shapes | 24 comparisons within unchanged .10 tolerance |
| Native actual-contact capture | 24 comparisons within .10; coarse dome sphere .436335 vs independent .444575 |
| Same captured snapshot mapped across frames | 48 accuracy passes; 0 frame failures at .001; max .00072223 |
| Independently initialized contacts across frames | 48 accuracy passes; **5 frame failures**; max .0165212 |

The explicit twin has identical physical particles but supplied wet-domain versus
dry-cavity history: it gives half immersion versus zero. Native dry-hole and
vertical-air-layer controls remain exactly dry, including a rod with real contact
on both sides of the air layer. Missing history is never reconstructed retroactively.
The explicit-snapshot executable still returns failure because its observer is
unchanged; it is not reported as an entirely green suite.

Separate rotated initialization of deeply embedded particles can select different
push-out faces. Positions differ by up to .3 m; this is not just query roundoff.
Rebuilding the previous and current FluidWorld against consistent current headers
produces identical 5,750,784-byte physical dumps across that matrix (SHA-256
`d0f1bcb039ec677e46ff817e21d45b6d5abf6d3ecac43018001b4a371c5fc57c`).
Thus the five integrated rotation discrepancies remain a pre-existing initialization
limitation, not a claimed covariance pass. This task does not redesign push-out.

## Combined final-source validation

The final `SHIP_RESULTS.json` names commands and exit statuses; `ship-*.log` and
their CSVs are authoritative. Earlier `final-*` logs belong to a rejected candidate.

Hydrostatics 217, vertical support 335, lateral wet support 109, drag 185,
lifecycle 24, project policy 26, Basic Water 56, supported-container 4,340 and
flat/radial demo 49 checks pass. Native fluid, cavity impact, boundary contact,
rigid coupling and smoothing checks also pass. Player integration has 5,661
checks and zero acceptance failures, with four existing diagnostic failures.

**Production remains failed:** 13,801 checks, the original one acceptance failure,
plus two existing diagnostics. Half-density coarse-pool 3–4 s plane-observer mean
is .6496175 against .5 +/- .10; the sampled mean is .5106341. The earlier audit
already demonstrated the remote-plane observer is not a general nonplanar oracle;
that does not independently establish actual moving-pool immersion here.

The unchanged current window/decay settling gates pass. Some measurements worsen:
half-density late RMS/peak become .165634/.327069 m/s versus .103021/.204490,
and light-body .147349/.285911 versus .046440/.087190. The historical 4 s
half-density velocity comparison is now .598597 versus .202693 m/s and fails its
original .5 comparison. It remains visible: historical comparison failures rise
from one to two, including one labelled legacy-mandatory. The current acceptance
mode was already window/decay; no settings, fixture or threshold was retuned here.
This is not a claim of uniformly improved settling or an entirely passing fluid gate.

Both original native application scenes run 1,200 fixed 1/60 s frames, exit zero,
and retain finite body positions. Ship peak speeds remain 2.095041 m/s radial and
6.703498 m/s flat, matching the previous no-launch observations. Other bodies have
maximum speeds 4.087501 and 8.174997 m/s respectively. These are regression traces,
not a new universal speed threshold or visual acceptance. An additional direct
native session verifies 125 actual particles and 600 executed fluid steps per
scene. Maximum measured body buoyancy is zero in these unattended initial
placements: these original-scene runs establish the dry/no-launch regression,
not a positive buoyancy control. Positive floating/sinking and swimming are
covered separately by the demo/production suites and known-answer fixtures.

Python observer tests pass 13/13, Pre-Alpha validation tests 14/14, registration
tests 7 passed / 2 skipped. Windows/Android were not qualified in this continuation.

## Cost and exact physical isolation

Four alternating before/after pairs, 480 steps per version, 100 actual particles,
identical colliders/settings, Release GCC 15.2 on Ryzen 5 5600/Linux; no concurrent
builds/tests launched during pairs. This shared workstation measurement isolates
particle contact tracking, not the whole coupled runtime:

| Wall cost | Previous | Current |
| --- | --- | --- |
| median | .426705 ms | .442024 ms |
| p95 | .501426 ms | .529318 ms |
| worst | .643203 ms | .797344 ms |

There is an overhead, not a claimed speedup. CPU medians are .424/.439 ms.
Each marker is 72 bytes; allocated marker capacity is 9,216 bytes in this fixture.
Storage is proportional to real markers/active cut cells and owner indices;
runtime does not copy the full particle domain per receiving body. The explicit
pre-cut snapshot interface does store separate geometry when deliberately used.

All eight 624,000-byte physical dumps are identical, including positions, previous
positions, velocities, accelerations and masses: SHA-256
`27a4354ae0941d63e77e8b95d9ef829ead1ff05124a52a4350887ee4e4086da4`.
Capture records 4,231 owned observations versus zero previously. Full *coupled*
body trajectories intentionally change with the corrected buoyancy reconstruction.
Different coupled trajectories cannot support a matched speedup claim.

## Rejected approaches and remaining boundaries

* Centre-only capture missed clearance contacts; actual cut support is needed.
* Advecting virtual domain samples made imagined water follow rising solids:
  production acceptance failures rose from 1 to 21. Complete rejected sources and
  failures are retained. No damping was added to mask the result.
* Current-cap-only cut cells were stable but missed the coarse dome sphere by
  .11647; retained birth geometry is required when the fit is unreliable.

Native capture remains a bounded contact-cell approximation. It does not prove
universal free-surface topology, thin gaps below particle resolution, fully
conservative pressure/momentum coupling or exact arbitrary moving-solid reconstruction.
It cannot recover pre-cut information from old inputs lacking ownership metadata.
Deep-start rotation, nonplanar moving-pool observation, reconstruction cost and
broader fluid-unlock qualification remain open. Retain the Alpha lock.

## Reproduction and human checklist

[Portable evidence, sources and reproduction notes](evidence/fluid-boundary-ownership-20261101/README.txt)
contain raw runs, failed candidates, checksums and final source/binary fingerprints.

1. With the diagnostic override only, compare original flat/radial scenes: no
   explosive force; normal floating, sinking and moving-object contact.
2. Check dry cavities and separated water layers remain dry; inspect coarse
   curved-body limitations and the retained failing observations.
3. Confirm normal Alpha still refuses Advanced Fluids, while Basic Water renders
   and swimming remains available independently.

No human acceptance, unlock, main promotion or patch publication is implied.
