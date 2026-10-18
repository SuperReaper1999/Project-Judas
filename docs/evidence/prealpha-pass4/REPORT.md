# Pre-Alpha Pass 4 — physics ownership and platform qualification

## Status and scope

Accepted input: **`7da56c2cc6ce7acda827a6e4019d610622b237e2`**. Work is on
**Experimental only**. Main remains at that accepted input. Pass 4 has no human
acceptance, release tag, M100 work or Alpha-readiness claim.

This pass substantially reduces the physics coordinator, removes redundant
impact scheduling work and investigates the remaining CCD and liquid failures.
It preserves the unsuccessful numerical experiments instead of shipping an
incomplete repair. Platform qualification is recorded separately from physics
performance and from human testing.

[All original findings](AUDIT_DISPOSITION.md) · [Machine-readable register](FINDINGS.json)
· [Exact changed files](CHANGED_FILES.txt) · [Final source/binary fingerprints](FINAL_FINGERPRINTS.json)
· [Reproduction and checkpoint identities](REPRODUCTION.md)

## Shipped changes

### A smaller physics coordinator with one owner

`PhysicsWorld.cpp` falls from **4,080 to 859 readable lines**. The public API is
byte-identical. One private `Impl` retains the original ordered state; ten
cohesive implementation units own bodies, bounds, contacts, CCD search, impact
events, joints, particles, persistence, queries and sleeping. The largest unit
is the 1,036-line body implementation; there is no replacement giant subsystem
file or second physics owner. Small hot helpers stay inline where appropriate.

The [module map](../../../src/physics/WorldModules.md) explains responsibilities,
internal dependencies and the fixed-step sequence. A common CMake source list
replaces duplicated direct-world fixture lists and rejects incomplete lists at
configuration, with positive and negative controls. Formatting expands the
implementation into readable control flow rather than compressing statements
to meet a line target.

Independent preservation checks establish **174 identical method bodies**, the
same **2,183 ordered state/declaration tokens**, helper/type preservation and
identical public-header bytes. Four negative mutations demonstrate that the
checker detects absent or changed mechanisms. Ten unchanged test objects were
linked against the old and extracted engines: eight complete outputs match
byte-for-byte; two differ only in four printed timing lines. The 270-step real
application comparison retains every recorded physical digest and counter.
Extraction and later functional optimization have separate checkpoints and
evidence. [Refactor proof](refactor/REPORT.md).

This is not whole-engine cleanup. Current source still includes 416 lines over
140 columns in `ScriptSystem.cpp`, 88 in `RuntimeWorld.cpp`, 139 in
`editor/EditorApplication.cpp` and 185 in `editor/EditorPanels.cpp`. Their
responsibilities and dense control flow remain incremental maintenance work;
line counts alone would not establish closure. The new coordinator has no lines
over 140 columns.

### Exact impact scheduling work

The event loop uses lookup-only hashing for vector-owned scheduled indices,
deduplicates body-pair expansion, marks existing dirty keys by index and retains
the original final lexicographic order. Two query scratch copies become safe
references after checking that their nested paths cannot re-enter the scratch
owner. No contact truth, TOI, CCD policy, physical ordering, timestep, tolerance,
iteration budget, material or sleep threshold changes.

A separate observer reconstructs the original key union at **252 actual
rescheduling events**: 871 duplicate candidates reduce to the same 307 unique
keys, with zero mismatches. Deliberately omitting a key produces 54 mismatches
and failure. One nonzero child-part index is exercised; the zero-change selected
event branch is retained but was not observed by this fixture. The same ten
native fixtures, four scaling controls and all 270 physical digests remain
passing/identical. [Scheduling evidence](dense/REPORT.md).

### Windows compilation and current documentation

Actual native MSVC compilation exposed a virtual `judas` module raw literal of
16,866 UTF-8 bytes, exceeding its per-literal limit. One adjacent C++ raw-literal
boundary splits it into 13,783 and 3,083 bytes; the existing 13,439-byte fragment
is retained. The emitted **30,305 JavaScript bytes are identical**, SHA256
`b9d26c0405d234a31dd9a5f95754a5dd38ad69d46966dad76a915344ee838da2`.
There is no runtime/API change, new binding or declaration change. The original
compiler failure and the native compile/execute control are retained.

