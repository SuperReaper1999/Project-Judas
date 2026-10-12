# Public save/prefab lifecycle audit

**Linux Release/Application: 55 checks / 0 failures, 23 frames, exit 0.
Native Windows/Application: 55 checks / 0 failures, 28 frames, exit 0.
Human/hardware acceptance and broader coverage remain pending.** Starting accepted checkpoint:
`0695433b949df0c1d05ac6798bf118121cc53e2b`.
This is a scoped continuation of the incomplete M78 audit, not human acceptance
or an engine-wide validation claim. All earlier registers and receipts remain
unchanged.

The new [Application driver](../../../../tests/ConsumerLifecycleAuditTests.cpp)
creates an isolated ordinary project using the current serializers and
AssetDatabase. Registered project scripts call the public `judas` module; they
never invoke `__judas`, private VM evaluation or a demo-specific runtime path.
Native hooks observe actual live world/body/script state and asynchronous service
results. Only a detached test-slot snapshot is edited to construct the deliberate
invalid required-participant input.

## Selected contracts

| Case | Original requirement | Positive oracle | Discriminating negative oracle |
| --- | --- | --- | --- |
| `PUBLIC-SAVE-REJECT` | [M61 §9–10](../../m78/requirements/M61_PROMPT.txt): load-aware script lifecycle, safe private load preparation/publication | Public save/load reconstructs actual body poses/velocities, durable IDs, session marker, typed references and saved slot state; `restore` runs once without replaying `start`. | A checksum-valid detached save with required `motors` participant version 99 fails before publication. The original world address, entity/body/script counts, actual pose/velocity and session/game values remain; callbacks continue and one ordinary fixed step still moves the original dynamic actor. Good and invalid slot bytes are unchanged by rejection. |
| `PUBLIC-PREFAB-INIT` | [M67 §10C](../../m78/requirements/M67_PROMPT.txt): bounded initial properties/state/motion, safe references, independent instances and atomic rejection | Two separate root/child hierarchies sharing one prefab/script asset receive different constructor properties/payload/motion. Authored internal references are remapped, a published external reference remains valid, and each slot starts once. Positive save/load restores these instances without replaying initial payload/start. | A valid first initialization record followed by an invalid second slot fails; a destroyed external entity reference also fails. Exact live entity/body/script counts show neither failed attempt published a partial hierarchy. |

Related frozen IDs: `JS.saves.save`, `JS.saves.load`, `JS.saves.status`,
`JS.saves.reference`, `JS.saves.resolve`, `JS.world.spawnPrefab`, `I.01`, `I.03`,
`A.05`, `H.06`, `ASSET.Prefab`, `ASSET.Script`.
These links select narrowly exercised member contracts. They do not promote the
entire associated mechanism, authored component or asset family.

## Executed Linux route and reproduction

[Compiler/linker/run receipt](linux-after-01/RECEIPTS.json),
[exact compiled source](linux-after-01/compiled-source.cpp) and
[raw assertions](linux-after-01/run.stdout.log) identify the actual executed
driver. It uses final lifecycle engine archive
`e2e4aec5ed13c6a6e5eb4db0f528682671ad879ae27080665699d8b61681618f`
and driver binary
`4d1e063842fd20b3b11d8170be0cbd6fc07aa9e134fc6e0d98efb007fe9dc62a`.
No failed application or public-API assertion occurred in this run; no ceremonial
before-failure is invented for these newly selected contracts.

## Executed native Windows route

The [native account](../native/REPORT.md),
[process/build receipt](../native/m78-lifecycle-consumer_lifecycle-build.json),
[raw result](../native/m78-lifecycle-consumer_lifecycle-RESULT.json) and
[source/object/binary identity](../native/FINAL_BINARY_IDENTITY.json) record
the same driver source with **55/0 in 28 frames**, executable SHA
`8c209bb1cce74f6efb90cb20cd40a44c3f231d0731ec0aa633b4004f053576bc`.
The embedded result scope says Linux because the unchanged driver shares that
text; actual MSVC build/process/binary receipts establish Windows execution.
Slot bytes were archived from the actual KnownFolder save root. Neither native
nor Linux counts are duplicated as separate 55-pass totals for each contract.

Earlier automatic approval review storage blocks remain preserved tooling history;
the operator subsequently authorized the named archive compression and retirement
of independently copied new outputs. No engine pass/failure follows from those
storage attempts. The native functional VM used software GL; this is not physical
GPU, performance, input/audio or human/editor acceptance.

Root owns the fixture/slot archival beside this receipt and the native schedule.
The generated project is original test content; existing projects and historical
evidence are not modified. The 55 checks include fixture/setup and shared
assertions and must not be counted twice as 55 independent passes per contract.

### Reproduction recipe

Root owns the CMake target and incremental Release build. Link the new driver
like `judas_save_application_tests`:

```cmake
add_executable(judas_consumer_lifecycle_audit_tests
  tests/ConsumerLifecycleAuditTests.cpp
  src/Application.cpp src/RuntimeOptions.cpp
  src/RuntimeDiagnostics.cpp src/TestHarness.cpp)
target_link_libraries(judas_consumer_lifecycle_audit_tests PRIVATE judas_engine)
judas_warnings(judas_consumer_lifecycle_audit_tests -Wpedantic)
```

The executable accepts one **fresh** output directory. It generates `project/`,
isolated user-data saves and `RESULT.json`. Preserve stdout/stderr, process exit,
the exact fixture files, slot bytes and source/binary hashes with the receipt.
Do not replace a natural failed result with regenerated expectations.

On Linux the actual user-data root is isolated with `XDG_DATA_HOME`. Windows uses
its KnownFolder LocalAppData path; environment overrides do not relocate that
service. The normal freshly generated project has a new random save identity,
so its native run still writes a unique test namespace. Archive from the actual
`RESULT.json.saveRoot`, not an assumed `fixture/userdata` path.

Most frames use zero simulation dt to independently compare construction/private
publication snapshots; resources, JS updates and normal outer-boundary services
still run. One ordinary 60 Hz fixed step after rejection verifies the old live
dynamic actor remains usable before the valid-load control. This does not claim
uninterrupted trajectory equality.

## Explicit remaining scope

- No arbitrary gameplay-side-effect rollback guarantee after publication or a
  failing `restore` callback. This negative input is a required-participant
  compatibility failure during private preparation.
- No complete corrupt-container/previous-good recovery, cancellation, busy,
  storage-failure, migration, exclusion, streaming or all-participant matrix.
  Legal simple-actor exclusion is corrected and separately exercised by the
  [77-check exclusion fixture](../save-exclusion/after/); this application fixture
  does not establish all excluded-component/reference combinations.
- Prefab external references use already published entities; authored internal
  references use normal remapping. This does not promise that caller-supplied
  original-source IDs inside replacement property JSON are automatically remapped.
- No full schema/JSON bound, nested prefab, every component, dependency fault,
  resource race or partially created native-allocation rollback matrix.
- No independent UI layout/lifetime or shared-IK/root-authority fixture is folded
  into these cases. Those remain separate coverage gaps requiring their own assets
  and discriminating controls.
- Scoped native Windows/Application execution is attached separately above.
  No new native editor/moved package, SaveStreaming Application, signals or
  runtime-tag-mutator probe is attached. Hardware/visual acceptance, human editor
  gestures, performance certification and full-subsystem coverage remain open.

The [new additive overlay](../coverage/README.md) retains every original
capability ID and baseline platform state. The frozen M78/closure tables and
all original failure evidence remain intact.
