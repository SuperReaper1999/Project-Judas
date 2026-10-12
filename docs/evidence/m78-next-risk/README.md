# M78 composed lifecycle, mod ownership and asynchronous replacement audit

Engine baseline: `7f83df131b5051c84e4314fde4508519f78daea4`.
Linux functional acceptance; Windows informational/nonblocking and **not executed
for these new cases**. Production source, existing tests and projects are unchanged.
No new engine correction was required by the observed cases. M78 remains incomplete.

## End-to-end chain

### COMPOSED-STREAM-COLD

Expected behaviour: original available M59/M62 briefs, current save/stream API and
previous committed LIF fixes require persistent qualified identity, suspended
changed state, independent script data and normal physical reconstruction.
Actual path: Application → ordinary project input/QuickJS → SceneSession →
WorldStreaming adoption/suspension → SaveService worker/cold restoration →
WorldPersistence → resumed world → actual queries/physical state.

The unchanged `SaveStreamingTests.cpp` provides real input-driven defeat/spawn,
locale/session state, adoption, joint/object tombstones, changed region props and
independent slots. The added probe retains its timestep, controls and expectations.
A copied ordinary project registers an `AuditMotor` tag and adds a normal script
slot to scene **and prefab** motors; only its start callback adds the public tag.
A save must restore the tag without replaying that callback. Queries use each
actual capsule, independently include/exclude the tag, and isolate other motors.
After cold publication, physical capsule queries work even before script-instance
synchronization. After synchronization, saved `starts:1` is observed in the real
instance. The adopted prop independently restores at `(2,4,-65)` with velocity
`(1,2,3)`, not merely the right persistent key/controller status.

Final physical run: write **21/0**, fresh read **49/0**, fresh read of second slot
**49/0**. Every run is a separate process. Existing generated project/game data
remain ordinary content; no scene-name physics branch or direct JS dispatch.
Missing: every exclusion/participant combination, mapped ragdoll plus this exact
streamed composition, all malformed retained-region records, cancellation races
and exhaustive support/body-generation matrices.

### MOD-APPLICATION-COLD

Expected behaviour: original M76 brief and current mod API: exact trusted loadout,
private publication, normal code/resource/lifecycle ownership and save identity.
Actual path: ordinary `judas_mod` install/enable → Project/ModRuntime session →
AssetDatabase/ResourceManager → QuickJS contributed prefab/signal → CharacterMotor,
contacts, Detour → additive region unload/revisit → scene reload → SaveService.

Two preserved accepted packages (hill-map and landscape-fork) are installed into
new disposable XDG user roots with exact hashes and explicit executable trust.
Their original bytes are retained. The supplied current native Application test
executes public game behaviour, not script-source enumeration. Its selected
registered landscape scene provides actual sculpted collision, navigation and
contacts; the flat hub is a different, deliberately flat scene.

Results: probe **19/0**, write **9/0**, cold read **11/0**, changed-loadout rejection
**7/0**. Save restores real script state without replayed signal and actual motor
position. Disabling landscape-fork rejects the old save with its normal exact
loadout diagnostic and leaves the original world installed. No automatic trust,
substitution or fallback to a different package is claimed.
Missing: arbitrary executable exports/private import aliases, all mod-version
updates, concurrent installer/resource workers, OS interruption/storage faults,
malformed participant matrices and all modded streaming/save combinations.

### MOVED-MOD-PACKAGE

The preserved accepted authoring fixture `.cache/m78-followup/dependency-base-project`
is copied to a new owned root, exported with the current engine, and moved to a
fresh `/tmp` package. It is the matching original base for those exact mod revision
contracts; the unrelated repository modding-lab has different revisions.

Exact mods install/enable in another disposable user store. From unrelated `/tmp`
cwd, the ordinary Application probe passes **19/0**, including actual composed
collision/contact/navigation/revisit/reload. The shipped current runtime then
executes ordinary logical scene switching, E spawn, F6/F7 queued save/load and
return to hub via the existing harness; exit **0**, three scene expectations hold.
This is a moved exported consumer, not a base engine demo path or a test-specific
mod implementation. Software GL is functional evidence; audio device initialization
reported ALSA errors in this offscreen environment. Audible playback, physical input
and Windows package/hardware review remain unverified.

### ASYNC-GENERATIONS-AND-SKELETON

Expected behaviour: current resource architecture and accepted async contracts:
IO/decode on workers; private results; owner-thread GPU publication; cancelled or
old-generation work cannot resurrect resources after replacement/teardown.

