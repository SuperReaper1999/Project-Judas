# Finite-height fluid support corrections — 2 November 2026

Experimental continuation from `c27a7ec110a40b564f0e4adf23022de3471a5640`.
**Advanced Fluids remain locked in Alpha. Main, published packages, Basic Water,
M54/M55 and independent research are unchanged.** These are automated Linux
results, not human acceptance or complete fluid qualification.

## Two demonstrated defects

### A solid boundary was missing from the integration partition

The unchanged `half_025` moving-pool case supplied the failing input. At rigid
step 334, sample 44, the field and orientation were frozen. Moving the body and
its query upward by 0.1 mm changed that sample's wet fraction from .97661686 to
.72530812. The fitted surface had zero confidence and no retained interval was
active for this sample: neither a polynomial fit nor virtual water caused it.

The query divided its vertical support at water endpoints, then selected a solid
support component at each band's midpoint. The solid's own ray entry/exit could
lie **inside** a band. Crossing that midpoint made the entire band eligible or
ineligible, rather than integrating its partial support. The trace identified
`uncovered_root`, after successful neighboring contact at the previous pose.

The partition now also includes actual box/compound/sphere ray boundaries.
Coincident endpoints merge only within a **local geometry** binary32 input-error
allowance, preserving far-coordinate dry/full controls without making remote
water enlarge that allowance. There is no new physical smoothing distance,
damping, impulse cap, timestep, solver budget or acceptance threshold.

For the frozen input, the corrected central fraction is approximately .850841;
the +/-0.1 mm pair differs by .0000294 instead of .251323. Independent numerical
integration of 2,048 zero-height admission queries gives .851331. This validates
the specific finite-support numerical contract; it is **not** an independent
continuum immersion or pressure oracle. Root cuts do not enumerate every possible
footprint/contact/topology transition. Finite quadrature and midpoint
approximations remain.

### Captured ownership was decided from a finite query's centre

A second known-answer fixture specifies a flat wet pocket and a matching captured
solid. Moving a finite-height query's centre across the captured face by microns
previously discarded all retained support, despite finite overlap. Wet fraction
jumped from approximately .25 to zero. An explicit dry-cavity twin conversely
jumped from approximately .5 to 1. Both reproduce in identity and oblique frames.

Ownership now intersects the query ray with **both captured and current solids**.
Pre-cut water/air geometry applies only inside that intersection; the remaining
query uses current material with the same solid occlusion. Removing owner identity
on this internal fallback prevents recursive retained-domain access. Fully
covered queries retain their original fast path. Native birth-cell intervals are
clipped to the same ownership intersection and connected captured component.

Matching world token, complete owner identity, shape, actual material/contact
witnesses and lifetime validation remain required. This does not advect invented
water with a body, extend birth cells beyond either solid, fill dry gaps or create
matter, velocity or acceleration. Zero-height queries retain their point contract.

## Results on the combined candidate

| Check | Observed result |
| --- | --- |
| Hydrostatics, including finite ownership/ray controls and frozen production regression | 346 checks, zero failures |
| Same new driver with both original physics modules | **20 failures**, preserved negative control |
| Vertical support / lateral wet support / distributed drag | 335 / 109 / 185 checks, zero failures |
| Lifecycle / experimental policy / Basic Water settings | 24 / 26 / 56 checks, zero failures |
| Supported containers | 4,340 checks across four cases, zero failures |
| Flat/radial interaction demos | 49 checks, zero failures |
| Player/swimming integration | 5,661 checks, zero acceptance failures; **four existing warm-up diagnostics fail** |
| Full production body run | 13,801 checks, **one acceptance failure and one diagnostic failure remain** |
| Original metadata-free known geometry diagnostic | **36 engine / 80 observer failures remain**; 360 cases; zero frame failures |

The new regression groups contain seven frozen-input checks and 122 geometric,
ownership and negative controls. They do not represent 129 independently qualified
engine capabilities. The expanded hydrostatic driver is now in the ordinary
`water` gate group; known-failing diagnostics are not demoted or removed.

Original coarse-pool comparisons, unchanged settings/windows/thresholds:

| Measurement | Previous checkpoint | Combined correction |
| --- | ---: | ---: |
| 3–4 s remote-plane immersion (target .5 +/- .10) | .649617513, **fail** | .631789144, **fail** |
| 3–4 s native sampled immersion | .510634065 | .503968597 |
| 4 s vertical speed (historical comparison .5 m/s) | .598596811, **fail** | .358166277, pass |
| 8–12 s RMS / peak vertical speed | .165634 / .327069 m/s | .179024 / .375990 m/s |
| 8–12 s native / remote-plane immersion | .506303 / .531734 | .494279 / .584713 |

