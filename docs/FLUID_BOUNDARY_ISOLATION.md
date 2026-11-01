# Exact-boundary fluid diagnostic — 2026-11-01

Base: Experimental `db90a17bee89a97dde17a07c6fcafc2cbd753297`.
**Investigation checkpoint: no production physics or reconstruction changes.**
Main, Alpha's compiled fluid lock, Basic Water and independent research are
untouched. This does not clear the original immersion check or qualify fluids.

## What the experiment establishes

The large remaining tilted-box and coarse-sphere errors predominantly arise in
**water/contact admission and the interface information available after solid
exclusion**. The existing body quadrature has much smaller residual error in
these cases. Replacing it first would address the wrong dominant mechanism.

The diagnostic uses an isolated copy of the actual `FluidHydrostatics.cpp`.
`EvaluateFluidHydrostatics`, shape-volume generators and physical settings remain
unchanged. An oracle supplies the mathematically specified free-surface boundary
at each query; it never derives an expected fraction from mass or density.

| Coarse domed surface | Tilted box wet fraction | Sphere wet fraction |
|---|---:|---:|
| Independent geometric reference | 0.430069 (160³ integration) | 0.444566 (analytic) |
| Native path | 0.328410 | 0.188985 |
| Exact cap; retain native admission | 0.328420 | 0.251916 |
| Exact cap; bypass both late admission checks | 0.423107 | 0.405711 |
| Exact boundary; unchanged body quadrature | 0.434434 | 0.444403 |
| Exact cap; bypass late checks and early range rejection | 0.434434 | 0.444403 |

Thus the tilted-box volume approximation contributes approximately **0.004365**
fraction error (0.44 percentage points), not the native 0.101659 deficit. The
sphere oracle differs from its analytic volume by approximately **0.000164**
(0.016 percentage points), not the native 0.255582 deficit.

The sphere has a mixed error: supplying the exact cap alone recovers about
0.06293 fraction. Missing fitted-surface information/fallback near the excluded
body therefore matters too. It is not accurate to attribute its whole deficit to
one Boolean contact check. The sequence above is a controlled ablation, not an
order-independent additive error budget.

## Actual mechanisms traced

1. **Per-height admission.** At each sampled height, the native reconstruction
   requires the query to lie in the exterior wet-column envelope and have local
   water or a route through its solid component to actual particle contact.
   Around a curved/tilted excluded body, the intended smooth boundary may place
   part of the body's virtual displaced volume below water even where the
   remaining exterior particles cannot establish contact at that same height.
   The fitted surface and the represented contact domain then disagree.
2. **Early range rejection.** Before fitting, field-AABB / projected particle
   height tests reject queries outside the represented particle extents. A cap
   inferred across a displaced hole can extend above those remaining exterior
   particle footprints. Those queries never reach the fitted reconstruction.
   Removing the late checks alone therefore does not reproduce the oracle.
3. **Available surface data.** Excluding whole particles removes nearby top
   observations, leaves truncated exterior columns and can make the quadratic
   fit unavailable or insufficiently reliable. Forcing the independently known
   cap isolates this loss without claiming that a runtime oracle exists.

Each late branch was bypassed separately, then together, then with early range
rejection bypassed. With the exact cap and all admission bypassed, the normal
integration path agrees **bit-for-bit** with the direct boundary oracle on all
52 geometry/frame combinations. This localizes the remaining discrepancy; it is
not an acceptable runtime implementation.

## Why the contact-gap case cannot be fixed by guessing

The original fine-grid box has half-width `0.30000001192092896 m`, particle
spacing `0.0625 m` and represented clearance `0.012499988079071045 m`.
It reports zero; the exact continuous-water oracle reports 0.5. Its aligned
0.25 m half-width positive control reports 0.5 natively and with the oracle.

A separate experiment constructs two genuinely different intended domains:

- Continuous water displaced by that 0.3 m half-width body: intended body wet
  fraction 0.5, retaining particle centres outside the body.
