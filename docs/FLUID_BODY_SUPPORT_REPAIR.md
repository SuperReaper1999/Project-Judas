# Particle-fluid body support — 2026-10-31

**Experimental correction; Alpha remains locked.** This repairs the confirmed
lateral dry-hole cases from the [unlock qualification](FLUID_UNLOCK_QUALIFICATION.md).
It is not an exact three-dimensional wet-geometry model or authorization to unlock,
publish, or promote. Main, Basic Water, conserved liquids and independent research
are unchanged. No one-way interaction option or research pressure backend is added.

Baseline: `82e324b2ae9630fb7a54125b36fe9e13938b97d9`. The containing commit identifies
the candidate. [Source hashes, raw results and rejected trials](evidence/fluid-body-support-20261031/)
are retained; the active working directory is `.cache/fluid-body-support-integration-20261031/`.

## Mechanism and correction

The existing column reconstruction correctly rejected external gaps and separated
pools, but a connected pool's convex hull still filled its internal lateral holes.
The hull could therefore report full immersion for a dry body. New adversarial
controls also demonstrated two additional ways of borrowing inappropriate support:
a separate wet compound child, and a wet end of a connected body whose other end
was dry.

The correction separates **indexing**, **solid geometry**, **wet contact** and
**dry evidence**:

1. A conservative hull remains a cheap rejection bound, not proof of immersion.
   A particle centre can be half a column from its bin centre, and its footprint
   extends up to another half-column. Physical queries therefore use a full-column
   half-width for this outer bound. Actual contact checks below determine admission;
   the larger bound does not itself create additional water.
2. Missing lateral support can extend through the queried solid's component only.
   Compound connectivity uses actual touching/intersecting oriented boxes, not
   adjacency of bins. A separate child cannot support the dry sample.
3. A closer missing exterior footprint is dry evidence rather than a reason to
   borrow a distant wet end. Existing lower water columns already vote dry in the
   vertical occupancy mean; applying a second binary rejection to them incorrectly
   removes legitimate partial immersion.
4. An empty bin is not necessarily an empty physical footprint: a jittered particle
   assigned to a neighbouring bin may overlap it. Such cells cannot supply definite
   dry evidence. Conversely, a rounded bin touching the solid is insufficient wet
   evidence: an actual particle footprint must touch the selected solid component.

These geometric predicates run before the existing occupancy mean and buoyancy/
drag equations. They retain the vertical column-envelope approximation and the
original exclusion of solid-shadow particles from bulk-motion donors. No forces
are clamped, no artificial damping or activation delays are added, and no mass,
gravity, particle count, timestep, cadence, iteration budget or controller is retuned.

The input particle/body state remains binary32; projection, SAT, distances and
column accumulation use ordinary binary64 as before. Contact uses a binary32
coordinate-roundoff allowance, not a spacing-based extra wet reach. No extended
precision, rational arithmetic or new pressure solve is involved.

### Maintainability and ownership

- `FluidSolidSupport.h`: private query-local solid preparation, strict donor
  occlusion, component selection and footprint contact. It retains no field/pose
  pointers beyond the query.
- `FluidWetFootprint.h`: pool rejection and bounded wet-support/dry-evidence
  traversal. Callback names distinguish solid coverage, possible material and
  actual wet contact.
- `FluidHydrostatics.cpp`: column reconstruction and force integration. Actual
  projected particle positions are retained alongside bin indices; bin centres
  are no longer substituted for physical contact geometry.
- `FluidWetSupportTests.cpp`: independent wet/dry, shape, component, shifted-bin,
  vertical-mean and conservative-bound controls. Public JudasJS/native function
  signatures and persistence formats are unchanged.
- The new support test and the existing lifecycle qualification are registered
  in the current water/lifecycle validation manifest.

## Final combined Linux results

GNU 15.2, Release, Ryzen 5 5600; native application checks use the NVIDIA GTX 970.

