# M70 passive-mode performance follow-up

## Human failure and measured cause

The operator reported an immediate framerate collapse on switching the ordinary
Character Lab physical scene to passive. [The original failure](HUMAN_FAILURE.md)
and preceding candidate fingerprints are preserved. This follow-up does not
change the accepted IK, animation, physical drive, project controls or artwork.

The actual two-rig project was reproduced through the public `P` input edge,
starting from its ordinary partial state. It retains 37 live bodies (33 dynamic),
30 joints, both scripts and 60 Hz fixed simulation. The previous stripped
one/ten-rig benchmark started passive from rest and was not an equivalent test.

At the uncorrected peak, sixteen real articulated impact events caused 1,669
incident trajectory queries. Fixed work took **58.503 ms**, including **57.229 ms
continuous impact work**, versus 0.115 ms ordinary velocity constraints and
0.029 ms position constraints. The connected rig must be reconsidered after a
limb impact changes its motion. The implementation repeatedly prepared identical
poses/orientations, repeated exact-input zero-margin contact tests, and searched
pairs whose retained fat broadphase bounds overlapped although their remaining
trajectories could not meet. No duplicate rigs, passive-drive loop or large
fallback-sampling explosion was found (four fallback queries / 68 samples at
the peak). Later settled steps were about 1.1 ms.

## Generic correction

`PhysicsWorld` now reuses derived CCD pose/orientation preparation for identical
represented body/time inputs. Samples are invalidated at the step boundary and
on every trajectory change; generation keys prevent slot reuse. Only live slots
are visited by the added invalidation. Cache storage is included in the existing
geometry-storage diagnostic.

Each trajectory query has a three-entry zero-margin truth memo keyed by both
represented positions and quaternions. The query's shapes/primitive IDs remain
fixed. All 32 bisection updates and all sampling times still execute; identical
geometry need not be evaluated repeatedly. Margin/proximity contacts are never
used as a substitute for actual-contact truth. The repeated child rotation
preparation is also hoisted out of the box-vertex loop with identical arithmetic.

For simple zero-pivot boxes/spheres, an outward remaining-trajectory bound rejects
geometrically impossible pairs before expensive narrowphase. A dynamic body's
represented center lies between its sampled remaining endpoints componentwise;
an outward enclosing sphere at both endpoints covers every orientation throughout
that segment. Static geometry uses its existing outward bound. Motion changes
invalidate the bound; its generation key is separate from the pose key. Nonfinite
or uncertain bounds fall back to the established query. Compound, cooked and
terrain shapes retain their existing query path rather than acquiring an
unproven tighter radius certificate.

These are engine-wide geometry-work reductions. No demo name, body name or
character anatomy is tested. Timestep, CCD iteration/bisection/sampling limits,
contact geometry, impulses, solver policy, collision filters, self-collision,
sleep thresholds, gravity and physical drive effort are unchanged.

## Evidence and qualification

The actual-project driver is `judas_character_lab_performance <output> passive-repro`.
It executes normal frame/fixed/presentation scripts, records existing M56 scopes
and authoritative all-body/mapped-body state, and never measures rendering inside its
fixed-step timer. Intermediate correction measurements were 33.810 ms peak for
pose/orientation reuse and 28.370 ms for the additional identical-geometry memo.
Their contacts/events/modes/sleep matched the preceding run. They were retained
as investigation results, not human acceptance.

CPU timings use Release on the development machine. Hidden software GL captures
establish simulation cost, not measured desktop GPU FPS. Interactive acceptance
of the refreshed passive scene belongs to the operator. No full production or
historical fluid/research rerun is required for this bounded geometry correction.

### Frozen correction and exact outcome comparison

The final candidate also stops a query after an actual-contact miss at the exact
endpoint: the future interval is empty. It does not skip the endpoint test or
change a time bracket. A trial step-local full-manifold memo produced no
meaningful timing gain and was **discarded**, rather than retained as extra
runtime machinery. The final cache is the small pose/orientation cache and
query-local truth memo described above.

The preserved uncached source was rebuilt with the same actual-project driver
and all-body observation fields, then the final candidate was restored
byte-for-byte and rebuilt. Both processes exited zero. Paired Release timings:

| Fixed-step window | Original median / p95 / max (ms) | Corrected median / p95 / max (ms) |
|---|---:|---:|
| First second after passive | 7.855 / 35.084 / 44.031 | 4.462 / 19.939 / 25.824 |
| Second second | 4.664 / 24.853 / 31.844 | 2.925 / 13.791 / 19.260 |
| Settled (steps 300–599) | 1.147 / 1.328 / 2.022 | 1.127 / 1.278 / 1.514 |
| All 540 passive steps | 1.175 / 14.716 / 44.031 | 1.148 / 8.344 / 25.824 |

