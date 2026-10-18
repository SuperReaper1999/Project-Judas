# Pre-Alpha Pass 3 — correctness, performance and maintainability

## Status and scope

Starting point: human-accepted liquid correction **`f647d7fd95f60d3a37dba70a51f4e45dc41c2922`**. Development and the verified checkpoint belong to **Experimental only**. Main remains at that accepted baseline. This pass is **not human accepted**, is not M100 and does not establish Alpha readiness.

The combined Linux candidate passed the focused matrix below. Two principal physics problems remain: curved-sheet CCD continuation and extreme dense-contact latency. The half-turn liquid query contract also remains failed. Investigation established their mechanisms more precisely; unsuccessful changes were preserved and rejected.

[Every original finding](AUDIT_DISPOSITION.md) · [Machine-readable register](FINDINGS.json) · [Exact changed files](CHANGED_FILES.txt) · [Candidate fingerprints](FINAL_FINGERPRINTS.json)

## Actual production changes

### Origin transaction admission

`RuntimeWorld` previously admitted a move between individually legal −10¹² and +10¹² m absolute anchors, then `PhysicsWorld` threw because the translation exceeded its existing 10¹² m per-component bound. Pending intent had already been consumed. The exact failure and independent positive/negative controls are preserved.

`src/OriginLimits.h` shares the unchanged native bound. Admission now rejects the actual invalid delta before accepting/publishing intent, gives an intermediate-anchor diagnostic and preserves an earlier valid pending request. Direct native calls retain their old exception. Twenty-three focused checks pass, including exact physical participant bytes and direct body-endpoint immutability. [Integration evidence](integration/REPORT.md).

### Exact redundant contact work

- `Contacts.cpp` no longer constructs an unused forward geometry context before reversed box/sphere dispatch.
- SAT evaluates the same axes/gaps/signs, then normalizes/orients only the winning response axis.
- Impact support membership uses an exact sorted **copy** and binary search. Original event order, pair list, row order and solver arithmetic remain authoritative.

No timestep, CCD policy, contact count/quality, iteration budget, material, sleep threshold or collision tolerance was changed. The 128,000-query primitive observer produces identical **11,718,912 bytes** of explicit contact and diagnostic fields. All 270 dense physical digests/counters and all four smaller final controls match the frozen baseline. [Dense evidence](dense/REPORT.md).

### Source-breakpoint diagnostics

The editor warns that a stopped online script also suspends its owner's transport and remote peers may time out. It observes only `NetworkIfLoaded`, preserving optional allocation. There is no transport-pumping reentrancy or timeout retuning.