Native qualification then exposed a separate SDK-tool defect. The documented
packaged inspector imported sibling Python modules, created two cache files
inside its checksum-verified SDK, and rejected its own mutation. Earlier Linux
checks invoked the source-tree inspector against the SDK, so they did not
exercise this packaged-entry ownership contract. The packaged Android verifier
had the same cache-writing path.

Both entrypoints now disable bytecode writes before sibling imports. Strict
inventory/checksum checks remain unchanged. Sixteen compiler-free subprocess
controls reproduce both original mutations, prove corrected byte-preservation
and still reject deliberately injected caches and corrupted payload. Their ELF
and APK fixtures are explicitly synthetic metadata, not executable platform
qualification. The original Windows 41/42 result is preserved separately from
the corrected follow-up. Nine additional commands reproduce the defect against
the actual 17-tool Linux SDK, then prove that a normally restaged SDK's packaged
inspector and native loader checks pass without changing any payload bytes.
An injected cache still fails. No C++ rebuild or binary identity rewrite is used
for this Python-only checkpoint, `df8929f73505d407697b5d35b115fc71d9100471`.
[SDK ownership evidence](sdk-readonly/REPORT.md).

Current architecture, liquid sampling and README status are corrected. Historical
reports retain their original wording and checkpoint-time status. The README
copyright and MIT terms are preserved.

## Correctness investigations that remain open

### Curved-sheet CCD continuation

Production retains the original full-fixture failure: **193 checks, four failed
assertions, 11/20/15 crossings** across the three curved meshes. Initial entry is
a separate passing contract. The preserved 16-triangle baseline reproduction
still exposes the same contact-loss mechanism; different research variants are
not claimed to have identical trajectories.

Finite-feature front ownership, a per-feature penetration-growth budget and an
outward-side certificate can make an isolated prototype pass all original
checks. Actual finite-edge, rear-entry, rotation, no-collider and ownership
controls expose what the mechanism requires. Removing history again fails,
which establishes its causal necessity for this prototype rather than treating
it as a harmless recomputable cache. It does not exclude other coherent
representations.

The prototype costs **39–41% more** on the preserved articulated mesh workload
and still lacks complete within-step expiry/re-entry, two-sided ownership,
invalidation, bounded history and cold-save/regional-suspension contracts.
It is **excluded from production**. Passing an isolated fixture does not justify
shipping incomplete physical continuation. All stages, failures and ablations
are preserved in [CCD research](ccd/REPORT.md).

### Half-turn liquid sampling

The unchanged full `knots` contract still fails at check 21. Binary32 point/owner
conversion shifts the represented query by **17.26 micrometres** across a
discontinuous finite-volume cell boundary, changing the sampled surface by about
4 cm and velocity by 0.016 m/s. The same two quantized points already differ
before shifting. Wave bytes, energy and conserved quantity remain unchanged.

Separate query controls pass **23 checks**; the previously corrected rejected-
transaction normalization oracle passes **36**. These do not replace the failed
full contract. Query-only smoothing would disagree with physical cell ownership,
geometry and transfers, and double intermediates cannot recover rounded input.
No fluid algorithm, implementation, threshold or research is changed.
[Liquid query evidence](liquid-query/REPORT.md).

FLU-001 remains deferred and failed. The historical a106 cold-save snapshot still
lacks continuation history and cannot guarantee exact restoration; no missing
state is invented.

## Matched Linux physics performance

Intel Core i7-4790, GNU 15.2.0, Release `-O3 -DNDEBUG`; unchanged real room1024
application, 270 measured 60 Hz steps, same scene/settings/physical gates.
The Windows VM was paused and other owned intensive work held during both runs.

