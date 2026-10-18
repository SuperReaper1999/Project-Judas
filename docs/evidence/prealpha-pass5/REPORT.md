# Pre-Alpha Pass 5 — physics correctness and performance

## Candidate status

Accepted input and unchanged main: **`4c141e9bb6c68a50c63dec263dcec398200328ad`**.
Work develops and checkpoints on **Experimental only**. The demonstrated runtime
optimization is qualified on Linux and native Windows. The original curved CCD
failures, severe dense-contact tail and liquid point-identity limitation are
**not solved**. This is scoped progress, not Pass5 human acceptance, M100 or Alpha
readiness.

[Every original audit finding](AUDIT_DISPOSITION.md) · [Machine register](FINDINGS.json)
· [Exact changed files](CHANGED_FILES.txt) · [Fingerprints](FINAL_FINGERPRINTS.json)
· [Reproduction](REPRODUCTION.md) · [Verification](FINAL_VERIFICATION.json)

## Implemented and checkpointed

### Exact broadphase candidate reuse

`src/physics/ImpactCandidateCache.h` isolates derived tree candidates for a single
impact-event pass. `WorldImpactEvents.cpp` uses it at two existing discovery
sites. The original sorted fat-proxy candidate superset is retained only while
**every** tree proxy revision remains unchanged. Actual-query filtering uses
current other-proxy bounds; queries outside the retained enclosure or allocation
budget fall back to the original tree query. No manifold, contact truth,
trajectory, sleep/filter decision, TOI or solver result is cached.

Bounds:4096 entries/262144 retained-ID capacity, with bounded filter/map overhead
separate from the ID budget. Observed peak ID capacity is24628 (~96KiB payload).
The helper has17 focused controls, including other-proxy invalidation, fat-only
filter rejection and oversized fallback. Omitting revision or filtering produces
the intended failures. A separate observer compares **981074** actual queries
against the original tree, with **zero set/order mismatches**. Tree traversal
calls fall to143207: **85.4% fewer**, not85.4% faster physics.

The public physics header is byte-identical, the coordinator remains859 lines,
and same-object archive comparison finds exactly one changed engine object,
`WorldImpactEvents.cpp.o`, among170 members. No controller/timestep, iteration
budget, CCD quality, geometry, material, fluid, script API or gameplay rule changed.
CMake and the focused validation manifest register the new private-helper test.
[Implementation and proof](dense/REPORT.md) · [Final production replay](dense-linux/REPORT.md).

Runtime checkpoint: **`be621491a6a1a578ac87e8c82e3b297ed8b18614`**,
`Reuse unchanged impact broadphase candidates`.

### Matched dense performance

Same i7-4790 host, GNU15.2 Release, scene,270 physical steps,60Hz inputs,
1024 loose pieces (1285 total bodies), original collision acceptance and profiling.
Windows VM and other owned heavy jobs were held for timing. Raw before/after
profiles, commands, timestamps, hardware, binaries and endpoints are preserved.

| Declared pair | Baseline median/p95/worst ms | Candidate median/p95/worst ms |
|---|---|---|
|1|25.645787 /361.145185 /1838.943474|25.371454 /322.786498 /1722.174617|
|2|25.367 /363.919 /1838.908|25.581 /326.411 /1957.994|
|3|25.873 /364.067 /1833.120|24.904 /333.756 /1704.814|

Matched p95 is8.3–10.6% lower across all three pairs. Median and maximum are
mixed; the second candidate maximum is worse. There is **no worst-case bound,
60Hz1024-body claim, universal speedup or memory-saving claim**. All270 records /
15 physical fields, ordered counters, final contacts and endpoint geometry match
exactly, including digest `192f0f4bed728953`. Four unchanged scaling controls also
match physical CSVs, endpoint arrays and acceptance fields exactly. A separate
final production replay confirms those invariants; it is not a fourth matched
speedup pair. [Raw measurements](dense/REPORT.md).

Further profiling attributes much residual cost to actual SAT/geometry work in
repeated discovery and response, not merely eight-corner clipping or tree visits.
A guarded native sphere/box manifold-reuse experiment is bit-exact in2653 native
controls and an actual compound physical fixture, but its single matched pair has
7.6% worse median, negligible wall change and mixed small tail differences. It is
**deferred, not installed**. Its initial executable-mode mistake and unpaired run
are excluded explicitly; the corrected wrapper requires every child/completion.
[Subcosts](dense-subcost/REPORT.md) · [Deferred experiment](reciprocal/REPORT.md).