The actual QuickJS-stop regression uses two independent worlds on real loopback TCP: short-stop survival, normal idle timeout under an injected eleven-second clock advance, exact frozen owner pose/velocity/clock, one resumed callback and ordinary reserved-identity reconnect. It is explicitly **not** two OS processes or a transparent distributed debugger. Existing broader session controls still pass. [Policy and proof](integration/REPORT.md#debugger-and-networking-distinct-pause-policies).

## De-spaghettification with behaviour proof

| Change | Concrete improvement | Preservation evidence |
|---|---|---|
| UI/joint bridges | `ScriptBindingsUI.cpp` and `ScriptBindingsJoint.cpp` replace dense unrelated dispatcher blocks; narrow typed context, no exposed Impl | Reconstructed complete original ScriptSystem: **63,499 tokens identical**; live API unchanged |
| Value conversion | Shared exact String/Number/ReadVec helpers; coercion, bounds and QuickJS ownership together | Original bodies/order/errors retained; negative mutations detected |
| Editor preview | `editor/RenderPreview.cpp` owns runtime render/appearance preview plus undo/redo history | Seven original function bodies token-identical; document/world history ownership retained |
| Runtime transaction | Origin admission/publication phases now readable control flow | Separate mechanical patch: **2,001 tokens identical** after the functional guard |
| CCD control flow | `ImpactTime`/`AdvanceImpacts` formatting makes continuation and failure paths readable | Whole PhysicsWorld mechanical stage: **53,686 tokens identical**; unchanged actual articulated trajectories |
| CMake | 175 ordinary source/link/warning triplets use the existing helper; four cohesive subsystem declaration modules | Original **6,646 command tokens** reconstruct exactly, with explicit accounting for new files/target; Linux/Windows/Android conditional declaration parity and 12 controls |

Script lifecycle formatting reduces long-line concentration; source size alone is not a quality measure. CMakeLists shrinks from 2,032 to approximately 1,600 lines; ScriptSystem's lines over 140 columns drop from 502 to 416. These files and RuntimeWorld/EditorApplication remain substantial. A universal lifecycle registry was **not** invented.

Mechanical changes are independently reviewable as `maintainability/MECHANICAL_FORMAT.patch`, `MECHANICAL_ORIGIN.patch` and `ccd/formatting/format-only.patch`, separate from extraction and functional changes. The checkpoint preserves the one combined tested state rather than manufacturing an untested intermediate source state.

The same cached native observer objects were linked against frozen accepted and candidate archives: **114 assertions pass**, all nine captured script/UI/joint state and physical observations match exactly. After the token-identical origin formatting, the original native tests, real editor and cookbook were rerun on the combined source. [Detailed maintainability report](maintainability/REPORT.md).

## Correctness findings retained honestly

### Curved-sheet continuation: still failed

The unchanged fixture reports **193 checks / four failures**, with 11/20/15 crossings across mesh resolutions. Initial arrival remains separately passing. The first loss occurs after three real TOIs: support continuation suppresses further CCD for the concave mesh, a glancing segment moves the box center behind a triangle's sidedness cutoff, then narrowphase loses contact while the box still straddles the sheet. This is a real physical defect, not merely an invalid endpoint oracle.

A **16-triangle** reduction reproduces the same 55.951 mm crossing and bit-identical first poses. Two attempted local policies were rejected: both retain failures and introduce unacceptable work. Neither is in production. A coherent repair must share finite-feature front-contact episode semantics across Boolean queries, response, invalidation and continuation/persistence; repeatedly adjusting one local cache or front normal is insufficient.

The final frozen/candidate `60 trough` comparison retains all three actual articulated trajectory digests and all 20 non-timing fields exactly, including the original failed assertions. This proves maintenance preservation; it does **not** turn failed collision behaviour into acceptance. [Original failures, rejected experiments and proposed repair boundary](ccd/REPORT.md).

### Liquid extended tests: one oracle corrected, one limit open

- **Normalization:** the original exact-total failure compared against a baseline from before a legitimate flow step. The corrected test still uses exact equality, now immediately before/after the rejected request, and additionally checks every participant's version/bytes. Full command: **36 passing checks**. The original unmodified source and its failure remain archived.
- **Half-turn knots:** full command still fails after 21 checks. Binary32 chart subtraction shifts the represented point by 17.26 μm across a discontinuous finite-volume cell boundary, causing about 4 cm surface and 0.016 m/s velocity sample differences. New fixed-state controls show the same discontinuity before any shift, and match the translated query to its correctly quantized point. An interior-cell control retains the original tolerances. These **23 passing controls are separate from the failed contract**.

No liquid production implementation, numerical algorithm or research was changed in Pass 3. FLU-001 stays deferred and failed; the legacy a106 save still cannot reconstruct absent historical continuation. [Full analysis and exact original reproduction](integration/REPORT.md).

## Matched performance

Release Linux, Intel i7-4790, same authored 1,024-loose-piece room, physical settings, 60 Hz step, fixture hashes and 270 measured steps. The worst recorded population includes **1,285 total bodies**, including machinery/static bodies; 1,024 awake and 128 sleeping at the endpoint. Do not misread the scene name as total body count. Windows was off during comparisons.

| Fixed-step metric | Frozen accepted baseline | Final combined candidate |
|---|---:|---:|
| Median | 27.052 ms | 27.334 ms |
| p95, same floor convention | 379.672 ms | 376.826 ms |
| Worst | 1,938.679 ms | 1,887.047 ms |
| Endpoint deepest penetration | 7.2171085 mm | 7.2171085 mm |
| Final physical digest | `192f0f4bed728953` | identical |

These are **mixed modest observations**, not a broad speedup claim. Earlier isolated changed-object measurements showed 25.819/372.143/1925.992 ms; the final combined measurement, not that better median, is authoritative here. A brief metadata-only probe during the final window is recorded. Final worst-frame RSS: **374,509,568 bytes**; profiler reserve: **70,809,112 bytes**. Memory reduction is not claimed.

The bounded baseline observer counts 57,496,461 linear pair comparisons over 71,980 support lookups. Discovery candidate queries/copy/sort take 107.7 ms inside about 619.8 ms island discovery. Rescheduling takes about 576.8 ms including 311.4 ms TOI; geometry about 886.4 ms and solving 161.1 ms. These categories **overlap and must not be summed**. Near-global event discovery/rescheduling remains dominant. A lookup-only scheduled-event index and dependency/proxy-growth-aware candidate invalidation are evidenced next investigations, not implemented optimizations or permission to reuse stale contact truth.

The primitive-only three alternating repetitions show a small approximately 2.4% improvement with exact output bytes; it is not extrapolated to whole-game performance. Raw data, profiler captures, original source/binary identities and reproduction commands are in [dense/](dense/REPRODUCTION.md).

## Final Linux validation

Release build: 35 selected targets, **zero C++ warnings/errors**. Existing third-party CMake deprecation messages are retained separately. This was focused validation, not another full production-suite run.

- **21/21 provenance-verified manifest cases:** collision geometry/contact/impact/arrival/continuity, CharacterMotor, native physics continuation, contact lifecycle, composed terrain/origin/save, origin physics/assemblies/participants/streaming/admission, save evolution, real session/breakpoint boundaries, first project, liquid origin and oracle controls.
- Supplementary original tests: impact robustness **4,412/0**, world persistence **27/0**, ragdoll **36/0**, scripting **35/0**, debug/reload **49/0**, runtime UI **22/0**, actual editor debugger **20/0**, existing consumer edit/debug/reload **37/0**.
- Independent cold processes: M77 **in-progress corrective step 24+13**, standing **23+13**, recovery **21+13**; M70 **10+14**, all passing. Native continuation separately checks actual restored physical data; M70's fixture proves restored partial authority and finite pose, not full rig trajectory identity.
- Cookbook: **38 executed scenarios / 312 checks**. Live/type drift: **473 symbols, 40 exports, 311 native operations**, 51 declared example files, TypeScript 5.9.3 and four missing/phantom controls. Static example-file coverage is not confused with 51 executed scenarios.
- Tooling: **12 CMake controls**, **six gate controls**. A temporary manifest-compaction error was caught by the latter; known findings were restored and the corrected six pass. The original failure and a mistaken relative TypeScript invocation are retained as development records, not engine defects.
- Actual editor Play/Stop saves an **identical authored scene**. First-project export launches after relocation from unrelated `/tmp` and renders a nonblank frame. Physics Stress Lab also exports and launches from its own relocated `/tmp` package, exercising bounded watch-tour startup.
- The final binary's embedded tracked and untracked build-domain fingerprints matched the active candidate before the gate, and stayed equal through it. See [native receipts](final-linux/RESULTS.json), [build commands](FINAL_BUILD_COMMANDS.json), [all observed build-domain inputs](FINAL_BUILD_INPUTS.json) and [binary identities](FINAL_BINARY_IDENTITIES.json).

The tested binaries identify the exact **dirty f647-based candidate**, not a falsely clean later commit. Candidate source hashes and this checkpoint's Git tree bind that state to the commit. Existing unrelated untracked inputs are observed in provenance and excluded from staging. Evidence-only additions do not alter tested C++ inputs.

## Platform qualification

Accepted-baseline Ubuntu clean-checkout CI **39133017442 passed**; [actual receipt](ACCEPTED_BASELINE_CI.json). This supersedes older reports' pending status for that baseline, but is not a claim of new Pass 3 remote CI or universal Linux ABI compatibility. Local final host is glibc 2.43; older-distribution compatibility belongs to its own CI artifact.

Current Windows qualification was attempted. The VM responded, then copying a 429 KB native-source archive failed with **disk full**; final guest receipt reports **zero bytes free**. No Pass 3 native C++ build, all-tool SDK, editor or moved-package pass is claimed. Existing guest configuration, saves and older evidence were preserved; the VM is back off. [Platform record](windows/REPORT.md). No new Android hardware qualification was performed.

## Remaining work and Alpha blockers

The [register](FINDINGS.json) preserves every original quote, 54 finding IDs and ten priority workstreams with separate current dispositions. Prior scoped passes are not relabelled as fresh validation.

1. **Curved-mesh CCD continuation:** implement coherent bounded finite-feature contact episodes and prove them against the original failures, not only first arrival.
2. **Dense event cascade:** profile the newly isolated rescheduling/discovery costs; choose exact invalidation/index improvements or a justified architectural change. Approximately 1.89-second worst steps remain unacceptable for real-time workloads.
3. **Qualification:** free Windows validation capacity and complete current native engine/all-tool SDK/moved-package checks; separately qualify supported Linux baseline artifacts and Android changes when affected.
4. **Liquid sampling contract:** preserve position intent and agree on consistent continuous reconstruction/query semantics across representations before changing the failed half-turn contract. FLU-001 remains a separate deferred legacy-PBF review.
5. **Maintainability:** continue cohesive runtime/editor/physics extraction with endpoint proofs. Component lifecycle bookkeeping remains distributed. Do not expose a giant Impl header or add a universal framework solely to move lines around.
6. **Deliberate boundaries:** regional liquid/articulation suspension pins, bounded local excavation, opaque inactive-state migrations, newly authored-content merge policy and M99 replication limits remain explicit. Resource-eviction scaling, full per-member capability coverage and new-developer onboarding still need separately scoped evidence.

These limits do not invalidate the bounded passing corrections. This candidate is ready for an **Experimental engineering checkpoint and human review**, independently of broader Alpha qualification.

## Human checklist — one combined pass

1. Open the Physics Stress Lab; **F5** watch tour, **M** motor power, **J/K** impulse one/all. Confirm springs, wake/sleep and independent assemblies behave as expected.
2. Select **4** for rotated geometry/gravity; use **WASD, mouse, Q/E**. Save/load with **F6/F7**, then **R** reload. Start with **1**; **2/3** intentionally expose the unresolved high-density limit.
3. Open a normal project in the editor: select render objects, inspect runtime appearance/material controls, try preview undo/redo, then Stop and confirm authored values return. This remains a human check, beyond source/token preservation.
4. In Developer Lab, stop on a source line, inspect variables, step, edit/reload, then Stop/Play before compatible saves. Confirm useful errors and no unexpected world advancement.
5. With an online peer, compare ordinary game pause with a long source breakpoint: the latter warns and can disconnect; continue/reconnect normally. No transparent online-breakpoint promise.
6. Revisit the accepted rotated-liquid demos: waves/quantity persist through supported changes. The half-turn seam sampling limit and deferred PBF test remain recorded, not visually certified by this checklist.
7. Try the relocated Physics Stress Lab package and ordinary project export from outside the repository.
8. Inspect readable UI/joint/origin/impact source and CMake modules; confirm the current documentation and API completion still describe the familiar public behaviour.

Editable lab: `projects/physics_stress_lab/physics_stress_lab.judasproj`.
Current local lab export: `.cache/prealpha-pass3/final-linux/physics-lab-export/`.
Relocated playable lab: `/tmp/Judas-PreAlphaPass3-Physics/judas`.
Press **F5** for its existing watch tour. No game content or lab policy was redesigned for this pass.

Protected history, existing games, accepted fluid implementation and independent research remain unchanged. Main is not promoted; no release tag, M100 work or Alpha declaration.