All current motion-window/decay gates pass. **Settling is not uniformly improved.**
The one production acceptance failure remains the original coarse remote-plane
comparison; the diagnostic failure is the zero-gravity pool warm-up. All five
historical four-second speed comparisons now pass. The root-cut-only prototype
passed the coarse original immersion comparison at .590505, but the combined
candidate does not; that isolated result must not be substituted for final-source
evidence. Both versions and the original failures are preserved.

Flat/radial demo particle mass remains exact and states finite. Their transient
particle peaks are 43.57 / 58.68 m/s, versus prior 43.72 / 49.07. The flat run pours
water beyond the finite basin and particles then fall; the radial cup impact has
a transient peak. Passing existing checks is not a universal particle-speed,
closed-world conservation or contact-stability certification.

## Cost and rejected approaches

Release GCC 15.2, Linux Ryzen 5 5600. Seven alternating before/after pairs, 300
identical frozen queries per batch; no build/test job launched during timing:

| Median batch mean | Original | Corrected |
| --- | ---: | ---: |
| Microseconds/query | 70.785 | 77.910 |

Min/max batch means are 69.584/76.470 and 76.947/100.641 us. This is approximately
10% **overhead**, not a speedup. Outputs intentionally differ. These are function
batches, not p95 physics steps, moving-world performance or a GPU measurement.
New interval vectors are query-local; no new persistent simulation state is saved.

Preserved rejected/diagnostic variants:

- Uniformly bisecting height bands established the integration dependence, but
  extra fixed subdivisions are not the repair.
- Replacing all finite-band geometry with zero-height slices broke the unchanged
  sphere hemisphere check and was rejected.
- Unmerged geometry cuts created roundoff slivers in existing 4,096 m dry/full
  controls. Local input-error endpoint merging preserves their unchanged limits.
- An initial new native-marker oracle incorrectly counted occluded actual matter
  in addition to birth geometry. Its failed run is retained; the corrected exact
  clipped-birth reference and foreign-owner-zero control expose that assumption.
- The earlier undersized-radius ownership fixture and source/results remain
  labelled as a rejected assumption, not production evidence.

## Changed files and boundaries

- `src/FluidBoundaryGeometry.h`: shared primitive ray/union intervals.
- `src/FluidBoundaryOwnership.cpp`: finite captured/current ownership integration.
- `src/FluidHydrostatics.cpp`: solid support entry/exit cuts.
- `tests/FluidFiniteBoundaryChecks.h`, `tests/FluidBandIntegrationChecks.h`,
  `tests/fixtures/fluid/support-band-334.json`, `tests/FluidHydrostaticsTests.cpp`:
  durable positive/negative and frozen-input regressions.
- `tools/validation/suites.json`: ordinary hydrostatic gate and current finding
  disposition; documentation index/current-link inventory: latest evidence.

No public API, authored setting or save format changes. Active legacy-PBF cold
saves, suspension and live origin shifts remain unsupported. The original
metadata-free curved/cut geometry limitations, deep-start contact rotation,
nonplanar moving-pool observation, reconstruction cost and broader unlock
qualification remain open. No Windows/Android qualification in this pass.

## Reproduce and review

```sh
cmake --build BUILD --target judas_fluid_hydrostatics_tests judas_fluid_vertical_support_tests judas_fluid_wet_support_tests judas_production_fluid_tests -j2
BUILD/judas_fluid_hydrostatics_tests
BUILD/judas_fluid_vertical_support_tests
BUILD/judas_fluid_wet_support_tests
BUILD/judas_production_fluid_tests --output FRESH_DIRECTORY
BUILD/judas_fluid_known_geometry_tests --output FRESH_DIRECTORY/known.csv
```

Expected exits: 0 / 0 / 0 / **1 / 1**. Original failures are not passes.
See [portable evidence and fingerprints](evidence/fluid-finite-support-20261102/README.txt)
for exact commands, raw before/after results, rejected sources and final hashes.

Human checklist:

1. Under the deliberate diagnostic override only, compare flat and radial water:
   floating/sinking props, tilted objects, cup filling/pouring and swimming.
2. Check dry cavities, separated bodies and movement across water boundaries;
   look for a remaining finite support pop or abrupt force.
3. Confirm normal Alpha still refuses Advanced Fluids and Basic Water/swimming
   remain usable independently. Do not unlock from these scoped passes alone.