| Fixed-step metric | Accepted baseline | Final scheduling implementation |
|---|---:|---:|
| Median | 25.234815 ms | 25.337992 ms |
| p95, identical floor convention | 366.814531 ms | 349.999855 ms |
| Worst | 1899.674751 ms | 1892.499906 ms |
| Whole-process wall time | 53.106494 s | 51.622092 s |
| Maximum process RSS | 410508 KiB | 408156 KiB |

Every recorded physical field and trajectory digest matches. These are **mixed,
modest observations**, not a broad speedup or memory-saving claim. The extreme
near-global event cascade remains a serious roughly **1.9-second tail**.
No repeated rerun selected a more favorable maximum.

The unchanged four-ragdoll gate separately passes on Linux: 52 articulated
bodies against 8,192 mesh triangles, 900 steps, median **0.652534 ms**, p95
**4.814678 ms**, maximum **15.280967 ms**, against the original **16.67 ms p95**
gate. Two bounded CCD search-limit hits remain recorded. This is a scoped
physical workload, not proof that all articulated contacts meet 60 Hz.

## Linux and platform evidence

The combined physics checkpoint has a Release SDK/focused build with zero C++
warnings in that log. Fifteen native functional invocations pass **383 checks**,
including actual M70 physics, independent M70 cold processes, and M77 in-progress,
standing and recovery saves restored in fresh processes. The M77 checks inspect
the restored physical Standing endpoint, not merely a cached status. Origin
physics, assemblies, streaming, lifecycle, persistence and particle application
checks pass.

The Linux SDK contains **17 public executables**. Verification, loader inspection,
real script checking, an intentional syntax failure, mesh/collision cooking,
volumetric authoring and normal export are exercised. A copied ordinary Stress
Lab passes editor authoring/undo/Play/Stop with exact authored scene restoration.
Its export launches for 90 frames after relocation from unrelated `/tmp`.
Actual automation graphics are **llvmpipe software OpenGL**; relative mouse mode
is unavailable in this environment. These results do not certify interactive
desktop capture or hardware GPU performance. Host ABI is GLIBC 2.43 /
GLIBCXX 3.4.32, not older-distribution qualification.

The final byte-identical script follow-up passes another **53 executed
assertions**, actual TypeScript/live API coverage of **473 symbols / 40 exports**
and four deliberate drift controls. Its SDK and moved standalone also pass.
The entire rebuilt GCC engine archive is identical to the earlier tested one;
only separate build identity/tool binaries change as expected.

Native Windows results are recorded separately. Android **ARM64/API26 Release**
compilation/linking passes both the physics changes and the final literal edit,
using the existing NDK 28.2 toolchain. Four Clang warnings remain in the final
incremental log. There is no new APK, emulator or physical-device test.
See [Linux receipts](linux/REPORT.md), [final scripting/SDK follow-up](linux-final-portability/REPORT.md),
[Windows receipts](windows/REPORT.md) and [Android receipts](platform/android/README.md).

### Native Windows qualification

The complete MSVC x64 Release **18-executable SDK**, editor and runtime build
passes from the no-Git source archive, whose 1,435 selected files were verified.
Native binaries retain actual `9deb47d1` provenance; the later Python-only
`df8929f7` patch is separately hashed when restaging the SDK. Existing MSVC and
upstream warnings remain visible.

All 18 executable identities and selected real tool operations run. Six packaging
controls pass, including Unicode/spaces relocation, normal export, missing-ICU
refusal without publication and corruption rejection. Seven corrected packaged-
inspector positive/negative checks pass without bytecode suppression flags or
environment masks; strict verification still rejects unexpected files.

Four affected physics/origin/motor/persistence fixtures pass **4,494 checks**;
ordinary M70 passes **98**. Eight separate M70/M77 cold processes pass **131**,
including actual restored physical Standing endpoints. Editor authoring/undo/
Play-Stop preserves the authored scene bytes. Runtime and relocated standalone
exercise save/load, rotated scene transition, pause/resume/reload; a deliberately
wrong-scene oracle fails. The native mesh-cost invocation retains **372 checks /
four known failures**, separately recorded rather than counted as green.

