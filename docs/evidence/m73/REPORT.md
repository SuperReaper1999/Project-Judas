# M73 candidate — focused execution report

Base: `f1609af0dac863257a0f200cb74ea8a504f9e644`. Candidate is uncommitted.
Linux operator review is pending. Existing M72 Linux acceptance, M67 deferred review
and Windows hardware acceptance retain their previous scope.

## Design and adoption

See `docs/M73.md` and `docs/judasjs/signals.md`. A lazy, world-local ScriptSystem
store owns indexed, slot-generation subscriptions and two bounded FIFO lanes.
`signals.subscribe/unsubscribe/emit/send/stats` and `onSignal` are the only additions.
Fixed delivery follows completed contact/fracture callbacks; explicit UI delivery
follows UI input/callbacks before gameplay. Drains freeze eligible work, recheck exact
recipient tokens, and defer new emissions. No change to physics integration,
existing engine observations, save schema, or legacy consumers is required.

Signals Lab uses ordinary project scripts, UI, a prefab and a streamed annex.
Counter receivers change real M72 material overrides, then publish UI receipts.
Two authored slots share the left entity without sharing behaviour objects. Restore
registers again without resetting facts. The service neither loads regions nor
provides durable notifications. The lab deliberately retains old HUD receipts as
facts when a receiver disappears; repeated prefab labels share a display row while
instances remain independent.

## Linux final checks

Affected incremental Release targets built successfully. The final incremental
builds have no compiler warnings/errors. Initial affected compilation also exposed
two existing RuntimeWorld misleading-indentation warnings, retained in development
logs; unrelated code was not reformatted to suppress them. No full production suite
was run for this bounded script/scheduler addition.

| Actual executed path | Result |
|---|---|
| `judas_signal_tests`, actual QuickJS | 95 checks, zero failures |
| Existing `judas_script_tests` | 35 checks, zero failures |
| Existing `judas_runtime_ui_tests` | 22 checks, zero failures |
| Existing real application `judas_streaming_revisit_tests` | 7 checks, two complete forward/backward traversals |
| Signals Lab shipping application boundary | 17 checks, zero failures |
| Separate-process modern save/load | 6 checks, zero failures |
| TypeScript 5.9.3 / API drift / actual VM enumeration | 29 exports, 336 public symbols, 233 bridge operations, 16 callbacks; 38 example type checks and 3 deliberate drift negatives pass |
| Editor normal Play/Stop | Authored scene IDENTICAL; zero failed resources/jobs |
| Normal export | 6 assets, 2 scenes |
| Moved `/tmp/judas-m73-signals-moved`, working directory `/tmp` | Actual runtime pause/resume/reload smoke plus 17 application and 6 fresh-load checks, zero failures |

Focused cases include copied/malformed payloads without getter/proxy hooks, deterministic
slot ordering, no listener, cancellation/re-subscription, disabled/replaced generations,
listener destruction of a later recipient, emitter destruction, foreign slot/world
rejection, ordinary fault/runaway interruption, bounded chains/capacities, queued-work
save capture and no new-VM replay. Actual application frames include multiple fixed
steps, a zero-step frame, paused UI delivery and modal closure suppression. The UI
signal cannot use the fixed-only CharacterMotor velocity setter. Normal streaming
adoption preserves live subscriptions; unload/revisit registers fresh instances.

## Measurements and bounded retention

`linux/signals-final.log` records a modest headless fixture with one null-payload
broadcast per step and 50 M56 profiler samples per workload. It separates acceptance,
delivery routing/envelope preparation and JS handler time. These are sanity numbers
on the current Linux machine during concurrent VM compilation, not universal
benchmarks. Empty drains were below 0.02 microseconds in this fixture; the zero-use
case is below useful timer resolution. No per-entity signal scan exists.

| Linux subscribers | Acceptance | Routing, excluding handlers | Handlers |
|---|---:|---:|---:|
| 32 | 13.698 µs | 67.298 µs | 3.490 µs |
| 128 | 124.205 µs | 337.097 µs | 17.812 µs |

A byte-cap run accepts 65 messages / 65 recipient IDs / 1,044,875 accounted bytes,
then rejects atomically. Every accepted message is delivered once. 500 publish/cancel
cycles without a drain leave zero subscriptions/events/bytes/recipients and 500 skipped
reservations. 128 listeners / three messages defer after 256 attempts and finish FIFO
at the next boundary. Queue accounting is not allocator RSS: the empty store and
container capacities can remain allocated until world teardown.

Allocation structure is explicit: one bounded serialized snapshot and native recipient
list per accepted message, then a fresh parsed payload/envelope per receiver. Indexes
hold native keys/strings, not JS closures or retained Entity wrappers. The tests measure
retained counts and CPU; they do not claim a total allocator census. Payload-dependent
QuickJS allocation and allocator overhead are excluded from the byte-accounting promise.

## Preserved failures and corrections

