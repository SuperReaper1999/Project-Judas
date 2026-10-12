# M78 cold-boundary investigation and combined handoff

This directory records the continuation after the frozen
[midpoint/contact candidate](../m78-continuation/). Starting HEAD is
`9e77b2226edcf217ab789fe4f0fea42a3415a72d`; the working candidate is uncommitted
and unstaged. Original tests, snapshots, inputs, deadlines and solver budgets
are retained. This is an investigation record, not a claim of universal physical
balance or exact cross-platform numerical replay.

## Read first

1. [Current report](../../M78_COLD_CONTINUATION.md).
2. [Causal investigation and repair](COLD_CONTINUATION_REPAIR.md).
3. [Independent source/evidence review](independent-review.md).
   The narrow M70 static-anchor and motor-query follow-up is documented in
   [Static anchors and motor query reconstruction](STATIC_AND_MOTOR_COLD_RECONSTRUCTION.md).
4. [Final affected Linux checks](final-after-static/LINUX_MATRIX.json),
   [editor/export smoke](final-after-static-smokes/RESULTS.json), and
   [actual current standing](final-current-standing/README.md).
   [Native Windows final report](WINDOWS_FINAL_V2_REPORT.md) and
   [final matrix](WINDOWS_FINAL_V2_MATRIX.json) separately identify retained
   core checks and fresh affected reruns against the final archive.
5. [Exact changed-file list](CHANGED_FILES.txt), [final fingerprints](FINAL_FINGERPRINTS.json)
   and [verification receipt](FINAL_VERIFICATION.json).

## Original failure and numerical experiments

The original native in-progress save is
[`m77-native-in-progress-step-failed.save`](../m78-continuation/repro/m77-native-in-progress-step-failed.save).
Its SHA-256 is
`a106ce212416a510ec0470d2cf7abe0fc96fccdfec202c95e5209c9e56c4379b`.
It remains a **13-check / 1-failure** witness. The format did not capture actual
latched commands, selected numerical continuation or a completed observation.
Nothing is fabricated into its absent fields. The separately captured original
Linux failure remains in the preceding evidence directory too.

The causal report indexes the first divergent preparation/solve boundary,
independent bounded-QP analysis, discarded command-only/waiting-bypass/IK
experiments, passive-only new-capture failure, drive-target deadband and actual
contact-publication proof. Copied diagnostic sources live under `probes/`;
production application is recorded in [SOURCE_APPLICATION.json](SOURCE_APPLICATION.json).
Diagnostic success is distinguished from actual-source combined validation.

The exact public native observation comparison is
[reviewed-first-returned-contact-sample-parity.json](reviewed-first-returned-contact-sample-parity.json).
It compares durable body identities and both directions of a real solved
pad/pad contact; it does not invent known-zero support.

## Combined validation and preservation

`final-combined/BUILD.json` records the initial affected Linux Release compilation
and exact unchanged fixture relinks. `final-combined/LINUX_MATRIX.json` retains
all executions, including the subsequently investigated M70 cold-read rejection.
`final-smokes/RESULTS.json` records navigation, editor Play/Stop, export,
scene/save/reload/pause commands and launch from an unrelated `/tmp` directory.
The final narrow static/motor reconstruction receives its own `final-after-static/`
source compilation and affected matrix, plus `final-after-static-smokes/`;
these do not replace the genuine initial M70 failure. The current-state observer
in `final-current-standing/` confirms genuine physical success independently of
a cached application observation.

The original 600-file audit, 439-file first follow-up and 2,505-file prior combined
manifests remain frozen. `PREVIOUS_MANIFEST_VERIFICATION.json` verified the latter
before applying the new save repair. The final inventory tool rechecks them with
named source/document exceptions, protected tracked history, seven original local
inputs, raw numerical/performance evidence and the unrelated existing report.
Build archives/executables remain in owned ignored cache directories; their
identity is recorded without staging them.

## Reproduction

Normal fresh builds use the repository's existing CMake/dependency instructions;
the new focused target is `judas_physics_continuation_tests`. To repeat unchanged
M77 new-process tests after building `judas_balance_lab_tests`:

```sh
build/judas_balance_lab_tests /tmp/judas-m78-new-cold cold-write
build/judas_balance_lab_tests /tmp/judas-m78-new-cold cold-read
```

Use a new output directory. The writer runs the original disturbance and save
sequence; the reader is a second process. `cold-stand-write`/`cold-stand-read`
and `cold-recovery-write`/`cold-recovery-read` use separate fresh directories.
Original M70 `judas_character_lab_tests` similarly accepts `cold-write`/`cold-read`.
No runtime diagnostic environment flags are needed for the retained correction.

To reproduce the **legacy failure**, copy `projects/balance_lab` into a fresh
`project/` subdirectory, copy the exact original snapshot into `slots/m77.save`,
then run the unchanged `cold-read` mode on that directory. It is expected to
return failure; that expectation must never be counted as a repaired test.
The current combined receipts contain exact absolute commands and executable,
fixture, snapshot and source identities.

`probes/build_final_combined.py` preserves the old archive, compiles only three
affected production translation units in Release and relinks existing unchanged
test objects into an owned archive. It is an incremental validation recipe,
not a substitute for a portable clean build. Owned output directories are
deliberately creation-only to prevent accidental historical overwrites.

## Performance and platform boundary

[Final Linux workload receipts](final-after-static-performance/RESULTS.json) retain the original
four 13-body rigs, 48 joints, 8,192-triangle floor, 60 Hz cadence, full contact path
and CCD requirements. Raw per-step rows are provided, including the remaining
tails and incomplete mesh-rig sleeping. The retained optimization and its paired
isolation measurements are indexed in the preceding continuation report.

[Windows timing record](../m78-windows-tail/REPORT.md) preserves the 56.376 ms
maximum and earlier repeats. The VM is a native **functional** test environment;
its wall timings are not a host performance or physical-GPU certification.
Native final build/test/editor/moved-package receipts in `native-final-narrow-wp/`
are kept separately from prototype and older-archive executions. The earlier
`WINDOWS_FUNCTIONAL_FINAL_MATRIX.json` remains a record of its earlier archive;
it is not silently relabelled as the later static/motor candidate.
The final native matrix is [WINDOWS_FINAL_V2_MATRIX.json](WINDOWS_FINAL_V2_MATRIX.json).

**FLU-001 is unchanged, failed and deferred.** Its original four-second criterion
and proposed later settling criterion remain in [the existing record](../m78-followup/FLU-001.md).
No fluid source, fixture, tolerance, protected research or original acceptance
evidence is edited by this continuation. Human gameplay and Windows hardware
acceptance remain separate from the automated results.
