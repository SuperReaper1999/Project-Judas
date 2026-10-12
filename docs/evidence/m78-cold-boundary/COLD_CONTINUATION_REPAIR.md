# M78 articulated cold-save continuation correction

## Scope and status

This is the follow-up investigation of the preserved native Windows M77 in-progress corrective-step snapshot. The original 60 Hz input sequence, controller settings, mesh, scene, gains, thresholds and test assertions were retained. The repair changes the generic save/restore continuation boundary; it does not change the joint equations, ten solver passes, corrective-step policy or fluid implementation.

The reviewed implementation was applied to five source files in [SOURCE_APPLICATION.json](SOURCE_APPLICATION.json). The exact proposed sources had passed the focused Linux proof before application. Root owns the subsequent combined final Linux/native Windows validation and its final receipts. Earlier diagnostic and failed results below remain immutable.

**The old 52,557-byte `a106ce...` save still fails its unchanged cold-read test.** It lacks the newly captured continuation state. No historical impulses, actuator commands or contact publication are fabricated for an old save. New captures use the versioned continuation contract.

## Preserved failure and first causal boundary

The original binary fixture is [m77-native-in-progress-step-failed.save](../m78-continuation/repro/m77-native-in-progress-step-failed.save), SHA-256 `a106ce212416a510ec0470d2cf7abe0fc96fccdfec202c95e5209c9e56c4379b`.

The native original capture passed 24 checks; its fresh-process read passed 13 and failed 1. After 720 unchanged resumed steps, root position was approximately `(5.13184, 0.21725, -1.10060)`, uprightness `0.769235`, speed `1.60322 m/s`, one completed landing, and failed progress. Linux reading the same bytes also failed 13/1. Uninterrupted native continuation of the byte-identical captured state reached supported standing at `(3.07948, 1.36083, 0.34133)`, uprightness `0.998602`, one landing and negligible speed.

A full round trip of the seven dynamic bodies' poses, velocities, masses, inertia, shape/filter/material settings, six joint settings and controller history was exact. The first inconsistency occurred at the next producer/solver boundary:

- The old save restored zero selected joint solve history and disabled angular-drive commands.
- No actual completed-contact publication epoch was restored. The controller correctly treated the sample as unavailable and skipped its whole first preparation phase.
- The uninterrupted first preparation advanced the saved swing time from `0.283333` to `0.300000` and supplied the next drive commands.
- Consequently, the first uninterrupted solve assembled 36 rows, while the old ordinary restored solve assembled 22 passive rows. Fourteen drive rows were absent.

The later failure was an actual touchdown deadline, rather than a stale handle or reach exception: the second swing reached `2.116665 s > 2.1 s`; the correctly identified contacting foot remained `0.696397 m` from its unchanged `0.2 m` landing gate. The existing generic message combined these possible causes, so a copied failure-only observer identified the exact branch.

## Discriminating numerical experiments

All interventions in this section were copied diagnostic implementations, not production settings.

| Intervention | Observation |
|---|---|
| Disable only the first source step's joint warm seeds, leaving physical state, commands, row equations and cadence unchanged | The byte-identical native `a106ce...` capture still existed, but uninterrupted continuation then failed. Its first solve had zero contact rows; its smaller projected residual did not imply a successful physical trajectory. |
| Replay actual captured commands only, with normal first preparation | Failed 13/1 on Linux and native Windows. |
| Replay actual selected seeds only, with normal first preparation | Passed 13/0 on Linux and native Windows, with genuine final supported standing. |
| Replay actual commands and seeds once, under strict incoming-body/settings guards | First 36 row equations, initial seeds, all seven post-solve velocities and first completed body states reproduced the uninterrupted boundary exactly; 13/0. |
| Increase equation accuracy through an independent exact small bounded QP at every solve boundary | Still failed the unchanged physical touchdown criterion. This rejected an iteration-budget increase as a demonstrated correction. |
| Alter coherent IK/reference geometry in isolated copies | Regressed the unchanged controller/application fixtures; rejected. No controller/gain repair was retained. |

The actual row system was reconstructed as `A = J M^-1 J^T + diag(gamma)`, `c = J v_free + bias`, with the real row bounds. The first 36-row warm trajectory had a larger independently measured objective gap than the failing cold trajectory. This establishes numerical continuation sensitivity, not a claim that a less converged solve is generally preferable. The production solve budget remains unchanged.

See [native-first-boundary-comparison.json](native-first-boundary-comparison.json), [first-boundary-closure-comparison.json](first-boundary-closure-comparison.json), [native-split](native-split/), the `linux-all-oracle-*` artifacts, and the preserved copied failure observer.

## Why the retained context is larger than passive seeds

The first old-save seed-only intervention established causality but did not justify a save format by itself. It replayed an actually captured prior state into a specific historical failure. Separate **new capture/read** experiments determined the complete contract:

| Newly captured context | Unchanged read result |
|---|---|
| Twelve passive values per joint only | Failed 13/1, despite exact equality of every restored passive value before the first solve. |
| Passive values plus actual angular-drive command fields, first original waiting phase retained | Failed with and without the three drive seeds. |
| Passive values, actual commands and three actual drive seeds, normal producer preparation restored | Passed 13/0. The first source/read 36-row equations and initial accumulated impulses were exact. Removing drive seeds failed. |
| The same context plus the actual completed PhysicsWorld observation publication, enabling the ordinary first producer phase safely | Passed new capture/read; the unchanged standalone controller history test still waited when no full-world publication was restored. |

The small target difference found during replay was separately explained by the existing angular-drive no-change deadband. The last completed source target differed by `0.000595620 rad` from a newly calculated restored target, with normalized absolute dot `0.99999995565` inside the existing `0.9999999` deadband. Preserving the actual command preserves the existing setter semantics; it does not add a new tolerance. See [angular-command-deadband-observation.json](angular-command-deadband-observation.json) and [new-capture-full-normal-first-equations.json](new-capture-full-normal-first-equations.json).

## Generic implementation

### Existing versioned participants

Articulation version 4 appends a bounded continuation payload for existing authored-joint owner references and existing articulated owner/joint-index references. It records actual passive history, actual angular-drive commands and drive history, and the completed regular interval. Mapping is checked against durable body IDs and current joint/body settings. Assignment is deferred until balance configuration and other physical-owner reconstruction are complete, before persisted sleep is applied.

Contacts version 2 retains its existing event-pair payload and appends actual completed contact-observation publication. Articulation 4 requires contacts 2; malformed, truncated or mismatched new-format context is rejected in the private restore candidate. No participant was added and the existing 17-participant ceiling was not raised. Old articulation versions 1–3 and contacts version 1 keep their original restoration behavior.

### Bounded physical context and invalidation

The native helpers are `PersistJointContinuation`, `PersistCompletedContactObservations` and `HasRestoredCompletedContactObservations`. They add no JudasJS bindings or class-layout changes.

Joint history is tied to the actual completed interval and reconstructed physical settings/body context. Nonfinite values, non-unit stored drive targets, stale/mismatched handles and duplicate/incomplete mappings are rejected. If the next real step changes the interval, seeds are discarded without rescaling while actuator intent remains. Relevant changed mass/inertia, authority, pose, enabled/filter/material state or joint settings invalidate restored history. The restored-only markers expire at the next real step and do not change ordinary live cache behavior.

Completed observations are distinct from contact warm-start caches. The saved readable positive rows retain durable body/primitive identity, point anchors, normals, impulses and separation; current generations/revisions are assigned only after physical-context validation. Remapped slot order is handled explicitly. No observation is applied as a new physical impulse, no collision callback is synthesized, and contact tangent/warm caches remain disposable.

A snapshot which intentionally omits any otherwise readable positive contact is marked **unavailable**, not falsely complete or empty. The existing unavailable-sample fallback then remains active. A complete actual zero-support publication is different from an unpublished sample.

### First resumed controller phase

`PhysicalBalance::Prepare` keeps its original standalone-history waiting behavior. Only a matching-dt, actually restored completed-world publication lets its first resumed producer phase run normally. No guessed contact load or invented prior support state is used.

## Direct sample proof and reviewed-source validation

[reviewed-first-returned-contact-sample-parity.json](reviewed-first-returned-contact-sample-parity.json) compares the actual public native `ReadBodyContactSample` result before save and immediately after restore, before the first resumed preparation. Twelve live body results matched exactly. One real pad/pad self-contact was returned in both directions: point, normal, impulse `0.1966645867`, separation, support velocity and identities all matched bitwise. The fixture's lack of external foot support was not replaced with fabricated support.

The final reviewed owned sources passed:

- Linux focused continuation guards: 40/0 at the reviewed proof boundary.
- Original cold write: 24/0; newly captured original cold read: 13/0.
- Copied public-sample observer: write 24/0/read 13/0, exact returned sample parity.
- Earlier original controller fixture: 119/0.
- Native copied actual-publication proof: write 24/0/read 13/0; controller 119/0.
- The old `a106ce...` snapshot: preserved 13/1.

Root added five further focused guards for the final combined build after this proof. Final actual-source Linux/native results belong to root's final matrices, rather than being inferred from the prototype. Source application fingerprints are recorded in [SOURCE_APPLICATION.json](SOURCE_APPLICATION.json).

## Remaining limits

The old-format snapshot does not contain the missing continuation and remains a genuine reproducible failure. No claim of retroactive exact continuation is made. The new contract preserves selected solver state for matching physical context and regular cadence; it is not a guarantee of bitwise identical future trajectories across arbitrary platforms, changed assets, changed timesteps or edited worlds. Authored terrain function equivalence also relies on the existing scene/content identity, while the native guard records its physical bounds.

No fluid code, M77 fixture, controller gains, acceptance threshold, cadence, unrelated physics system, protected research, original failure artifact or original final manifest was changed by this repair. Nothing has been staged, committed or pushed.