### CI failure notifications: actual cause and repair

Eight hosted runs since Pass3 fail the same packaging prerequisite. The source
check scans only root literal `add_executable` statements after actual tools
moved into reachable CMake modules/helper calls. It falsely reports five missing
tools before configuration/native tests. Actual declarations/builds remain present.

The test now follows reachable local includes and literal existing target helpers,
rejecting inert commented/quoted text and missing local includes. Real missing
helper and orphaned-module negatives still fail the actual inventory assertion.
Original local24/1failure is preserved; corrected26/0 and CMake helper12/0 pass.
All five other exact CI tooling commands pass locally. The workflow now invokes
the **packaged** SDK inspector and retains subsequent strict verification, rather
than testing only the source entrypoint. No gate/notification was disabled.

CI checkpoint: **`050016a179313c90584ce00f141cc5ca4c521808`**,
`Restore CI inventory validation after CMake modularization`.
The original logs are retained in [CI evidence](ci/REPORT.md). **Fresh hosted run39159743311 passes all13 workflow steps**, including the native
build, **38/38 selected CTest gates** and packaged SDK Ubuntu24.04/max-glibc2.39
qualification (highest requiredGLIBC2.38).
Its exact050016 source/build/artifact receipts are in `ci-followup/`; local passes
were not substituted for the observed hosted result. The older initial Pass1 resource-gate failure was a
separate already-corrected prerequisite issue, not another current physics failure.

## Investigated, deliberately not shipped

### Four curved-mesh continuation failures

The original unfiltered fixture remains **372 checks /4 failures** on accepted
baseline, production candidate and native Windows; initial entry372/0 passes.
Curved-only research selects193 checks and is named separately. The original
16-triangle reduction is preserved, not simplified into an infinite plane.

Actual native front contact can continue after the represented body's mean moves
behind a finite triangle. Stateless side selection then loses that contact.
Research must handle real finite departure, welded-edge handoff, disconnected
contact patches, native epsilon sidedness, velocity changes, sleeping and physical
endpoint publication together. Contact ancestry is not interchangeable with a
stateless memo or an always-front triangle normal.

This pass traces exact genuine departure, irrelevant history and genuinely
influential behind-centre states. A later actual native-front witness proves one
fresh acquisition, but its suffix still costs509 certificate nodes. The final
bounded target-directed prototype retains ordinary uncertainty and **three failed
curved checks plus three failed helper positives**. Initial-entry and reduced-case
passes do not make truncated trajectories valid. The prototype is rejected.

No state/save/API change or partially transactional Step exception was introduced.
Causal lifetime/persistence, robust bounded continuation and safe uncertainty
remain a correctness blocker. All failed variants, exact poses/features, sources,
original fixtures and measured costs are preserved in [CCD research](ccd/REPORT.md).
There is no claim that three of the four production failures were fixed.

### Half-turn liquid query identity

The original full `knots` check21 still fails at its unchanged threshold. Separate
query23 and normalization36 controls pass. Independent actual-triangle/cap, owner
alias, partial-load, shift/restore and full/dry controls pass38 half-turn and39
radial checks; the absent-mesh negative fails as intended.

Two distinct original points can become the **same binary32 shifted point** while
having different cell-owned caps/velocities. A stateless sampler given that one
point cannot recover both identities. Actual wave bytes/ledger remain exact;
partial-body displaced-volume error is about1.3e−15m³ (radial9.0e−16). This identifies
a query-identity/reconstruction contract boundary, not a lost-volume repair.
Query-only smoothing would disagree with actual geometry/cell ownership.

Current liquid API guidance is corrected; no runtime, fixture threshold, M54/M55,
legacy-PBF or independent research implementation changed. A precise owner-local
identity API or coherent continuous physical reconstruction requires separate
design; the evidence does not prove all continuous fluid methods impossible.
[Liquid proof](liquid/REPORT.md).
Documentation checkpoint: **`139d0de20e2e443bd186a6ed8f786eaa223cbba6`**,
`Record liquid sampling identity limits`.

## Combined qualification

- **Linux:**11 focused fixtures/32614 assertions against the final numerical archive;
 15 actual application/lifecycle processes/383 assertions; current-identity cache
 and continuation gate129; contact lifecycle1076; real Spring Range36+14; Lastlight
 startup/presentation with unchanged tracked source. Actual independent network
 processes14 checks, reconnect and host physical endpoints. Do not sum repeated
 fixture assertions into a fictional coverage count.
