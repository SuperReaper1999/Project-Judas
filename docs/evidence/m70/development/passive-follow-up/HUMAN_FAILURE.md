# Human passive-mode performance failure

The operator reviewed Character Lab and reported: "when you switch to passive
framerate immediatly crumbles other than that i think its fine". This is a
blocking ordinary-use performance follow-up, not overall M70 acceptance.

The exact preceding exported candidate was reproduced before changing physics,
using its normal public logical controls: switch to physical at frame 15,
partial startup, passive at frame 60, then continue to frame 500 at 1/60 s.
`application-baseline.log` and `application-baseline-m56.json` preserve that run.
`preceding-candidate.sha256` preserves the preceding file fingerprints.

The capture confirms two full articulations, 30 joints and 37 live bodies.
Peak retained fixed work reached 53.268 ms; rigid physics accounted for
52.669 ms and the contact/joint solver scope for 52.017 ms. In later settled
frames simulation work was 1.184 ms median / 1.597 ms maximum. This reproduces
an expensive transition/collapse window, not an indefinitely slow steady state.
The normal ten-rig benchmark was not equivalent to this two-rig transition.

These measurements are CPU profiler timings from the ordinary exported
application with hidden software GL. They establish the simulation stall;
they are not a measured desktop GPU framerate or catch-up certification.
The human report remains authoritative for the actual visible slowdown.

Narrow subscopes and existing impact counters will distinguish initial impact,
ordinary constraint solving, continuous impact search, positions and sleep
before any correction. No timestep, fidelity, filtering or effort is reduced.

## Narrow diagnostic reproduction

The real project performance driver retained both original rigs, scripts,
ordinary scene objects, 60 Hz fixed steps and the public `P` input transition
at step 60. It continued for 600 steps. The uncorrected peak was step 111:
58.503 ms fixed, of which 57.229 ms was continuous impact work. Normal velocity
constraints took 0.115 ms and position constraints 0.029 ms. Sixteen genuine
impact events required 1,669 incident trajectory queries / 2,579 advancement
iterations. Only four queries used temporal fallback (68 samples), so fallback
sampling was not the principal cause. There was no duplicate articulation.

In the first second after `P`, fixed work was 7.717 ms median / 36.847 ms p95 /
58.503 ms maximum; the next second was 4.691 / 25.881 / 34.256 ms. Steps 300–599
were 1.115 / 1.299 / 1.408 ms with no impact queries. The ordinary stripped
one/ten-rig benchmark did not reproduce this partial-to-passive landing path.

The first exact pose/orientation reuse candidate reduced the peak to 33.810 ms;
the query-local identical-geometry contact memo reduced it to 28.370 ms. Neither
changed the observed event/contact/mode/sleep sequence. These were intermediate
measurements, not a declaration that the human failure was fixed. Final correction
and verification are recorded in the sibling follow-up report.