| Check | Result |
|---|---|
| New wet-support regression | **109/109**. Boxes, spheres, separated/touching/oriented compound parts, dry and wet ends, shifted footprints, a preserved vertical mean and positive coarse support. Includes rotated gravity, translation and a 4 km local offset. |
| Negative control | The same 109 checks linked against the baseline hydrostatics translation unit produce **48 failures**. Other engine libraries, fixture inputs and test source are held equal. This is a targeted negative control, not a claim of a full historical engine rebuild. |
| Existing hydrostatics | **120/120**, unchanged expectations. Includes the recorded 752-particle neutral-support fixture. |
| Production flotation | 8,021 recorded checks, **zero mandatory failures**; two retained existing diagnostics (zero-gravity micro-rest and observational sub-particle-size settling). |
| Particle-field player | 5,661 recorded checks, **zero mandatory failures**; four retained existing microscopic-rest diagnostics. Includes rotated density, common freefall and swimming into/out of actual particle water. |
| Supported containers | **4,340/4,340**, unchanged supported stationary-container cases. This does not certify unrestricted free-moving container conservation. |
| Cavity impact | Seven cases pass the independent momentum/energy controls. |
| Actual demo interaction | Pool **24/24**, planet **25/25**: dip, acquire, lift, carry and pour actual particles; count/mass remain exact. Destroy/reconstruct and surface invalidation checks pass. The existing headless test removes scripts/UI; the native export retains them. |
| Live dry-coffer regression | New ordinary authored scene, 512 moving particles and real static walls, compared for 600 played steps. No particle centre enters the coffer in either run. Baseline applies **172.42 N on the first dry step** and reaches 314.46 N later; candidate reports **zero immersion and zero water force throughout**, with the dry cube resting at y=0.13 m. The normal rendered application reproduces the corrected physical outcome. |
| Lifecycle/policy | Lifecycle **21/21**, project water **56/56**, project-fluid policy **26/26**. Default-off behaviour and explicit save/stream/origin rejection remain intact. |
| Build/gate wiring | 18 CMake organization tests pass; registration module reports nine tests with two Ninja-dependent skips. |
| Native scenes | Original radial, flat, lake +3 m and lake +10 m each run **1,200 fixed frames** without launches. Recorded original radial/flat body/player/harness rows match the previous repaired baseline exactly. |
| Fresh-process determinism | Two fresh rotated-flotation runs match all 240 recorded non-timing physical/solver rows and all six complete body/particle snapshot files exactly. This is fresh-start replay, not unsupported particle cold-save continuation. |
| Relocated export | Linux Experimental export succeeds; moved outside the source tree to a directory with spaces, then runs 240 frames with no engine-root override. No force-suppression/debug wrappers are linked into the ordinary runtime. |

Selected final production measurements: neutral body drift about 5.3 mm in four
seconds; rotated floating-body final local vertical speed 0.1970 m/s; 80 kg body
0.4330 m/s. The unchanged settling limit is 0.5 m/s. The coarser half-density
case's independently observed immersion is 0.5980 against its existing upper
comparison bound of 0.6: passing, with a narrow margin, not broad certification
of every release height, body or platform.

## Matched cost measurements

Three alternating-order pairs. Frozen-query sampling uses 100 observations per
process after five warmups: **300 measurements per implementation/input**. Dry
and wet snapshot geometry and prescribed uniform test gravity are identical in
both arms. The 752-particle wet snapshot returns exactly the same fraction 1 and
2116.07421875 N upward force in both implementations; the dry snapshot returns zero.

| Whole-body query | Baseline median / p95 / worst | Candidate median / p95 / worst |
|---|---:|---:|
| Dry | .132 / .194 / .220 ms | .127 / .200 / .261 ms |
| Wet | 3.144 / 3.622 / 4.239 ms | 3.215 / 3.758 / 4.649 ms |

The wet median adds about **0.070 ms (2.2%)**. This is a correctness correction,
not a claimed optimization. The dry difference is too small/noisy for a speedup claim.

The ordinary shipped-scene performance fixture uses 125 particles, 360 steps per
run, and unchanged physical/cadence settings: **1,080 whole-step measurements per
scene/implementation** across the three pairs. First steps are included.

| Scene | Baseline median / p95 / worst | Candidate median / p95 / worst |
|---|---:|---:|
| Classic | 1.458 / 4.328 / 5.280 ms | 1.575 / 3.957 / 11.799 ms |
| Terrain planet | 1.587 / 7.743 / 8.765 ms | 1.631 / 7.514 / 10.224 ms |

All unchanged fixture budgets pass. The higher candidate worst samples are
preserved, including the 11.799 ms classic first step; they are not discarded or
advertised as a worst-case improvement. Maximum headless process RSS over these
runs is 30,424 KiB baseline / 29,512 KiB candidate; this is not evidence of a memory
optimization or a leak-free soak. No concurrent agent compiler/game/functional
workload ran during sampling; normal host background work was not controlled.
These are CPU physics/coupling costs, not GPU timings or full-game FPS.

The larger 1,152-particle production case still takes roughly 12 ms amortized
coupling per rigid step in this qualification. Large pools, fine spacing and many
immersed bodies are not certified by the small-scene budgets.

