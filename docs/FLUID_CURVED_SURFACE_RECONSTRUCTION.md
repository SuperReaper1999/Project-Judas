# Curved free-surface reconstruction — 2026-11-01

Starting checkpoint: `03fbd812963d6190f18a881cf68ec44339c4f326`, Experimental.
This is an incremental spatial-sampling correction, **not a fluid unlock**.
Main, published Alpha packages, Basic Water, particle integration, drag,
solver budgets and the independent fluid research are unchanged.

## Mechanism and correction

The prior sampler averaged neighboring column heights over a body-sized search
reach. On a bowl it imported taller remote water into the body footprint; on a
dome it imported lower remote water. Discarding a whole column whenever any of
its footprint overlapped a solid compounded the error near sloped interfaces.
The original test observer independently assumes a single remote median plane;
it is also wrong on known curved geometry. Neither answer is a ground truth for
the original moving pool.

Connected physical queries now fit a local quadratic height patch to reliable,
single-layer **exterior** column tops at their actual tangent coordinates.
Weighted binary64 Householder QR uses the normalized basis

    h(x,z) = c0 + c1*x + c2*z + c3*x*x + c4*x*z + c5*z*z.

The coordinates are relative to the query, normalized by its existing search
reach. The fitted cap is c0 at the query. No temporal filter, rest-state target,
velocity classifier, density-derived immersion target or added damping is used.
Particle positions remain binary32; this does not convert the fluid solver to
double or import a research pressure backend.

Enclosing donor coverage, numerical rank and source validity are required.
Truncated tops, relevant bottom boundaries and multilayer donors are excluded.
The latter also reduce confidence in proportion to their weight. A smooth
confidence combines effective sample redundancy, numerical conditioning and
prediction uncertainty. Effective count is `(sum w)^2 / sum(w*w)` (zero
confidence at six, full redundancy factor at twelve). Prediction uncertainty
propagates the weighted residual and binary32 coordinate error through the QR
evaluation functional. The spatial model's error budget is one quarter of a
particle spacing; the independent tests' 0.10 fraction / 0.001 frame tolerances
are unchanged. Poorly supported data fall back to the original reconstruction.

A subtle rejected prototype used residual and donor count alone: one arbitrarily
weak donor could complete a missing quadratic mode and cause a finite change.
Propagating input uncertainty rejects that ill-conditioned prediction. Existing
vanishing-remote-donor regressions remain authoritative.

For the qualified reconstructed branch, each column's exterior area is clipped
against the actual box-union or spherical cross-section. Ordinary baseline
queries retain their original ownership rule. Partial area is **not proof of
contact**: a reconstructed solid query must still establish actual represented
particle contact through the sample's selected solid component. Fitted heights
never bypass the lateral, vertical-gap or disconnected-component checks.

The original and reconstructed vertical integrals use separate partitions. A
zero-confidence fit therefore reproduces the original answer, rather than
changing its integration points invisibly. Low-level unconnected queries and
background velocity/acceleration reconstruction are unchanged.

## Known-answer results and remaining failures

The original 360-case diagnostic is retained byte-for-byte: six mathematical
surfaces, three spacings, five immersion offsets, both solid-occupancy modes and
two global coordinate frames. Analytic volume and independent vertical-ray
integration agree within 0.0000031. This is specified geometry, not a claim that
a curved free surface is a hydrostatic equilibrium under uniform gravity.

| Measurement | Before | Corrected candidate |
|---|---:|---:|
| Engine fraction failures / 360 | 76 | 36 |
| Engine frame failures / 180 | 3 | 0 |
| Legacy observer fraction failures / 360 | 80 | 80 |
| Largest engine frame difference | 0.00427097 | 0.000108123 |

All remaining 36 engine failures are the previously preserved fine-grid,
partially immersed displaced-box fixtures with a **0.012499988 m represented
contact gap** after particle-centre deletion. The correction does not fabricate
water across that gap. The original strict diagnostic still exits 1; it is not
a passing release gate.

At 0.25 m spacing, the known half-wet upright bowl changes from **0.684375 to
0.496881**; the dome changes from **0.119350 to approximately 0.4198**. Both now
meet the unchanged ten-percentage-point comparison, as do all tested original
cases with represented contact. Affine and saddle cases recover approximately
0.5. This is bounded accuracy, not exact arbitrary-surface integration.

Additional independent 64³/96³ body-volume sampling checks tilted boxes,
spheres and a two-piece compound against flat, bowl, dome and wave geometry.
They expose important remaining limits: a coarse displaced sphere under a dome
is still substantially under-wet, and a tilted box under a dome remains about
0.102 below its geometric reference. Partial-area clipping improves these cases
but does not solve body/contact undercoverage or oriented subvolume integration.
Three of these 24 additional measurements exceed the unchanged 0.10 comparison.
Those results remain in the evidence; no universal curved-body accuracy claim
is made. The current subvolume quadrature still uses a projected vertical slab,
not exact clipping of every oriented volume cell against the fitted surface.

