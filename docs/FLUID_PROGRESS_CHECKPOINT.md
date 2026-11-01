# Experimental fluid engineering checkpoint — 2026-11-01

**Engineering checkpoint only. Advanced Fluids remain locked in Alpha. Main is
unchanged. This is not full fluid qualification or human acceptance.**

This checkpoint consolidates the wet-geometry investigation, the operator-approved
window/decay contract and a correction to finite-body viscous loading. It retains
all original failures and unsupported combinations rather than manufacturing a
green fluid-unlock result.

## Implemented corrections

- Keep resolved vertical water layers separate; do not fill resolved air pockets
  merely because water exists above and below them. Retain the documented
  sub-resolution reconstruction and lateral-support boundaries.
- Use one height-dependent exclusion rule for displaced solids, so a vanishing
  remote donor cannot change ownership of every other column.
- Give body hydrostatic sampling a field-owned reconstruction frame, with a
  conditioned geometric seed and the existing caller fallback for degenerate
  clouds. Equivalent parent/compound-child representations now agree.
- Exclude background-motion donors using their actual horizontal positions,
  rather than rounded column centers. Preserve the full-query solid exclusion
  so a body does not borrow its own contact reaction as background acceleration.
- Integrate the existing authored drag over wetted volume. A single force at the
  wet centroid cannot resist spin around that point. Distributed loading includes
  real lever arms and finite box-subvolume moments; no angular damping setting,
  energy clamp, delay, extra solver iterations or changed timestep was added.
- Make the one-interval column-union case explicit and walk subsequent intervals
  using iterators. This removes GCC 15's inlined bounds warning; the post-cleanup
  coarse run preserves all 720 non-timing physical rows exactly.

The drag law is

    df = -k * (bodyMass / bodyVolume) * (v + omega cross r - u_water) dV

Its integrated six-dimensional resistance is symmetric positive semidefinite.
The fixed-size double-precision Cholesky solve advances that law with implicit
midpoint. For fixed mass/inertia and stationary water its impulse work equals
rigid kinetic-energy loss; no energy cap is applied. The input velocity retains
the existing ordering after the gravity/hydrostatic kick. The existing authored
`hydrostaticDragRate` is unchanged. Water velocity is piecewise constant within
each quadrature subvolume; finite sample and fluid reconstruction errors remain.
Sphere angular moments use the existing finite point quadrature.

## Verification

| Check | Result |
|---|---|
| Vertical geometry, vanishing-donor continuity, equivalent solid frames | 317 checks passed |
| Hydrostatics, including recorded rotated-force covariance | 124 passed |
| Lateral wet support | 109 passed |
| Window/decay validator | 46 passed |
| Independent calm model / native reference | 81 passed |
| Distributed drag: analytic decay, passivity/work, frame/Galilean covariance, actual wet geometry | 185 passed |
| Swimming | 89 passed, 2,475 steps |
| Project fluid policy / lifecycle | 26 / 21 passed |
| Flat / spatially varying planetary-gravity demo integration | 24 / 25 passed |
| Build organization / registration | 18 passed; registration nine tests with two environment skips |
| Full production qualification | **13,801 checks: one mandatory failure**, two scoped diagnostics |
| Original four-second mode | **8,021 checks: one mandatory failure**, two scoped diagnostics |

The final production test driver and all acceptance thresholds are unchanged from
the approved window-contract candidate. Its original failed four-second mode is
still available. All resolved four-second speed comparisons now pass, but that
alone is not the qualification criterion.

### Motion measurements

These compare the candidate at the beginning of this continuation with the final
distributed-drag candidate, using identical authored fixtures, cadence and rate.
Corrected trajectories differ; no bit-identical-before/after claim is made.

| Fixture | Late peak before / after (m/s) | Late RMS before / after | Late travel before / after (m) |
|---|---|---|---|
| half_020 | .375252 / .128914 | .150201 / .055676 | .115878 / .045154 |
| half_025 | .530423 / .286080 | .187565 / .127492 | .132042 / .083179 |
| rotated_half | .441299 / .101515 | .182092 / .053657 | .143646 / .033329 |
| light_80kg | .382859 / .286183 | .139215 / .156928 | .112682 / .086620 |

All four pass the unchanged late peak <0.5 m/s, RMS decay and travel-range decay
checks. Light-body late RMS increases slightly; this is not described as a
uniform improvement of every numerical measurement.

### Remaining failed observation — not removed

The coarse pool's original 3–4 second remote-plane immersion estimate is
**0.6341740926**, outside **0.5 +/-0.10**. This same assertion was already failing
at 0.6019317627 before this continuation. The sampled mean is 0.5055831671.
Late sampled/remote-plane means are 0.5017629591 / 0.5662507375 and pass their
unchanged checks. The earlier six-failure and three-failure runs remain archived.

Manufactured geometry proves the remote horizontal-plane estimator is not valid
for a general sloped/nonplanar surface: it can report 83% for an exactly
half-submerged box. That does not prove the runtime's wet estimate is correct.
An independent local affine replacement passed 92 manufactured controls after a
source-roundoff boundary correction, but failed on actual pool data: its local
fit has roughly 0.18–0.23 m RMS residual and incompatible early/late immersion
estimates. **It was rejected and is not part of active acceptance or production.**
Both old and proposed observations are preserved. No tolerance was widened.
A qualified nonplanar wet-geometry oracle is still required before unlocking.

The zero-gravity quiescent-preparation diagnostic and under-resolved small-body
legacy settling diagnostic also remain recorded. No new supported scale is
claimed for bodies smaller than the represented particles.

## Performance and compatibility limits

The coarse case's median executed coupling is about **18.25 ms** (particle work
about 8.37 ms), versus 18.23 / 8.33 ms before distributed drag. These sequential
same-authored-workload timings are diagnostic, not isolated benchmark acceptance;
trajectories differ. Height-dependent reconstruction is still substantially more
expensive than the earlier inconsistent shortcut (about 10.88 ms in that run).
Basic Water and disabled advanced-fluid projects do not execute this path.

No M54/M55 conserved-liquid implementation, independent research, renderer, MK2
solver, gameplay settings or project acceptance policy was changed. No new
persistence state is introduced: the drag moments are derived each step. Active
legacy-PBF cold saves, regional suspension and origin shifting remain explicitly
unsupported, as checked by lifecycle preflight; this checkpoint does not claim
those integrations work. Windows/Android and physical hardware were not qualified
in this continuation.

## Checkpoint boundary and next work

This is ready to preserve the independently tested engine corrections on
Experimental **with the existing fluid lock and unresolved observation failure**.
It is not ready to enable Advanced Fluids in an Alpha patch. Main and release
channels remain unchanged.

Next: establish a qualified, nonplanar local wet-volume observation, then repeat
fluid-unlock qualification and address reconstruction cost. Do not silently use
the rejected plane fit or relabel the failed production test as passed.

Sources, commands, before/after CSVs, rejected prototypes and fingerprints:
[checkpoint evidence](evidence/fluid-progress-checkpoint-20261101/README.txt).
Earlier investigation reports remain historical records of their exact sources.