The peak remained step 111. Continuous-impact cost fell from 42.837 to 24.614 ms;
geometry predicates fell from 114,145 to 78,955. Its 16 impact events, 1,669 queries
and 68 fallback samples are unchanged. The earlier first diagnostic capture's
58.503 ms peak is preserved, but is not substituted for the paired baseline.
Timing varies across runs. Some landing steps still exceed 16.7 ms; these
measurements do not promise uninterrupted 60 FPS or certify large ragdoll crowds.

All **600 steps × 37 live bodies** match binary32 bits for positions, rotations,
linear and angular velocities: 288,600 float observations, including signed zero.
All 32 mapped-body observations also match, as do body/contact/event/query,
sleep/mode/safety counts and fallback/sampling diagnostics. Every pose is finite;
both scripts report zero diagnostics. The independent review found no geometry,
trajectory or generation-lifetime discrepancy. Iteration-work counters decrease
where an impossible pair or empty future interval is now rejected; physical
results and bounded policies do not change.

`exact-outcome-comparison.json`, `independent-review.json`, and the two compact
captures retain every step's counters, scopes and binary-state hashes, plus
initial/activation/final body snapshots. The large lossless raw state/M56 captures
remain receipted under ignored `.cache/m70-review`; selected peak/settled M56
frames are included here. They can be reproduced with the driver, without adding
tens of megabytes of redundant raw observations to the repository.

### Affected verification

The affected Release relink completed with zero warnings. Final executed checks:
impact timing **89 fixtures / 3,197 checks**, contact lifecycle **30 fixtures /
1,050 checks**, joints **29**, collision/trigger events **26**, physical animation
**64**, and actual-project/VM lab **66**, all with zero failures and exit zero.
Existing protected fixture sources/results were not edited. No full production,
fluid or unrelated historical research suite was repeated.

The runtime package is refreshed through the normal M38 exporter. Final package
and application receipts are recorded alongside this report. Human re-test of
passive responsiveness is pending; other lab behaviours retain operator
acceptance. Nothing is staged, committed, pushed or tagged.

The moved ordinary application also passed two affected runs: the initial
F3/P passive capture (500 scripted frames), then impacts/drive control/prefab/
save/load/full/passive/return/reload (450 scripted frames), both exit zero.
The owned export source's Assets/Sources were hidden; the executable was launched
with no arguments from an unrelated `/tmp` directory. The refreshed package
contains eight assets, three scenes, 31 files and **53,040,986 bytes** (50.58 MiB).

The separate rendered **software-GL application** capture retains a **44.107 ms**
landing fixed-step spike, compared with 53.268 ms in the original application
capture. These asynchronous-startup runs have different contact trajectories
and are not used as a paired speedup claim. Their final retained settled-window
medians were 1.184 and 1.222 ms respectively. The controlled all-body comparison
above remains the matching-workload result. Neither synthetic-frame automation
nor headless rendering measures real-time desktop catch-up/FPS; visible passive
responsiveness still requires the operator's re-test. A normal desktop launch
uses unchanged real-time cadence and the actual desktop renderer.

The corrected package was reopened on the normal desktop through a transient
user service (PID/launch receipt in `desktop-reopen-service.json`). The initial
background launcher did not stay alive after its command finished; that launch
receipt is retained, and only the persistent service remains. No other user/
consumer process was stopped. The desktop uses fresh writable runtime state,
normal input and desktop graphics; M56 captures the human run without overriding
cadence. Press **F3**, then **P** to repeat the reported transition. This is a
hand-back for review, not a claim of human acceptance.

Narrow implementation delta from the preceding candidate: `src/PhysicsWorld.cpp`
and `tests/CharacterLabPerformance.cpp`. Current documentation/evidence/receipts
were updated; authored project assets and controls remain byte-identical. The
whole M70 candidate remains uncommitted at the original starting HEAD. Protected
FTFT/P1, earlier milestone evidence, fluid work and Renderer source remain
unchanged.

## Superseded handback: human G/floor freeze

The desktop handback in this report subsequently froze on landing and was
force-closed by the operator. It is not the final accepted candidate. See the
[core replay and consumer follow-up](../passive-crash-follow-up/REPORT.md). The
timings and bit-equivalence proof above remain valid for the earlier exact-cache
correction's fixture, but do not establish the repaired sampler's final outcome.