The original moving-pool `half_025` remote-plane comparison remains failed.
Its original settings, observation formula and threshold are unchanged. Better
known-surface interpolation does not establish that the legacy plane is a valid
measurement of its evolving nonplanar water, nor establish the true immersion
there. The final raw production results are supplied alongside this report.

## Final-source qualification and cost

| Check | Result |
|---|---|
| Hydrostatics, including 45 new controls | 169 checks, zero failures |
| Vertical support / lateral wet support | 335 / 109 checks, zero failures |
| Wetted-volume drag / particle boundary contact | 185 / 24 checks, zero failures |
| Lifecycle / advanced-feature policy / Basic Water | 21 / 26 / 56 checks, zero failures |
| Flat and planetary real interaction | 49 checks, zero failures |
| Player-fluid full suite | 5,661 checks, zero acceptance failures; four existing micro-rest diagnostics fail |
| Production-fluid full suite | 13,801 checks, one preserved immersion acceptance failure; two existing diagnostics fail |
| Original known-geometry diagnostic | 36 engine and 80 observer failures retained; zero frame failures |
| Observer Python / validation-registration Python | 13 / 14 tests pass |

The newly added end-to-end curved-box assertions **fail eight times with the old
03fbd812 sampler**, linked against the same current test object. They pass with
the corrected implementation. No existing assertion, fixture, threshold or
observer implementation was changed. The final build has no compiler warnings.

In the original coarse pool, the original 3–4 second observer mean is now
0.636864 (previously 0.634174), still outside 0.5 ± 0.1. The runtime sampler's
corresponding mean is 0.496342. The late settling window passes: RMS 0.103021 m/s,
peak 0.204490 m/s and vertical range 0.055197 m. These are distinct observations;
neither is substituted for the preserved failed check.

Four alternating-order before/after query benchmark pairs use the same 512-particle
fields and fixed query inputs. Each row aggregates 20 batches of 200 queries.
No build or test job overlaps these microbenchmarks. Linux Ryzen 5 5600, GCC 15.2,
Release `-O3 -DNDEBUG`; timings are host diagnostics, not platform certification.

| Connected query | Before median / p95 / worst (µs) | Corrected median / p95 / worst (µs) |
|---|---:|---:|
| Flat | 11.35 / 14.15 / 15.59 | 22.40 / 23.90 / 25.74 |
| Bowl | 13.45 / 14.84 / 17.51 | 25.20 / 26.40 / 29.30 |
| Dome | 9.06 / 10.28 / 10.36 | 20.02 / 20.90 / 20.97 |

This is an accuracy correction **with additional cost**, not a speedup. The coarse
pool's executed coupling median is about 19.92 ms versus the previously recorded
18.09 ms, but these full-scene runs were not alternating matched performance
trials; do not infer a precise whole-engine regression ratio from them. Particle
integration is unchanged. The fit runs only for connected advanced-fluid queries;
Basic Water does not pay for it. No Windows, Android or browser qualification is
claimed for this checkpoint.

## Rejected experiments

- Whole-column masks plus quadratic fitting: fixed the bowl, retained excessive
  dome undercounting.
- Unconditional partial-column ownership: reintroduced six false-wet dry-gap
  checks and broke the low-level column mean. Rejected.
- Hard minimum donor count / hard multilayer veto: avoidable admission switches.
- Weighted count without uncertainty propagation: failed 24 existing
  vanishing-donor continuity checks. Rejected.
- Fixed eight-point vertical quadrature: failed sphere/continuity/frame checks.
- Initial circle clipping classified a tangent midpoint as an interior edge.
  The independent circle-in-square area test caught it; the corrected method
  uses a sector for that boundary. The failed source and log are preserved.

No failed experiment was accepted by loosening its test.

## Verification and reproduction

Build an Experimental Release configuration:

    cmake --build BUILD -j2 --target judas_fluid_hydrostatics_tests \
      judas_fluid_vertical_support_tests judas_fluid_wet_support_tests \
      judas_fluid_known_geometry_tests judas_production_fluid_tests \
      judas_fluid_demo_integration_tests judas_player_fluid_integration_tests
    BUILD/judas_fluid_hydrostatics_tests
    BUILD/judas_fluid_vertical_support_tests
    BUILD/judas_fluid_wet_support_tests
    BUILD/judas_fluid_known_geometry_tests --output NEW/known.csv
    BUILD/judas_production_fluid_tests --output NEW/production
    BUILD/judas_fluid_demo_integration_tests --output NEW/demos
    BUILD/judas_player_fluid_integration_tests --output NEW/player

Expected original diagnostic failures remain failures. Other final-source results,
commands, source/binary fingerprints, raw measurements, additional-shape source,
negative controls and rejected prototypes are in
[evidence/fluid-curved-reconstruction-20261101/README.txt](evidence/fluid-curved-reconstruction-20261101/README.txt).

## Review boundary

This correction can be checkpointed independently; Advanced Fluids are **not
qualified to unlock**. Remaining work includes contact-undercoverage cases,
oriented/curved-body accuracy, an independently justified observation for the
original moving pool, and the previously documented persistence/streaming/origin
and numerical restrictions. Basic Water does not depend on this reconstruction.
No human acceptance, platform certification or conservation guarantee is inferred
from these static geometry tests.