Five selected unchanged AsyncApplicationTests cases execute normal Application,
real files/decoders/GL and real worker gates: stale-generation **20/0**, project-switch
**22/0**, project-close **20/0**, missing-recovery **22/0**, shutdown-outstanding
**19/0**. They inspect actual GPU buffers/contents, demand and destruction order;
gates control timing only, not data or answers. Test-relative fixture paths live
under a new owned cwd, preserving historical FTFT test outputs.

The new live skeletal probe uses the existing bend-bar GLB through the commodity
importer and normal asynchronous ResourceManager. Two RuntimeWorld consumers share
one immutable skeleton. A real old decode is held before publication; an equal-
length authored root-name change (`Root` → `Base`) is independently imported from
the copied GLB. The newer asset publishes while old work remains outstanding;
both actual resolved skeletons follow it. Releasing old work records stale discard
and cannot replace the new GPU handle/skeleton. Retained old shared data stay immutable.
Malformed GLB visibly fails, exposes no live old skeleton and recovers when repaired.
World/resource teardown returns fixture residency to zero. **25/0**.

This is not a custom glTF runtime parser or an invented hot-reload API: the only
raw byte edit is a same-length test-fixture name replacement; Judas imports both
versions through its existing parser. Missing: every asset family, all pending
sockets/ragdolls/IK/resource revision combinations, project-switch/multi-VM shared
skeletal consumers, job capacity faults and renderer pixel equality.

## Preserved failed attempts and interpretation

- `stream-fixture-01`: new helper compile failed for a missing Scene header.
- `composed-compile`: new relocated probe compile failed without tests include path.
- `composed-write`: **21/3**. The helper attached the tag script to scenes but not
  the game's runtime-spawned navigator prefab. The uninstrumented prefab correctly
  lacked the new tag/state. Attach the same normal script to the copied prefab;
  no runtime change, expectation or threshold adjustment.
- `composed-v2-read`: **39/2**. BeforeFrame runs before the restored world's first
  ScriptSystem synchronization; Capture enumerates constructed instances, not its
  pending restore map. The physical tag/query checks passed at that boundary.
  The observer now checks script state after that normal synchronization, retaining
  the same starts==1 requirement, while physical checks remain at publication.
  The later original phase already observed starts==1. No settling delay/controller
  retune was inserted. Source trace: Application::beforeFrame, WorldScripts::
  UpdateUIScripts/Synchronize, ScriptSystem::Restore/Capture.
- `mod-probe`: **19/3**. Wrong selected scene: a flat protected hub was compared
  against sculpted-landscape expectations. Selecting the registered landscape
  via the ordinary `.judas` argument passes the unchanged assertions.
- `skeletal-01`: new helper compile failed for a missing JSON header.
- `failures/inventory-01`: identity hashes passed, but the new inventory omitted
  its own filename. The packaging omission was corrected and its first inputs
  retained; no source/binary expectation was changed.

Exact sources/receipts/logs for these failures are preserved. They establish
probe setup/observer problems, not new Linux product defects. Only current final
successful saves are copied as artifacts; earlier intermediate writes to the same
owned save slots were replaced by later test writes, not presented as original
failed snapshots. No original historical failure archive was modified.

## Reproduction and identities

`probes/` contains exact observers, helpers and serial runner phases. `RESULTS.json`
records real command/cwd/environment/exit/log/binary identities and elapsed process
wall times. These times include setup/waits and are **not** a new solver performance
benchmark. Native Linux current archive:
`.cache/m78-lifecycle/lif003-build/libjudas_engine.a`.
Runner inputs and commands reuse committed Release flags/link recipes; unchanged
Application/tool objects are linked to that exact final engine archive. No full
production suite or unrelated research rerun was performed.

Reproduce in a fresh checkout with the equivalent Release archive. The supplied
runner intentionally refuses existing result/log directories. Sequence:
`run_linux.py`; compile/run PrepareStreamFixture with the committed run_driver;
compile the final physical composed observer; run final write/read/read-b;
select registered landscape for mod probe/write/read/mismatch; export/move and
run `run_moved_package.py`; compile/run SkeletalReplacement with software GL.
Historical runner attempts are retained, not a recommended command to overwrite
old outputs. New reproduction paths must be chosen explicitly.

`FINAL_FINGERPRINTS.json` records current source inputs, audit evidence and hash-only
local binaries. `verify.py` checks without regenerating expectations, accepts an
audit-only descendant checkpoint and rejects production-path differences from the
starting engine. `CHANGED_FILES.txt` lists exact new paths. Existing excluded 73
untracked diagnostic entries and frozen historical evidence are outside staging.

See [remaining scope](REMAINING_COVERAGE.md) and the additive coverage overlays.
No production correction is proposed; Linux-passing audit evidence is independently
checkpointable without claiming completion of the whole M78 audit. Human checks
remain pending; Windows is nonblocking and unexecuted here. FLU-001 and independent
fluid research remain unchanged.
