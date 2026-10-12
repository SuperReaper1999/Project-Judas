# M77 cold continuation: retained failure and bounded isolation

This follow-up does **not** establish a production repair for the in-progress
cold-save failure. It preserves the unchanged fixture, input, 60 Hz cadence,
720 resumed steps and standing thresholds. No controller/physics/persistence
production edit follows from these diagnostic overlays.

## Exact reproduction

The byte-preserved original Windows snapshot is
[repro/m77-native-in-progress-step-failed.save](repro/m77-native-in-progress-step-failed.save),
52,557 bytes, SHA-256
`a106ce212416a510ec0470d2cf7abe0fc96fccdfec202c95e5209c9e56c4379b`.
[Reproduction instructions](repro/README.md) identify the original fixture,
verified archive and native executable. A fresh native capture recreated the
**identical** snapshot SHA; fresh and original snapshot reads failed identically.

| Experiment | Outcome |
|---|---|
| Original native cold write | 24 checks / 0 failures |
| Same native live state, uninterrupted next 720 steps | Standing; upright 0.998602, one landing |
| Original native cold read | 13 checks / 1 failure |
| Fresh identical native snapshot read | Same 13 / 1 failure and trajectory |
| Exact original native snapshot read on Linux | 13 / 1 failure |
| Linux-generated snapshot cold read | 13 / 0 |
| Native replay of actual captured angular commands only | Still 13 / 1 |
| Linux normal `Prepare` evaluation instead of initial restore return | Still 13 / 1 |
| Linux identical live capture, one next-step joint/contact warm SEED disabled | Fails physical standing; zero landings |
| Native identical live capture, one next-step joint/contact warm SEED disabled | Fails physical standing; zero landings |

The original native failed endpoint is root `(5.131843, 0.217250, -1.100598)` m,
upright `0.769235`, speed `1.60322` m/s, one landing, with
`corrective step lost support, reach or physical touchdown`. The same snapshot
on Linux fails with root `(5.10148, 0.240301, -1.09076)` m and upright `0.773511`.
This is not a Windows-only crash or save decoding failure.

## Authoritative roundtrip comparison

The all-state observer compares durable entity IDs, not newly allocated native
body slots. All **seven dynamic mapped bodies** in this fixture preserve exactly:

- positions, quaternions, linear/angular velocities;
- mass, centre of mass, world/body inertia;
- shape type/dimensions/children/pivot, filters, physical material;
- enabled/sensor state and force-accumulator hashes.

All six mapped joint settings, parent mappings/rest rotations, authored settings,
and serialized controller semantic-history hash also match exactly. Therefore
there is no evidence here of shape/inertia/text-precision corruption or altered
joint frames causing the failure. The complete world has twelve bodies: seven
mapped dynamics, one static floor and four massless CharacterMotor query shapes.
See [native-cold-state-roundtrip-differences.json](native-cold-state-roundtrip-differences.json)
and [m78-combined-v2-state-isolation.json](m78-combined-v2-state-isolation.json).

Expected native slot reallocation preserves the mapped bodies' relative ordering.
The query shapes initially expose two separate reconstruction seams: disabled
owner 12's capsule is enabled until its next motor update, and three other
capsules initially use motor-origin position rather than its offset centre.
They are `queryOnly`, do not receive rigid contact response, and their first
normal motor update reinstates the proper states. These observations are
recorded, not asserted as the cold failure's cause or repaired in this batch.

## Proven omissions are not a sufficient causal repair

The current save carries `JointSettings` but omits authoritative
`JointAngularDrive` enabled/target/stiffness/damping/effort/rate fields. Restored
joints begin disabled. A separate ordinary two-body Ball-joint experiment
[joint-drive-roundtrip.json](joint-drive-roundtrip.json) proves the missing
first 60 Hz actuator response without skeletons, contacts, gravity or M77.
Disposable angular-drive impulses remain a separate warm history.

