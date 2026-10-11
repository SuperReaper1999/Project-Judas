# M77 performance and numerical limits

## CURRENT — final foot-choice correction

The final generic .1% load tie band, portable profiler test and unchanged
60 Hz simulation have been rebuilt and remeasured on Linux. All active
benchmark actors finished physically standing; all four independent actors
succeeded in the four-rig trial. Linear interpolation defines p95 in this pass.



| Mode | Bodies | Fixed median / p95 / max ms | Controller median / p95 / max ms | Frame median / p95 / max ms |
|---|---:|---|---|---|
| perf-zero | 5 | 0.2783 / 0.3554 / 0.4378 | — | 6.5874 / 7.8162 / 679.0220 |
| perf-passive | 33 | 0.6467 / 0.7580 / 1.0268 | — | 7.2822 / 8.0851 / 681.3520 |
| perf-one | 12 | 0.3631 / 0.4507 / 1.0843 | 0.0133 / 0.0205 / 0.1186 | 6.7213 / 7.5880 / 714.3434 |
| perf-four | 33 | 0.5297 / 0.6155 / 1.1210 | 0.0329 / 0.0506 / 0.1705 | 7.0642 / 7.7990 / 711.3250 |
| perf-activation | 12 | 0.3556 / 0.4322 / 0.8721 | 0.0140 / 0.0244 / 0.0676 | 6.7576 / 7.5564 / 710.1831 |
| perf-step | 12 | 0.3763 / 0.5129 / 2.0981 | 0.0166 / 0.0289 / 0.0961 | 6.8651 / 7.8371 / 716.3455 |
| perf-recovery | 13 | 0.2994 / 0.3874 / 0.6104 | 0.0130 / 0.0277 / 0.0773 | 8.2918 / 9.4991 / 709.6951 |

Raw final samples, all scope decompositions, first-60-excluded statistics and
largest outliers are in `step-choice-followup/`. The ~680–716 ms first-draw
spikes remain visible; no average-only acceptance claim.

## CURRENT — final real-time Application

Same 1500-frame real-time harness on the final binary, with P/B/J controls.
Last 120 retained frame intervals: median 12.797 ms,
p95 13.703 ms, max 14.134 ms;
mean interval rate 77.84 FPS. Fixed scope median
0.3829 ms, p95 0.4883 ms,
max 1.2056 ms. Retained discarded time/cap hits are
0 / 0; all profiler diagnostics are zero.
This proves only the retained window, not every earlier interval.
Cold startup / first draw / first task activation spike snapshots are
170.9 / 825.5 / 57.9 ms; full decompositions retained.
No native controller-dominated hitch is demonstrated; cold presentation stalls
remain a limitation. See `realtime-followup/`. The first follow-up application
invocation used an unrecognized profile-output variable and only supplies an
application smoke log; performance comes from the separate correctly enabled
`JUDAS_PROFILE=1 JUDAS_PROFILE_OUTPUT=...` run.

## HISTORICAL — earlier candidate capture, before the load-tie correction

Release, Intel i7-4790 development host, actual 800×450 GL submission, 60 Hz fixed
simulation. Host and Windows heavy work were serialized. Raw samples and top
outliers are retained in `performance/`; the real-time Application capture is in
`realtime/`. Figures are measurements, not universal performance promises.

| Mode | Allocated bodies | Fixed median / p95 / max ms | Controller median / p95 / max ms | Frame median / p95 / max ms |
|---|---:|---|---|---|
| Zero active tasks | 5 | .2863 / .4240 / 1.1449 | — | 6.7838 / 9.0384 / 874.3475 |
| Four passive rigs | 33 | .6622 / .9223 / 1.4079 | — | 7.5816 / 10.0311 / 672.5923 |
| One standing rig | 12 | .3564 / .5517 / 1.1109 | .01263 / .02209 / .10977 | 6.9689 / 9.1241 / 746.2695 |
| Four standing rigs | 33 | .5863 / .9207 / 1.5566 | .03651 / .06205 / .23136 | 9.1844 / 12.7866 / 705.8023 |
| Activation | 12 | .3560 / .4767 / 1.4301 | .01254 / .02258 / .08028 | 6.8888 / 8.4342 / 755.2042 |
| Corrective step | 12 | .4006 / .6354 / 1.4706 | .01840 / .03547 / .09764 | 7.2033 / 9.1562 / 893.7001 |
| Recovery | 13 | .3183 / .4115 / .5550 | .01403 / .02791 / .07628 | 8.4911 / 9.6957 / 708.3266 |

600 samples per case, 1080 for recovery; first-60-excluded statistics and top-five
outliers are also retained. Counts include disabled massless motor proxy bodies.
The private four-rig performance fixture gives the shipped frictionless negative
actor normal authored grip; its physical settings otherwise match the lab.
Passive fallen versus upright active rigs have different contact workloads and
are not a controlled claim that the controller speeds up physics.

## Real-time application

1500 rendered wall-clock-driven frames, P/B activation and J perturbation.
The profiler retains the last 120 frames and three spike snapshots: median frame
interval 12.9646 ms, p95 14.6289 ms, max 16.7245 ms; mean interval rate 76.35 FPS.
Fixed scope median .3862 ms, p95 .4671 ms, max .6114 ms. Retained frames report
no discarded fixed time or cap hit, and all profiler overflow/incomplete counters
are zero. This does not establish that every earlier frame was cap-free.
CSV `frame/60` is a synthetic harness timeline, not wall-clock performance.

Cold startup took 297 ms; the first draw took 1089 ms (873 ms world rendering,
209 ms UI). First standing activation had a 74.6 ms frame, dominated by render/UI
submission (58.9 / 14.6 ms), not controller preparation. These visible one-time
stalls remain disclosed; their exact driver/text-resource causes were not proven.
Steady interactive figures must not be used to conceal the cold maxima above.

These full performance captures precede the later SaveService participant
whitelist correction and the Windows profiler Lane constructor correction.
At that earlier capture, physical controller, physics, contact and render-loop
algorithms matched the then-current candidate. The later load-tie correction
is measured above; these original captures are preserved as historical evidence.
The constructor preserves buffer bounds and all default initialized values;
its affected profiler/native regressions and final-binary smoke are recorded
separately. No simulation-quality or cadence reduction was made.

## Numerical restriction

In free flight, paired actuator reactions pass the independent 1e-5 tolerance.
The whole seven-body articulation's linear residual is <3e-6 kg m/s over 240
60 Hz steps. Angular momentum residual is .03706 kg m²/s with effort off and
.04081 with effort on. The inherited solver has no gyroscopic transport and
includes constraint corrections: M77 does not claim exact global conservation.
The original stricter failed expectation remains preserved.