## Rejected approaches and retained evidence

- Cell adjacency alone left the smallest real gaps wet.
- Requiring only raw vertical interval overlap broke 24 hydrostatic checks and
  later three production checks. Restoring the established vertical envelope was
  necessary; no acceptance threshold was changed.
- Restricting support to actual solid components fixed separated children, but
  did not by itself distinguish a dry end of a connected object.
- Counting existing lower columns as extra binary air evidence double-counted
  their dry contribution. A new independent mean control exposes that error.
- Treating absent bins as definitely dry introduced a rotated settling failure
  (0.5461 m/s). A read-only earlier-body-pose experiment made no difference and
  recorded zero historical-mask-only cells in that case; that hypothesis is not
  integrated. Bounded traces instead show actual particle footprints overlapping
  the supposedly empty bins by centimetres, far above the roundoff allowance.
- Using rounded bins as wet witnesses produced 172.421 N on a genuinely dry
  phase-shifted fixture. Actual footprint/solid contact removes it.
- A non-conservative outer bin hull also rejected genuine wet support. The
  positive anchored-footprint control proves the need for a conservative bound.
- The separately tried one-way dynamic-body policy missed existing settling
  gates and is not adopted. Containers and ordinary displacement remain intact.

## Remaining limits and unlock boundary

This remains a local, particle-resolution column approximation. Vertical internal
air pockets, sub-resolution shoreline details and exact arbitrary 3D wet topology
are not solved. Occupancy still uses the established vertical/area-weighted mean
and finite body-volume quadrature, not exact cut-volume integration. No universal
concave-cavity or thin-gap certificate is claimed.

Existing exterior momentum exchange, quiet-water jitter and observational tiny-body
limits remain. Actual pool/planet carry tests still record transient particle-speed
peaks; passing their count/mass/geometry gates does not establish a general speed
or passive-energy bound. Historical FLU-001 and protected research are unchanged.

Active particle cold saves, ownership by streamed regions and live origin shifts
remain explicitly unsupported. No missing state is silently reconstructed. Basic
Water and the separate conserved-liquid representations retain their own contracts.

No fresh Windows/Android execution is claimed. Native platform qualification,
advanced-fluid editor UI interaction and human gameplay acceptance remain pending.
The Alpha project lock, default-off Experimental checkbox and warning are unchanged.

The live coffer is `assets/scenes/fluid_wet_support_review.judas`: an orange cube
in a dry pocket, plus a light green and dense gray cube in the surrounding pool.
Two particles splash outside the open outer pool; perfect open-pool containment
is not claimed. An initial invalid authoring trial treated water-volume positions
as lower X/Z corners instead of X/Z centres, placing particles inside the supposed
dry pocket before simulation. That scene/result is retained and excluded from
engine findings. The corrected diagnostic explicitly verifies the initial layout.

## Reproduce / human checklist

```sh
cmake --build build --target judas_fluid_wet_support_tests judas_fluid_hydrostatics_tests judas_production_fluid_tests judas_player_fluid_integration_tests judas_fluid_demo_integration_tests judas_fluid_lifecycle_qualification_tests -j2
./build/judas_fluid_wet_support_tests
./build/judas_fluid_hydrostatics_tests
./build/judas_production_fluid_tests --output /tmp/body-support-production-fresh
./build/judas_player_fluid_integration_tests --output /tmp/body-support-player-fresh
./build/judas_fluid_demo_integration_tests --scene pool --output /tmp/body-support-pool-fresh
./build/judas_fluid_demo_integration_tests --scene planet --output /tmp/body-support-planet-fresh
./build/judas_fluid_lifecycle_qualification_tests
JUDAS_WORLD_STATE=none ./build/judas judas_tech_demo.judasproj
JUDAS_WORLD_STATE=none ./build/judas assets/scenes/fluid_wet_support_review.judas
```

Use fresh evidence directories. The `performance/` raw rows record all paired
commands; the baseline substitutes only the pinned hydrostatics implementation
into identical final engine libraries. No whole-engine before/after speedup is claimed.

For human review: inspect the original/flat scenes, actual floating/sinking props,
swimming and cup pickup/filling/carry/pour in both gravity arrangements. Use R to
reset the original scene. Check quiet-water jitter and the new dry-gap/compound
cases before deciding on supported limits. Review the explicit persistence and
platform gaps before any separate unlock proposal. Do not remove the Alpha lock
or claim human acceptance from these automated results.
