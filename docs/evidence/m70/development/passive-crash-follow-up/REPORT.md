# M70 consumer and frozen-landing follow-up

The operator's desktop landing freeze supersedes the earlier passive-performance
candidate's handback. Other M70 human-reviewed behaviour remains accepted;
**this corrected landing candidate still requires human re-test**. No commit,
push or tag was made.

## Measured failure and generic correction

The operator pressed G and the articulation locked up on hitting the floor. The
saved process dump is from that unresponsive state; its SIGABRT was externally
sent during force-close, rather than an uncaught engine exception. See
[the original report](HUMAN_FAILURE.md) and retained backtrace/signal records.

The dump shows 39 bodies / 32 dynamic bodies. The first 16-body articulation
processed **6,638 impact events**, **723,397 CCD queries** and **1,203,912 search
iterations in one 1/60-second step**. Its 16 impact counters had all passed the
existing threshold. That threshold only selects an inelastic response; it does
not limit events. Jointed impact response was already inelastic.

The event history advances from 9.147122455e-6 to 1.149967618e-5 seconds, then
continues at increments as small as 3.388131789e-21 seconds. Six bones alternate
between exactly two quaternion representations while their position bits stay
unchanged. Re-normalizing a float quaternion at zero/sub-ULP angular drift creates
that false geometric change, which repeatedly reopens a captured contact. The
repeated pair is bodies 8/12: an ordinary permitted self-contact, not a script
callback or a ground-only exception. [Exact represented poses](core-motion-analysis.json)
retain the numerical evidence.

`RigidMotionSegment::Evaluate` now preserves the stored rotation when the
existing pre-normalization angular update is **bit-identical** to its anchor.
Actual represented angular movement retains the original arithmetic and
normalization. Translation still uses the original arithmetic independently.
There is no angular tolerance, time clamp, skipped contact, event-budget increase,
change of filtering/restitution/constraint iterations, quality reduction or
cadence reduction. The general rigid-body integrator remains unchanged.

`RigidMotion::Evaluate` also uses logarithmic segment lookup, preserving the
**earliest** segment at shared/equal boundaries. This addresses the growing
history lookup cost; it is not used as a substitute for correcting the artificial
contact cycling. The earlier exact-cache/bounds performance reductions remain;
their old bit-identical fixture proof is retained for that preceding byte cohort,
not asserted for this intentional numerical correction.

A replay of the exact post-velocity-solve core state with the old sampler timed
out at 35 seconds after 5,376 events / 473,097 queries; it repeated pair 8/12 near
the same time. A normal public `Step` of the recovered bodies completed, because
it performs an additional velocity solve; that alone did not reproduce/clear the
human failure. The CCD-only numerical replay is explicitly a diagnostic fixture,
not a project/runtime API or an engine test hook.

## Owner-script contact subscription

`entity.ragdoll.receiveContactEvents` is an explicit boolean opt-in, default false.
Authored `ragdoll.receive-contact-events true`, the inspector checkbox and normal
prefab properties use the same field. False omits the optional authored/fingerprint
extension, preserving legacy default fingerprints. Current subscription is
consulted at fixed-boundary dispatch; enabling it does not synthesize an earlier
enter event.

The existing M42 observation/delivery path forwards actual external mapped-body
contacts to the owning entity's script, with safe `selfBody` and stable `selfJoint`.
The bones and other collider need no scripts. Same-owner internal articulation
pairs are not forwarded to the owner; ordinary body callbacks remain unchanged.
Delivery is once per observed body pair/phase, rather than once per manifold
point/CCD solve. It does not introduce an injury system or an unbounded event bus.
A many-contact articulation can still legitimately receive multiple distinct
body-pair events per fixed step.

The preceding M70 candidate already contained owner attribution. This follow-up
makes subscription and its filtering explicit and proves the consumer's actual
**unscripted bone versus unscripted ground** case, rather than claiming a new
solver or treating a scripted-impact probe as sufficient evidence.

## API conventions and documentation

No new vector overload or configuration semantics were added. Shared IK settings,
targets and mode-placement configuration deliberately use position tuples
`[x,y,z]` and quaternion tuples `[x,y,z,w]`, matching their authored parser.
Existing live transforms, physics/motor vectors and legacy limb IK use object
vectors. Shared IK result snapshots expose objects. The reference documents
these boundaries and includes an explicit conversion example.

Legacy limb IK is documented as position/pole based; M70 shared IK supports
position **and orientation** targets. `Up to16` and `at1000` typos are corrected.
Types/source/reference/inventory/live enumeration now cover the contact opt-in:
311 public symbols, 27 exports, 214 native operations, 15 callbacks, 35 examples;
TypeScript 6.0.3 and three negative drift controls pass.

## Platform and validation scope

No Windows environment is available on this host. **Windows M70 moved-package
validation remains outstanding**; the Linux export is not Windows evidence.
The host/tool availability observation is retained in
[windows-availability.json](windows-availability.json).

Only affected motion/contact/joint/physical/lab/API and package checks are rerun.
No full production set, unrelated fluid research or post-M70 feature work is
started. Focused results, numerical replay timings, final package receipts and
fresh source fingerprints accompany the completed corrected candidate.

## Preserved development failures

