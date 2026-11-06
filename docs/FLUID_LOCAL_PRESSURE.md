# Locally constrained fluid pressure — 6 November 2026

**Both original authored basins pass one coupled, frozen-body rest interval at
 their original timestep. This is a numerical reference, not a usable runtime
fluid backend.** Main, the Alpha fluid lock and the existing native solver are
unchanged. Independent research and older evidence are untouched.

## Integrated result

The level and rotated fixtures use the preserved geometry, authored float poses,
water import, gravity, finite top gas ports and original float timestep
`0.01666666753590107 s`. Water quantity and gas mass remain independent spatial
fields. Every local volume equation must meet the existing absolute/relative
thresholds. Outputs are published only after phase bounds, gas-only port and
whole-field quantity ledgers pass; no overshoot is clipped.

| Fixture | Spatial cells | Newton iterations | Worst normalized volume residual | Solve cost |
|---|---:|---:|---:|---:|
| Level | 21,696 | 15 | 0.000571 | 34.96 s |
| Rotated | 37,334 | 11 | 0.000628 | 35.99 s |

A normalized residual of **1** is the acceptance boundary. Largest absolute
residuals were `1.18e-23` and `1.97e-19 m³`. Water and gas stocks were nonnegative
in both fixtures. Conserved water ledger errors were `-8.99e-17` and
`-3.64e-29 m³`; gas errors were `-5.28e-23` and `-4.21e-27 kg`. Water outflow
through gas-only ports was zero. Tiny numerical capacity excesses remain
recorded, rather than clipped: `4.75e-31` and `1.12e-27 m³`, within the original
local acceptance budgets. Initial spatial remap errors are reported separately.

These are **one sample per fixture**, not median/p95 or a speedup claim. The
whole-process peak RSS was 154,788 KiB. Inputs remain frozen, starting from rest.
No physical substeps, delayed forces, reduced timestep or reduced fidelity
acceptance was used for the final result. Continuation experiments generated
numerical guesses only and are not counted as physical timestep passes.

The representation retains the authored grid resolution and actual aperture
splits. Optional extra midplanes are disabled for these reference runs; real
geometry, solid exclusions, port footprints and initial quantities remain.
The former default oversampling option is retained for existing prototype
callers. This is not permission to collapse or close microscopic openings.

## Architecture

- **CutCondensedPressure:** algebraically eliminates low-degree microscopic
  pressure rows using positive star-mesh updates. High-fill nodes remain in the
  reduced system. Eliminated equations are reconstructed and audited, not
  replaced by one common owner pressure. The old solver is an inexact
  preconditioner; iterative refinement checks the unchanged original operator.
  Gauge anchors act only inside the preconditioner.
- **CutDirectedTransport:** positive directed M-matrix condensation for
  implicit water and independent gas-mass transport. The physical storage of
  eliminated cells is retained. Forward-error checks use positive
  supersolutions and a separate conservative quantity bound.
- **CutExactResidual:** error-free floating expansions plus outward interval
  rounding bound residuals of the represented coefficients and state. Product
  underflow is enclosed, not silently treated as exact zero. Overflow/nonfinite
  inputs reject. This avoids inventing a large roundoff floor which obscures
  the smallest real cells.
- **CutLocalPressure:** the coupled local-volume coordinator. Its pressure
  linearization differentiates the actual implicit transport equations.
  Flexible Krylov iteration uses material- and transport-aware preconditioning;
  line search changes solver iterates, never the physical equations or timestep.
  Zero-flow upwind kinks use a generalized derivative. Final admission still
  evaluates the original nonlinear equations and quantity ledgers.

Compensated values use two binary64 components. This is not an imported research
solver, arbitrary rational arithmetic, a fluid particle rewrite, or a claim
that ordinary binary64 alone can represent every microscopic pressure mode.
The residual enclosure assumes standard IEEE round-to-nearest, gradual
underflow and no unsafe floating-point reassociation. This pass qualifies Linux
GCC in that environment; changed floating-point modes and other platforms are
not qualified. An explicit high-conductance precision-exhaustion control still
rejects without publishing; the numerical representation is not universal.

## What the costs actually show

| Phase | Level | Rotated |
|---|---:|---:|
| Material transport and trial evaluations | 28.39 s | 26.57 s |
| Transport tangent applications | 4.40 s | 6.57 s |
| Pressure linear solves | 1.86 s | 2.62 s |
| Transport evaluations | 65 | 28 |
| Linear applications | 14 | 10 |

Most time is now in material transport and its accuracy checks, not the pressure
linear solve alone. The reference must **not** be enabled for games at these
costs. Next work should profile/filter the transport path, reuse factorizations
and accepted evaluations, and keep the strict reference as an oracle for any
faster candidate. If that cannot reach a practical budget, reconsider the
runtime representation rather than pushing this reference into the engine.
No measurable runtime speedup is claimed here.

## Validation and retained failures

45 new numerical controls plus 254 retained controls pass (299 total).
AddressSanitizer, UBSan and leak checks pass for all 299 outside the tracing
wrapper. Local coupling is checked at gas densities 1.2 and 1.5 kg/m³,
including the matching ambient gas supply. LeakSanitizer requires the untraced run. The original dense fine-grid linear systems
also pass their original-row audits with the new condensed solver; their old
balance failures remain in historical evidence.

Rejected experiments include the simple frozen pressure correction,
volume-scaled preconditioning, a left-biased zero-flow derivative, overly
pessimistic global transport error bounds, and continuation attempts stopped
at their declared time budgets. Their logs/source snapshots are retained.
One early test driver dereferenced missing output after an intentionally
unsupported precision case; that driver failure was corrected, while the
precision-exhaustion rejection remains a test. It was not a native engine
crash or a physical pass.

## Remaining work and support boundary

This completes a meaningful **pressure + spatial transport reference interval**,
not Advanced Fluids V2 runtime integration or settling qualification. It has no
momentum advection between steps, moving-solid displacement update, MK2 force
publication, live wet queries, long-run settling, cold-save/streaming workflow,
new renderer surface, or game-ready performance. These are still required.
No new playable fluid build or human acceptance is claimed.

Human review checklist: inspect the two admitted fixture records and quantity
ledgers; reproduce them if desired; keep the fluid lock until a practical
multi-step native candidate passes body coupling, settling and persistence.

[Reproduction, raw results and source hashes](evidence/fluid-local-pressure-20261106/README.md).