Original setup failures are preserved: a task-only source-verifier encoding
mistake, limited-user access to SYSTEM-created generated staging, and missing
font/example prerequisites in the isolated test runner. Supplying unchanged
inputs or task-owned access and rerunning the affected cases does not alter
engine/controller behavior or acceptance thresholds.

Stock QXL reports unavailable OpenGL 3.3 cleanly. Actual desktop/editor/game
execution uses isolated llvmpipe copies; pristine SDKs do not gain injected Mesa
DLLs. No physical GPU/audio/controller or VM performance acceptance is claimed.

The user-authorized disk expansion is **40→90 GiB**. Original system/recovery
partitions remain unchanged; the new tail becomes the **50 GiB P: work volume**.
Older generated build trees are hash-backed outside Git and relocated with
compatibility junctions. Toolchains, personal data and irreplaceable evidence
remain; persistent VM XML is unchanged. Future HDD relocation remains the user's
separate plan.

## Remaining Alpha blockers and next work

1. **Curved CCD:** design bounded causal finite-feature ownership with complete
   release/re-entry, invalidation and persistence; retain rear-entry and finite
   miss controls. Do not ship this pass's research prototype.
2. **Dense event cascade:** address repeated global chronological discovery and
   rescheduling at an architectural boundary, with matched actual trajectories.
   This pass's exact deduplication does not close the tail.
3. **Liquid query contract:** agree on coherent precision and continuous
   reconstruction across physical geometry, query and transfers. FLU-001 is a
   separate deferred review.
4. **Platform breadth:** retain software-renderer/VM limits and qualify hardware,
   Android devices/controllers, older Linux ABI and clean-machine distributions
   separately. Successful native compilation is not universal portability.
5. **Maintainability and integration:** continue cohesive runtime/editor/script
   extraction and targeted lifecycle checks. Universal subsystem registration,
   arbitrary regional suspension/replication and unrestricted migration of
   retained topology remain outside established contracts.

All ten original workstreams and 54 findings are retained in the register.
Carried-forward evidence is explicitly distinguished from new execution; broad
semantic documentation and per-member feature certification are not claimed.

## One human checklist

1. Open **Physics Stress Lab** in the editor. Start with **1**; press **F5** for the
   watch tour, **M** for motors and **J/K** for one/all impulses. Check independent
   motion, contact, settling and wake-up.
2. Select **4** for rotated gravity; try **WASD/mouse/Q/E**, **F6/F7** save/load and
   **R** reload. **2/3** deliberately expose the known density limit; mode 3 can
   still hitch severely.
3. Stop Play and confirm authored content is restored. Inspect ordinary scene
   editing and the moved standalone package; verify interactive mouse capture
   on the desktop, which automated software-rendered tests cannot establish.
4. Revisit the Character/Balance labs and a saved in-progress action. Confirm
   restored physical motion continues without invented legacy-save compatibility.
5. Inspect `PhysicsWorld.cpp`, its module map and private modules: responsibilities
   and fixed-step order should be understandable without traversing a monolith.
6. Run the packaged SDK inspector with its receipt outside the SDK, then verify
   the SDK again; no cache files should appear inside it. On Windows, inspect
   the editor and moved package with their stated VM/software-renderer scope.
   Physical GPU/audio/controller checks remain separate. Android compile-only
   evidence is not a device acceptance request.
7. Revisit accepted liquid demos without treating them as proof that the documented
   half-turn seam or deferred legacy PBF contract is fixed.

Editable project: `projects/physics_stress_lab/physics_stress_lab.judasproj`.
Local moved Linux game: `/tmp/Judas-PreAlphaPass4-Physics-portability/`.
The corrected Linux developer SDK is
`.cache/prealpha-pass4/sdk-readonly/linux-native/corrected-sdk/`; the original
`build/linux-sdk` snapshot was retained for the packaged-entry reproduction.
The Windows SDK and moved-game paths are listed in its platform report.

The final-source and exact changed-file inventories accompany this report.
The Linux-validated corrective work is independently checkpointable while the
remaining failures keep broader Alpha qualification open.
