# Advanced Fluids settling trials

The tested Experimental baseline is `2fccc514bb7bd2e384c4bf726393c273d0b44beb`.
No pressure or contact prototype passed the combined acceptance checks. The
engine, original fixtures and thresholds are unchanged. The Alpha lock stays
in place. [Source patches, commands, traces and fingerprints](evidence/fluid-settling-trials-20261103/).

## Original results and extended observations

The production suite records 13,801 checks with zero mandatory failures and
five strict diagnostic failures. The three 0.25 m density cases share the
same stationary warmup. Zero gravity and the sub-particle box account for
the other two failures. The player suite records 5,661 checks with zero
mandatory failures and four strict micro-rest failures.

The original pool diagnostic is mean particle speed below 0.05 m/s at
240 fixed steps. The extended controls kept the stationary placeholder in
place for 20 seconds and evaluated the original diagnostic at four seconds.
They do not substitute a later evaluation for that diagnostic.

| Pool | Mean at 4 s | Mean at 20 s | Mean over final second | Bulk at 20 s | Particles above rim at 20 s |
| --- | ---: | ---: | ---: | ---: | ---: |
| half_025 | 0.086848 m/s | 0.061312 m/s | 0.057748 m/s | 0.015390 m/s | 0 |
| zero_gravity | 0.097213 m/s | 0.094924 m/s | 0.094925 m/s | 0.094922 m/s | 300 |

The zero-gravity result is sustained bulk drift. The earlier phase trace
identified the initial pressure kick and wall momentum that generated it.
Ordinary-gravity motion persists at smaller scale. In a separate stationary
two-particle column, the upper particle stays at a bit-identical position
while retaining a downward velocity of 0.01648 m/s. The floor impulse still
balances the column weight to approximately 0.00000011 N·s per step. This
isolates a position/velocity inconsistency even without free-surface
reconstruction, moving walls or rigid-body buoyancy. It does not explain
every mode of pool motion. [Earlier zero-gravity trace](FLUID_SETTLING_FOLLOWUP_20261103.md).
[Two-particle control](evidence/fluid-settling-trials-20261103/joint-support/README.md).

### Sub-particle body

The 0.12 m box is smaller than the 0.20 m particle spacing. At four seconds
it is moving upward at 0.7269 m/s against the original 0.5 m/s comparison.
A calm-water control uses the independently measured release surface, exact
upright box immersion, gravity, half-density buoyancy and the existing
2 s⁻¹ full-immersion drag rate. It converges to 0.9290 m/s at the same time.
The control omits fluid feedback, surface motion and rotation. Even perfect
immersion in that simplified model does not imply the four-second pass.

A scratch extension preserves every original physical trajectory field over
240 steps and then continues for eight seconds. In the solver, one-second
vertical peak speed falls from 1.8622 m/s at 3–4 s to 0.3500 at 7–8 s and
0.1123 at 11–12 s. The last one-second travel range is 17.9 mm. This
establishes decay in this fixture while leaving the failing diagnostic and
sub-particle-scale support boundary intact.

## Rejected corrections

| Trial | Benefit | Reason for rejection |
| --- | --- | --- |
| Joint density position and velocity solve with finite support | half_025 warmup 0.042237 m/s. Independent column isolates the split-state defect | Four mandatory half-density failures and one neutral-drift failure. The scratch support mobility also needs an explicit dynamic-body ownership model |
| Nominally normalized Poly6 with exact gradient | zero_gravity 0.000000711 m/s | Four mandatory half_025 failures and five rotated failures |
| Wider Poly6 at its unbiased nominal lattice root | zero_gravity 0.000000096 m/s | half_025 warmup worsens to 0.185967 m/s and early independent immersion fails |
| Compact quartic with its exact gradient | Removes nearly all zero-gravity startup motion | Five mandatory half_025 failures |
| Compact quartic density with existing Spiky projection | zero_gravity 0.000255 m/s; half_025 0.036188 m/s; rotated half 0.039682 m/s. Full rigid suite has zero mandatory failures and one remaining strict diagnostic | The deeper player pool develops sustained bulk oscillation. Full player suite has seven mandatory failures and seven strict diagnostics |

The compact quartic changes only the pressure density estimate in the trial,
while the velocity smoother retains its original kernel. At the standard
support/spacing ratio of 2.1 the cubic Poly6 kernel overestimates the nominal
lattice density by 1.48015%, versus 0.00443% for the normalized quartic. The
quartic still responds to real compression. The independent lattice sweep
shows that neither kernel is uniformly less biased at other ratios.

The seven quartic player failures separate into three deep-pool bulk-speed
failures and four assertions around a fixed-time swim jump. The three
density cases use identical warmup geometry. Over the last 90 liquid
substeps the 20,000 kg pool's vertical bulk speed spans −0.2244 to +0.2217
m/s, versus −0.01065 to +0.00774 m/s in the baseline. Gravity contributes
approximately −2,180 N·s per substep. The wall response ranges from
1,124 to 3,275 N·s with quartic versus 2,075 to 2,285 N·s in the baseline.
Direct net pressure and smoothing impulses over each late substep remain
below 0.007 N·s in magnitude. This locates the bulk oscillation in the
pressure-driven variation of wall support, rather than in large direct
net pressure or smoothing momentum errors.

The jump assertions fail after the player has already exited the quartic
water at the original scheduled frame. The unchanged run still enters and
exits the field. A separate control requests the jump at observed water entry
and passes all 1,232 checks for each implementation, including bounded swim
acceleration and exact agreement with a playerless particle trajectory. This
classifies those four timed failures without changing or overruling them.
The deep-pool oscillation still rejects the candidate on its own.

## Remaining boundary

The two causal controls for further pressure work are the rest-lattice
startup kick and the supported column's mismatched position/velocity.
A candidate needs to pass both and keep the pressure load and wall reaction
consistent in the deep pool. Stationary support should be checked before
moving walls, with ownership and reaction applied once. Keep the original
dry-gap, immersion, decay, freefall, player and rotated-gravity checks in
the acceptance set. The ordinary warmup failures and the sub-resolution
comparison remain open.

Current Linux spot checks passed the Alpha fluid-policy suite (26), project
water (56), settling contract (46), independent reference (81) and contact
clearance (545). These are tests of the unchanged engine. No performance
improvement or platform qualification is claimed.
