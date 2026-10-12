# M78 final native Windows functional closure

This is a **case-scoped functional validation** of the final combined source, not comprehensive M78 certification, Windows hardware validation or a performance gate. The existing Windows 10 VM stayed at 4 vCPU / 6 GiB and its existing disk/configuration. Work ran serially after Linux released the CPU slot. Rendering used the established `llvmpipe` setup. No full suite, engine edits, test retuning, commits or pushes were performed here.

`SUMMARY.json` maps actual executions to raw receipts and limitations. `FINAL_BINARY_IDENTITY.json` verifies the final source freeze and exact archived native bytes. The initial `PLAN.md` is retained as the pre-execution plan; this report supersedes its planned counts and scope.

## Executed final-source routes

| Route | Observed result |
|---|---|
| Physics continuation / independent restored publication | 112/0 and 8/0 |
| Balance controller / rigid motion / physical state / joints | 119/0, 10,239 PASS, 64/0, 29/0 |
| Contact lifecycle / impact timing | 1,076/0 and 3,197/0; 89 impact cases; original recorded fallback counters retained |
| Gravity / world-save / lower-level streaming | 70/0, 27/0, 82/0 |
| M77 original step / radial / lifecycle | 15/0, 18/0, 26/0 |
| M77 ordinary standing fresh cold writer/read | 23/0 and 13/0 |
| M77 ordinary recovery fresh cold writer/read | 21/0 and 13/0 |
| M77 new-format corrective cold writer/read | 24/0 and 13/0 |
| M77 legacy `a106…` snapshot, unchanged | **13 checks / 1 failure**, exit 1; preserved rather than reclassified as a pass |
| Mixed original M77 + distant normal motor cold writer/read | 25/0 and 13/0 |
| Mixed actual-body uninterrupted/cold pair | 25/0 and 13/0; independent full physical comparison below |
| Ordinary standing/recovery actual endpoint reads | 13/0 and 13/0 |
| M70 CharacterLab baseline / cold writer/read | 98/0, 10/0 and 14/0; authority/finite-pose scope below |
| Current SignalApplication ordinary project writer/read | 17/0 and 6/0; independent fresh-process restoration |
| Final editor edit/duplicate/undo/Play-Stop/save | Exit 0 |
| Final runtime in moved ordinary package | Exit 0; original 1,720-frame four-scene/save/load/pause/reload harness |

Counts are per fixture, not unique engine capabilities. Passing related fixtures does not promote an entire subsystem to independently audited status.

## Real physical endpoint witnesses

`ACTUAL_ENDPOINT_BY_ENTITY.json` compares uninterrupted and fresh-process native records from the same new-format mixed save. All **13 durable bodies, 7 mapped articulation records and 16 completed contact rows** match exactly after excluding only fresh `bodies[].nativeHandle` values and canonicalizing traversal order. No numerical tolerance or physical field was removed. Raw record inequality remains preserved. The independently derived actual endpoint is physically standing: upright 0.9986021519, support-relative root height 1.3618701696 m, relative COM speed 7.13618e-7 m/s, two supported feet. No Linux/Windows bitwise trajectory parity is asserted.

`ORDINARY_ACTUAL_ENDPOINTS.json` derives current physical standing from real body transforms, sampled gravity and completed contacts after fresh-process reads of the ordinary snapshots. Standing: upright 0.9999289513, height 1.4182453156 m, relative COM speed 2.94671e-6 m/s. Recovery: upright 0.9999911785, height 1.2495478392 m, relative COM speed 9.93501e-8 m/s. Both have two supported feet. These reads do not assert uninterrupted trajectory equality.

CharacterLab's existing cold-read proves restored authority and finite pose; it is **not** a full M70 articulated trajectory-equality oracle. The 112-check continuation fixture separately proves exact actual moving static-anchor pose/linear/angular velocity round-trip and actual nonzero-offset motor root/proxy geometry, enabled state, layers, tags and queries before the first callback. The distinct scopes remain explicit in `SUMMARY.json`.

## Exact native engine identity