`development/` retains the first build/tooling failures, first signal run and application
failures. First signal run: 54 checks / 5 failures — stale sender wrapper fixed to null
while retaining full sender identity; reused fixture metadata paths changed to unique
roots. Scene authoring initially omitted required fluid/render fields; generated content
now follows the real parser. Material assertion now checks runtime overrides. Adoption
proof waits for normal independent startup and uses a separate region entity for revisit:
adoption intentionally removes the original local mapping. Fresh-load assertions also
wait for restored slots to start independently, rather than invent a global start barrier.
The corrected runs above supersede these receipts without deleting them.
A final lifetime review also reproduced an unsubscribe guard failure in an actual
entity destroy callback (`development/teardown-unsubscribe-before.log`): three
application assertions failed. The narrow correction permits an owning teardown to
unsubscribe after the entity has gone; creation/emission remains forbidden. The same
application reproduction now passes all 17 checks, and the self-destruction case is
covered in the final 95-check VM run.

Initial Windows transfer was attempted before its VM-only HTTP server was listening;
no source was transferred by that attempt. The server was started and transfer retried.
Its guest status response is no longer retrievable; this note records the limitation
rather than constructing a synthetic failure log. The VM was later shut down by another agent at the operator's accidental request;
this interrupted validation, not an engine crash. After restarting it, source hashes
matched but an interrupted incremental build retained a 92-check executable. The
final two affected translation units were explicitly touched/rebuilt; this discrepancy
is retained rather than counting the stale executable as final evidence.

## Native Windows 10 VM checks

VS2022 x64 Release build of runtime/editor/SDK and both focused test executables
passes. Explicit final rebuild: 95 signal checks and 35 existing script checks,
zero failures. At that candidate boundary, 26 transferred source/project/example files matched Linux SHA-256
identities (`windows/m73-final-source-proof.json`). Existing MSVC conversion,
uninitialized-local and existing application/header warnings remain recorded;
no unrelated warning cleanup was included. The dependency CMake notices are preserved.

Final native desktop save (17 checks), separate-process load (6 checks), normal
editor Play/Stop (authored scene IDENTICAL, no failed resources/jobs), export
(6 assets, 2 scenes) and moved-runtime pause/resume/reload all pass. Final receipts
are under `windows/`.
These are synthetic native Windows execution, not Windows hardware/input acceptance.
The established VM uses app-local Mesa llvmpipe DLLs; those additions are explicit
VM test setup and are not inserted into normal exports. The final moved path includes
spaces and `é α`, launched from an unrelated directory. No Linux/Wine substitute is
counted as Windows evidence.

## Human repeated-delete follow-up

The desktop process stayed alive, but a second D press threw `ReferenceError`:
`world.entity('30')` creates a wrapper even when the receiver has been destroyed.
Optional chaining therefore called non-idempotent `destroy()` on that stale wrapper.
The publisher's normal fault isolation stopped its lab controls/HUD; no engine
process crash occurred in the human session. The journal is retained in development.

Content-only correction: test `receiver?.valid` before destroying it. No engine/API,
signal ordering, physics or save semantics changed. The application fixture now
presses D twice and exits safely when a script faults rather than assuming captured
state exists. Its first reproduction exposed that test-harness assumption (a null
JSON conversion aborted the test executable); that receipt is preserved separately.
The corrected fault-aware before run reports 10 checks / 1 failure. After: 17 / 0,
fresh-process restore 6 / 0, rebuilt 6-asset/2-scene export and a moved shipping
runtime repeat D/B/pause/resume/reload sequence pass. No full suite was repeated.

The same content correction passes in the existing native Windows moved runtime
with the identical script hash, no script fault and exit 0 (`windows/m73-repeated-delete*`).
The earlier Windows 95/35/17/6 receipts retain their pre-content-fix scope; the revised
application test fixture was rebuilt on Linux only. Windows runtime binaries are
unchanged. Final source/package hashes are refreshed for this accepted-for-retest
candidate; human re-test remains pending. Original Linux package/user saves remain
preserved at `/tmp/judas-m73-signals-before-delete-fix`.

## Workspace hygiene and limits

Showcase contents are preserved at repository-root `video_work/`, narrowly ignored;
`.cache/judas-promo` remains a compatibility symlink. Existing user Videos deliverables
remain untouched. No unrelated tracked showcase source diffs existed. `asset_packs/`
was untracked but was **not actually ignored** at entry; a local `.git/info/exclude`
entry now ignores that exact folder, without moving/changing its contents or adding
it to the candidate. Both `git ls-files` queries are empty.

Messages are transient, project-local plain data. No automatic observation rebroadcast,
networking, promise scheduler, durable delivery, signal assets or editor wiring UI.
Admission is not guaranteed eventual callback: cancellation/retirement can skip it.
Limits bound work counts, not arbitrary user-handler wall time; ordinary QuickJS
interrupts still apply. Hardware/input-feel and final visual acceptance are operator-owned.
No M74 work, commit, push or tag. Protected research, historical evidence and Claude's
consumer work are unchanged.

## Human review (one checklist)

1. B broadcasts; T changes only the left pair; watch counters/materials.
2. U unsubscribes/re-subscribes the left; D removes the right; P spawns independently.
3. Escape pauses. Activate **UI preview** with mouse or arrows/Enter: preview changes,
   fixed counters wait. Resume: queued fixed responses arrive.
4. L loads/unloads/revisits annex. F6 saves; close/restart and F7 restores counters.
5. Stop/Play resets the authored lab. Repeat in the moved package.
