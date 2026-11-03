# Static particle-pool settling follow-up

Experimental source `a6eeedbe0d7fae39d0dcbb57b5569a2e9853046b`.
The original 240-step stationary warmup still ends at **0.0868477 m/s** mean
particle speed against **0.05 m/s**. It is a strict diagnostic failure. The
unchanged half-density run has zero mandatory failures in 2,176 checks and
passes the four-second comparison. This investigation does not change
fluid behaviour, fixture criteria, the Alpha lock, or independent research.

The predecessor of the preferred-clearance repair gives **0.0499571 m/s**
when its FluidWorld implementation is linked against the same host and
current compatible header. The new and predecessor runs match exactly
through five particle substeps. At substep six, all positions still match,
but the newer code submits 84 contact rows and the predecessor submits 92.
The eight absent rows all point up from the stationary placeholder. The
first later position difference is at substep eight.

One representative particle ends at `(-.125603, 1.090031, -.375761)` with
pre-contact vertical speed `-.195655` m/s. It is 31 micrometres above the
expanded top-face plane and 75.8 mm beyond the real lateral edge of the
placeholder. Its actual nearest-feature gap is about 118 mm. The
predecessor nevertheless transfers 3.057 N s upward on the top-face
normal. The new finite-feature classification drops that row. Keeping all
departed rows with zero wall velocity recovers the predecessor's complete
warmup digest and mean speed, but that also reinstates impulses on
departed static witnesses. It is a diagnostic control only.

| Isolated intervention | Mean speed after warmup, m/s |
| --- | ---: |
| Current source | 0.086848 |
| Keep all departed stationary rows | 0.049957 |
| Suppress initial hard history, use latest witness, or restore marker eligibility | 0.086848 each |
| Sweep each density projection from the start of that iteration | 0.077431 |
| Next-horizon finite preferred contact, gap-aware normal target | 0.088210 |
| Next-horizon finite preferred contact, zero normal target | 0.082511 |
| Use net projected density displacement after initial drift repair | 0.065590 |
| Project the pressure displacement onto stationary support mobility | 0.064674 |
| Project mobility in both the pressure denominator and displacement | 0.070609 |

The gravity-aware speculative prototype requests an unsupported positive
velocity target and aborts at contact admission. Projected displacement
and mobility trials change pressure and contact coupling and remain
unqualified. Every trial that retained final-feature admission failed the
unchanged warmup threshold. The predecessor's near-threshold pass is
therefore evidence of a different boundary impulse history, not evidence
that its departed contact law should be restored. Earlier phase traces
show density correction and wall response exchange large opposing kinetic
energy terms at stationarity. A physically credible next trial needs a
joint pressure/boundary update that uses a consistent active support set
for both pressure displacement and velocity response, with separate
known-answer static and moving-wall controls. Zero-gravity residual decay,
high-speed wedge residual, and curved geometry remain separate findings.

[Reproduction and focused traces](evidence/fluid-static-settling-20261103/).
The Python reproducer compiles scratch source variants and relinks them
against a Release GNU 15.2 build. Copy the script to a fresh ignored
`.cache/<experiment>` directory in the checkout before running so tracked
evidence stays untouched. Run from the repository root with
`.cache/2d-stage1-build` configured and `judas_production_fluid_tests`
built. Example commands:

```sh
python3 .cache/<experiment>/reproduce.py current
python3 .cache/<experiment>/reproduce.py old
python3 .cache/<experiment>/reproduce.py keep-static
python3 .cache/<experiment>/reproduce.py detail
python3 .cache/<experiment>/reproduce.py old-detail
python3 .cache/<experiment>/reproduce.py speculative
python3 .cache/<experiment>/reproduce.py mobility-velocity
```

The scratch driver exits directly after the original warmup diagnostic to
isolate the failure. Its summary therefore does not cover body release or
settling qualification. The tracked excerpts contain the original criterion
and all first-divergence rows. Full per-substep hashes and trial sources
remain in `.cache/fluid-warmup-recovery-20261103`.