The unchanged frozen archive is `71a722dded64cfcc634d679e55ddbd257058da888ada914f7f7150b8d88c2d62` (67,019,246 bytes). It does not itself contain the last source updates. Every final link places the freshly compiled `PhysicsWorld.obj` (`6d70c3f9ff8c932ddc8bac2b0a7ab47539813d95e0ea17d7e000e77465702ed1`) and `WorldPersistence.obj` (`1890d0fd27038ddf3e2c67290dc33bdcb2ef499a02543da88480dadfd0b3eabb`) exactly once before that archive. `/VERBOSE:LIB` proves the replaced members were not extracted. Remaining successful links omitted only diagnostic `/MAP` output under an explicit storage authorization; compile/link semantics stayed unchanged.

Final runtime SHA-256: `46b5c697c740c651250c35d0a5a73cabdbae3c0209663f06d88927d4c5a282b9`.

Final editor SHA-256: `339eecca6e058e11f65c0cef0021c26701277e7fe5c1386938074be70c4355bc`.

Final continuation112 executable SHA-256: `6d6281f1ec3c8678512c7386aadfc3b2e37df4377fd0a80cca6423ad8b6ea5aa`.

All 62 generated object/executable receipt rows have losslessly archived, hash/size-verified host bytes under the ignored `.cache/m78-closure/native-binaries/` directory. Exact archive, source, input object and executable manifests are linked from `SUMMARY.json`.

The requested `WorldPersistence.cpp` local shadow warning disappeared. Two old C4244 conversions remain unchanged. Native raw COFF bytes differ after the identifier rename, while raw `.text` sections match; this is not falsely reported as identical native binaries. See `WP_WARNING_REVIEW.json`.

## Preserved failures and setup corrections

Initial disk-full links, the original duplicated response entries, conservative preflight rejections, a missing observer include, metadata BOM decoding and archival-tool failures remain distinct from runtime results. Only explicitly named generated duplicates were retired after lossless host SHA/size verification. Receipts preserve the exact paths and archived bytes. No original archive, historical project, source fixture, package or VM configuration was removed. Per-file NTFS compression was limited to 15 newly generated owned editor objects and retained their exact logical bytes.

The first signals test actually failed (write 10/1, read 4/3). Its guest `lab.js` was an older 2,827-byte copy with SHA `baacc8be0249c6fa91740c6e3bdd3cea040eecc4c0090b0af6e2212586eb642f`, missing the already-accepted validity guard. The driver was the exact current source. The old guest project and failures were preserved unchanged. A **new** byte-verified 17-file current project (776,296 bytes; `lab.js` SHA `2dd4516e6b56b20f0ed9a433c9cfe4fad2757a72aa1ea54f8591d513d2ceacc2`) passed 17/0 + 6/0 using the same compiled binary, with no engine/test change. See `CURRENT_SIGNALS_INPUTS.json` and the paired old/current receipts.

The final moved-package smoke used 45 byte-verified files from the existing ordinary export, replacing only its runtime in a **new** package tree. It launched without arguments from `C:\Windows\Temp`, with an unavailable engine root. This demonstrates the new runtime's package operation; it is not a fresh invocation of the export generator. The original 4 MiB storage preflight rejection is preserved; a separately approved 2 MiB diagnostic storage reserve enabled the new V2 launch. No simulation quality, harness or acceptance threshold changed.

## Remaining native coverage

- Final-source SaveStreaming **Application** write/read/read-b was not executed in this closure. Lower-level streaming/save passes and the actual signals region/save route are useful separate contracts, not substitutes.
- Three new Linux lifecycle audit observers (navigation input race, resolved prefab mod policy, streaming motor publication) and the tiny save-exclusion probe were not newly exercised natively.
- A fresh final-source native export-generator invocation and full M70 articulated trajectory equality remain outside the demonstrated scope.
- Earlier native matrix/export receipts remain prior-source evidence; they were not relabeled as fresh executions.
- The unchanged legacy `a106…` snapshot cannot recover missing continuation history. New-format saves pass; no retroactive reconstruction is claimed.
- FLU-001 and its original settling check/evidence are unchanged and deferred. No fluid implementation, threshold or protected research was changed.
- Windows VM timings remain diagnostic only. No performance work or physical-GPU acceptance occurred here.

The demonstrated corrected native contracts are ready to accompany the combined candidate for operator review. These results do not close the remaining comprehensive M78 capability audit or replace human acceptance.