- **Performance:** unchanged four-ragdoll52-body/8192-triangle observer passes its
 existing16.67ms p95 gate: **4.183063ms p95 /13.130335ms worst**; two bounded CCD
 search limits remain. This is separate from room1024 and not a matched speedup.
- **Editor/export/SDK:** real17-tool Linux identities/checksums/dependencies and
 selected operations, syntax-negative, authoring/undo/PlayStop/save and relocated
 four-scene standalone all pass. Actual packaged inspector followed by SDK verify
 retains every payload byte. [Linux receipts](linux/REPORT.md).
- **Native Windows:**16 selected MSVC targets, zero compiler warnings, selected
 physics plus actual M70/M77 fresh physical endpoints, editor PlayStop and relocated
 Unicode/spaces standalone pass. Missing historical contact fixture is preserved
 as an archive setup failure, then exact supplied-fixture follow-up passes13327.
 Four curved failures stay failed. Source1438 files/PE/pristine artifact hashes
 and unchanged VM configuration are verified. Stock QXL is not GL3.3; isolated
 llvmpipe copies provide functional graphics only. VM is suspended as found.
 No full SDK rebuild, hardware GPU/controller/audio or performance claim.
 [Windows receipts](windows/REPORT.md).
- **Android:** affected ARM64/API26 native runtime compiles and links successfully;
 source pins remain unchanged. No new APK/emulator/device/hardware acceptance.
 [Actual build](android/RESULTS.json).
- No full production-suite marathon. Linux SDK still requires host-compatible
 system libraries (recorded glibc2.43/GLIBCXX3.4.32); older distributions are not implied for the host-built artifact. The separate
 hosted Ubuntu24.04 SDK passes its declared2.39 baseline; this does not inherit
 newer-host artifact compatibility or qualify every distribution.

## Remaining original-audit disposition

All10 original workstreams and54 findings retain their original text/status/history
with appended current execution/disposition fields. Fresh checks are distinct from
carried-forward evidence. [Register](AUDIT_DISPOSITION.md).

Ranked remaining work:

1. **P0:** coherent, bounded finite/sided mesh contact continuation with endpoint,
   cold-save and invalidation safety. Do not ship the isolated passing reduction.
2. **P1:** extreme connected-contact tails still approach two seconds. Actual
   repeated geometry/dependency work remains after broadphase savings; no noise-based
   maximum-speedup or arbitrary fidelity reduction.
3. **P1:** precise liquid query/reconstruction identity contract, keeping geometry,
   storage, submersion and conservation mutually consistent.
4. **Release qualification:** physical platform hardware, olderLinux ABI, full clean
   release/untracked-input policy, archive test-data completeness and untouched
   legacy-controller/advanced composition coverage remain explicit.
5. **Deferred:** FLU001 and exact historicala106 missing-continuation limitation;
   unrestricted regional suspension/native-topology migration/universal online
   replication/security certification remain unsupported, not newly broken.

Protected historical evidence, accepted games/assets/fluid implementations and
Solbert's independent research are unchanged. The exact4c comparison and final
fingerprints verify the intended inventory. No main update, M100, Alpha release,
release tag or Pass5 human acceptance.

## Human checklist

1. Run the relocated physics lab and press **F5** for the watch tour; **1/2** select
   ordinary128/512-piece rooms. Confirm contact/assemblies behave normally.
2. **M** enables machine motors, **J** impulses one assembly, **K** all, **P** drops
   pieces. Check wake, collision and settling. **3** remains the known extreme1024
   stress case and can stall badly; it is not a60Hz acceptance test.
3. Test **F6/F7** save/load, **R** reload, **Esc** pause/resume and editor Play/Stop.
   Confirm authored content is restored and input/camera control remains usable.
4. Optionally reopen Spring Range/Lastlight with the current runtime: aim/fire,
   switch camera, pause/restart, and compare familiar physical interactions.
5. Inspect the failed curved/liquid receipts and the fresh CI run. Their known
   limits must remain visible; a useful lab and green unrelated gate do not close them.

Working package: `/tmp/Judas-PreAlphaPass5-Physics/judas`.
Editable project: `projects/physics_stress_lab/physics_stress_lab.judasproj`.
This corrected candidate is independently checkpointable; the broader Pre-Alpha
readiness blockers remain open and human acceptance belongs to the operator.