- Original human freeze and rejected-candidate fingerprints remain intact.
- `owner-contact-lab-first.log`: one test authored replacement entity ID 21,
  which was already used by a generated mapped-body entity. Corrected to ordinary
  runtime ID allocation; stale handle/generation and callback exit proof remains.
- `rigid-motion-first.log`: the old no-spin oracle expected another normalization.
  It now explicitly expects unchanged angular geometry while retaining original
  arithmetic comparison for real represented angular motion.
- The first affected build named a nonexistent JS API target after completing
  the valid targets. The subsequent relink uses the existing cookbook/live
  enumeration target; no runtime or build-system workaround was introduced.

## Corrected numerical replay result

The identical captured CCD state with the corrected sampler completed in
**29.343 ms**, **18 events / 1,684 queries / 320 segments**, with zero event-cap
fallbacks and zero sampling fallbacks. The old 35-second result is a timeout
lower bound, not a completed measurement. [Reproducible compact fixture](replay/README.md)
records all ordinary shapes, mass/inertia, filters, joints and initial support
classification; diagnostic progress logging exists only in a generated temporary
source copy. It is not added to the shipping engine.

Affected results are in [affected-check-results.json](affected-check-results.json):
rigid-motion 8,398, impact timing 3,197, contact lifecycle 1,050, joints 29,
ordinary touch events 26, physical animation 64, actual project/owner contacts 98
and live API surface 9 checks; all exit zero. These counts include individual
float-state oracle comparisons, not thousands of separate gameplay scenarios.
The normal affected build/relink has zero compiler warnings. No full production
run or unrelated fluid/research suite was repeated.

## Current ordinary-project performance and remaining limit

Four sequential 600-step runs use the ordinary imported two-rig project and its
public P/G controls, unchanged 1/60-second cadence and collision/drive settings.
All completed with finite poses and no script diagnostics. These are fixed-step
CPU timings, not desktop FPS or GPU measurements. The numerical correction
intentionally changes the false rotation samples; preceding bit-identical
PhysicsWorld optimization comparisons do not describe this new cohort.

| Current workload | Median / p95 / maximum fixed-step ms |
|---|---:|
| Physical scene, P at step 60 | 1.317 / 8.643 / 34.539 |
| Same passive run, more than two seconds after P | 1.245 / 3.682 / 5.213 |
| Physical scene, G at step 60 | 1.069 / 5.267 / 24.185 |
| Wall scene, G at step 60 | 16.120 / 39.001 / 65.979 |
| Physical scene, P at 60 then G at 300, active interval | 10.587 / 32.136 / 69.272 |

Passive landing still has a transient 34.539 ms fixed step; settled passive work
is substantially smaller. Continuously driven full rigs fighting dense contacts
remain expensive. The wall peak spent 63.071 ms in continuous impacts, with 39
events / 3,851 queries / 207,613 geometric predicates. This is bounded completed
work rather than the reproduced repeated normalization freeze, but it can still
cause noticeable frame spikes. M70 does not provide balance, recovery or a crowd
performance guarantee. That cost has not been hidden with contact suppression,
reduced cadence or quality. Human responsiveness remains to be re-tested.

[Current run summaries](corrected-runtime-summary.json),
[all 600-step timings/counts/body hashes](corrected-runtime-steps.json) and selected
[active peak](active-wall-peak-m56.json)/[settled passive](passive-settled-m56.json)
M56 frames retain the measurements. Large raw states and full M56 captures remain
in `.cache` with [hash receipts](corrected-raw-receipts.json), not committed binaries.
The compact corrected replay recipe was also rebuilt and executed: 29.937 ms,
the same 18 events / 1,684 queries, zero caps or sampling fallbacks. The old
35-second timeout was preserved and not rerun ceremonially.

## Corrected editor, package and desktop handback

The editor used a fresh owned project copy. Duplicate/undo and authored state
after Play/Stop were IDENTICAL; save succeeded and export included eight assets.
[Editor result](editor-crash-corrected-result.json) and log record exit zero.
Offscreen SDL's relative-pointer warnings are expected; this check does not
claim mouse or visual acceptance.

The current Linux package is
`/tmp/judas-m70-character-lab-corrected-20261008`: eight assets, three registered
scenes. Its executable matches the final rebuilt runtime. Only the owned source
copy's Assets/Sources were hidden; the reviewed project is intact. No-argument
launches from an unrelated `/tmp` working directory completed the wall G/landing
and physical save/load/return/spawn/reload action sequences, both exit zero.
[Package results](package-application-results.json) record exact commands and
working directories; process elapsed times are correctness smoke durations,
not frame benchmarks. The rejected package remains untouched for the core replay.

The corrected package was reopened as an ordinary desktop user service, without
test input, offscreen SDL or forced software rendering. It remained running at
the post-launch check, PID 173316; [launch receipt](desktop-reopen-service.json).
Human re-test: G landing in the initial wall scene; F3 then P/G, J and N for
passive/active, impulse and explicit return; reload/Stop normally. Other accepted
M70 behaviours remain in scope, but final acceptance is not claimed.

Fresh final fingerprints cover the corrected source, current documentation,
assets and evidence. Earlier rejected fingerprints/failures are preserved.
HEAD/main/local origin-main remain `934c5d3f0556c920cc7cae8b80dc4677d8cbf87b`;
nothing is staged, committed, pushed or tagged. Protected historical research,
accepted fluid work and unrelated projects remain unchanged.