- An explicit 0.3125 m half-width dry cavity surrounding the **same body**:
  intended body wet fraction zero, retaining centres outside that cavity.

On this lattice, both produce **exactly the same 32,268 retained particle states**.
The packed position/mass arrays have the same SHA-256:

    c7b6d9a0d62de7d336eb98c532e6b3695ca69ac990ac3db32b0785b7cbeb2d82

Previous positions, velocities, accelerations, density and receiving geometry
are identical too. There is no particle centre between the two boundaries.
The current sampler has no observation that distinguishes these specifications.
This establishes missing boundary/domain information, not a floating-point
precision problem. It does not prove that every evolving-runtime gap has this
cause or that runtime contact history can always reconstruct the missing data.

A second negative control authors an unambiguously dry ring-hole with 0.075 m
clearance around the body. Native reports zero in both coordinate frames. The
unsafe exact-flat-cap/all-admission-bypass reports **0.5**, inventing buoyancy.
A height cap alone is not an exact oracle for a domain with that explicit void.
Simply disabling the guards, inflating contact distances or filling every fitted
cap would restore false wetness. These experiments are deliberately not shipped
as production functionality.

## Validation and scope

- Native trace-only mode reproduces all **24** previous additional-shape results
  bit-for-bit. The diagnostic did not accidentally change the original path.
- **416 case measurements:** 52 geometry/frame combinations × eight modes.
  Flat, bowl, dome, wave; tilted box, sphere, compound, fine contact gap and
  aligned positive control. These are ablations, not 416 independent passes.
- **26 harness controls pass**, including flat/full/dry oracle checks, comparison
  of sampled sphere volumes with an independent analytic integral, and expected
  detection of the unsafe bypass's dry-hole failure.
- Native global-frame maximum difference is 0.000134766; exact-boundary oracle
  difference is 0.0000000298. Both satisfy the unchanged 0.001 comparison.
- Unsafe forced-cap modes 2 and 4 reach a **0.00241208** frame difference and
  fail that comparison. They remain diagnostic failures, not qualified repairs.
- Unchanged native regressions: hydrostatics 169, vertical support 335, lateral
  wet support 109 checks, all pass. Original fixtures/thresholds are unchanged.
- The previous production, player and known-geometry suite results remain the
  latest qualification of the unchanged runtime; they were not rerun needlessly.
- No dynamic fluid conservation, pressure, energy, moving-body or performance
  claim follows from static boundary-oracle measurements. No platform expansion.

Sphere reference: for radius R and domed cap h(r)=-r²/2, let
`s = 2*R²/(sqrt(1+R²)+1)`. The independently integrated wet volume is

    Vwet = 2*pi*((R³ - (R²-s)^(3/2))/3 - s²/8).
    fraction = Vwet / (4*pi*R³/3).

The separate body-volume grid integrations use 64³, 96³ and 160³ samples, without
calling the runtime field sampler. Their agreement with the analytic sphere
control is checked; adjacent-grid agreement alone is not called proof.

## Corrective direction

Keep the safeguards. Prototype an explicit **solid-aware water boundary/domain
representation** that distinguishes displacement-created missing samples from
real air. It must preserve where the free surface and the actual wet solid
boundary lie, with consistent bounds and connected support. Partial geometric
ownership must survive exclusion rather than be guessed from centre deletion.

First prove that representation on the identical-particle twin domains, the
original contact gaps, tilted/spherical dome cases and genuine dry holes. Then
integrate it into runtime coupling with moving-solid/lifecycle checks and cost
measurements. The source of boundary information must be explicit; absent old
information cannot be invented. Replacing the particle pressure solver or
rewriting the body quadrature is not justified by these results.

This is a representation correction to design and test, **not permission to
claim a complete runtime fix from an exact-boundary diagnostic**.

Raw results, staged experiment gates, sources and fingerprints:
[evidence manifest](evidence/fluid-boundary-isolation-20261101/README.txt).