The controller also intentionally waits for a completed contact sample after
restore. Its first `Prepare` returns before reference clocks or actuator
commands advance. Native uninterrupted first-step swing time is `0.300000042`
s, while restored first-step time stays `0.283333361` s. Neither a replay of
**actual** previous commands nor bypassing that return repaired the unchanged
cold case. Both failed overlays are retained. The blind bypass conflates an
unpublished contact ledger with a genuinely completed zero-contact result and
is not proposed as a safe production policy.

There is no root-force/torque actuator in `PhysicalBalance::Prepare`: source
inspection rules out a separate lost root command. It writes joint angular
commands only.

## Warm initial-guess sensitivity

A separate owned overlay disables only the **next** initial solve's warm seeds:
`JointSolver::Prepare(..., warm=false)` and zero warm inputs to `AddContact`.
It keeps actual geometry cache eligibility, completed observations, current
body state, current control commands, clocks and input unchanged. It never
serializes/restores old solver impulses or invents contact loads.

For the two Linux continuations, saved bytes and complete pre-step physical
state are identical (snapshot SHA
`54d8cd220055df34d39a4028c106f52352f1032885d298177352f7b4b7784437`,
retained as `repro/m77-linux-warm-seed-source.save`). Untouched continuation
stands with one landing; disabling only the next warm seeds fails with zero
landings. The first initial solve contains **zero contact rows**, narrowing
this discrimination to joint initial-guess sensitivity.

Passive projected impulse residuals after ten iterations are maximum/RMS
`0.0490884 / 0.0171309` with warm seeds and `0.0311624 / 0.00976333` without.
These aggregate linear and angular rows (Ns and Nms) and are not a universal
physical-error norm. The cold numerical residual is smaller in this comparison;
there is no evidence-supported argument here for simply raising the iteration
budget. The final Linux warm-disabled root is
`(4.44635, 0.262071, 2.04266)` m, upright `0.97722`, zero landings, with the same
physical-touchdown/reach failure reason.

The native experiment confirms the same mechanism with the **exact original**
save SHA `a106ce…4379b`: warm ON/OFF pre-step physical-state and actual-command
sidecars are also byte-identical. Warm ON stands with one landing; single-step
warm OFF fails at root `(4.6606, 0.356535, 1.84911)` m, upright `0.976438`, speed
`1.47167` m/s and zero landings. Initial contact rows are zero in all ten passes.
The final projected joint maximum/RMS is `0.0491163 / 0.0171268` with seeds and
`0.0312263 / 0.00977123` without; again the smaller residual is not sufficient
trajectory proof. The diagnostic production-base archive is unchanged; only
owned PW/JointSolver observer objects are substituted. See
[m78-diagnostic-warm-v1-isolation.json](m78-diagnostic-warm-v1-isolation.json).

The diagnostic outcome is distinct from the original write fixture's 24 / 0
result: the extra uninterrupted continuation is an observer, not a changed
acceptance assertion.

## Conclusions and boundaries

- The combined candidate passes the original corrective-step unit and ordinary
  application scenario, but its native in-progress cold-save scenario remains
  a retained failure.
- Physical and semantic save data are preserved exactly; actuator omission and
  first-sample inhibition are real seams, but neither isolated change repairs it.
- The live task is sensitive to dropping a single joint warm initial guess.
  That does not justify persisting disposable caches or changing gains,
  thresholds, input, timestep or mesh/contact fidelity.
- No small generic cold continuation correction has been established by these
  experiments. The retained failure must remain visible in final delivery.

Raw command/compile identities and traces:
[m78-combined-v2-drive-isolation.json](m78-combined-v2-drive-isolation.json),
[cold-wait-isolation.json](cold-wait-isolation.json),
[warm-seed-isolation.json](warm-seed-isolation.json),
[m78-diagnostic-warm-v1-isolation.json](m78-diagnostic-warm-v1-isolation.json),
[probes/prepare_cold_state_observer.py](probes/prepare_cold_state_observer.py),
[probes/prepare_warm_seed_observer.py](probes/prepare_warm_seed_observer.py).
The original production/test files, original failed save and protected historical
work are not rewritten by these owned probes.
