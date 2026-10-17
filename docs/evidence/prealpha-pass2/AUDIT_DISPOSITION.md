# Original Pre-Alpha assessment: complete disposition

This reconciles the exact [original assessment](ORIGINAL_ASSESSMENT.md), recovered
from its 2026-10-17 final response. [Provenance](ASSESSMENT_PROVENANCE.json) records
its exact source/text hashes. [FINDINGS.json](FINDINGS.json) preserves all 54 original
quoted subfindings, all 10 original priority rows, actual evidence paths and next actions.

**Pass2 scoped Linux combined validation: 26/26 selected contracts pass.**
[Integration evidence](integration/REPORT.md) preserves original 25/26 failure and
corrected follow-up, 32 supplemental positive commands, three still-failed contracts
and 52 frozen source hashes. This is not exhaustive engine validation, human
acceptance, a clean release artifact or Alpha readiness. Late tooling 5/6 has a preserved 6/6 follow-up; actual
Android build, current Windows C++/SDK and older-Linux CI require separate follow-ups.

## Interpretation

Original detailed IDs did not exist: section letters A–E and table priorities
P0/P1/P2 are historical; the subfinding IDs here are new reconciliation labels.
Resolved means a bounded mechanism has correction and scoped evidence; improved
means partial correction or essential remaining gaps; open means demonstrated
failure or required qualification is still outstanding; deferred marks an explicit
boundary/separate work; unsupported by evidence means no current causal reproduction.

## Original priority workstreams

| ID | Priority | Original workstream | Current disposition |
|---|---|---|---|
| P0-01 | P0 | Explicit build identity and two release channels | resolved |
| P0-02 | P0 | Reliable regression and release gates | resolved |
| P0-03 | P0 | Curved-mesh collision correctness | improved |
| P1-01 | P1 | Dense-contact predictability | open |
| P1-02 | P1 | Packaging and clean-machine usability | improved |
| P1-03 | P1 | Composed lifecycle contracts | improved |
| P1-04 | P1 | Incremental architecture cleanup | improved |
| P2-01 | P2 | Editor onboarding and actionable diagnostics | improved |
| P2-02 | P2 | Current documentation and support matrix | improved |
| P2-03 | P2 | Platform and release qualification | open |

## All 54 original subfindings

Exact original quotations/source/test/evidence references and next actions are in
the JSON. These summaries retain unresolved bounds rather than hiding them behind
a passing parent row.

### P0-01 — Explicit build identity and two release channels

| Finding | Status | Actual result / remaining boundary |
|---|---|---|
| C-01 | resolved | Explicit engine-version.json and shared native identity replace milestone inference/fallback. Unknown source identity is explicit and release-ineligible. |
| C-02 | resolved | Full revision/tree/fingerprint, build ID, independent channel and toolchain/configuration fields identify unnumbered checkpoints. |
| C-03 | resolved | Exact tracked-blob archives include a hashed selected-file inventory and frozen source metadata. CMake validates no-Git archives, detects modifications and rejects malformed inventories. |
| C-04 | resolved | Git HEAD/index/packed references and both primary branch references are configuration dependencies; branch/channel controls exercise actual independent fixtures. |
| C-05 | resolved | The legacy text line remains compatible; runtime/editor/public tools expose --build-info-json and --version before initialization. Android ABI identities are embedded. |
| C-06 | resolved | Bounded shell-free probes compare channel/source/target identity and conservative untracked observations. Missing, stalled, malformed and mismatched binaries are rejected. Exported runtime SHA-256 is recorded. |
| C-07 | resolved | Optional unauthenticated HTTPS ancestry comparison follows the selected channel/revision; offline/missing curl is nonfatal. No updater was added. |

### P0-02 — Reliable regression and release gates

| Finding | Status | Actual result / remaining boundary |
|---|---|---|
| R-04 | resolved | Native and Python process probes now enforce time/output bounds with real stalled/oversized/failed/missing child controls. |
| DX-05 | resolved | Optional pinned tools/verification setup and shared TypeScript resolution replace hardcoded milestone cache paths; native/game builds still need no npm. |
| PL-01 | resolved | Opt-in focused manifest binds executable bytes/embedded identity and preserves explicit failures. Final combined26/26 selected Linux cases pass; the original25/26 test-setup failure is preserved. A late stale tooling assertion failed5/6 and was corrected to distinguish resolved scheduled arrival from unresolved/manual cases; follow-up6/6 passes, with original failure preserved separately. |

### P0-03 — Curved-mesh collision correctness

| Finding | Status | Actual result / remaining boundary |
|---|---|---|
| A-01 | resolved | The actual cooked-triangle oracle now asserts the unchanged 5 cm crossing gate and distinguishes collider removal, legal flight, finite edges, rear entry and rotation. The false-green measurement defect is corrected; remaining physical continuation failures stay separately open. |
| A-02 | open | Pass2 finite-patch initial arrival passes 193 checks with zero coarse/medium/dense arrival crossings, without changing solver budgets or inputs. The original full 90-step continuation still fails 4 assertions, with 11/20/15 crossings versus 24/24/24 baseline. This original physical correctness finding remains open. |

### P1-01 — Dense-contact predictability

| Finding | Status | Actual result / remaining boundary |
|---|---|---|
| B-01 | open | Matched Pass2 dense scene preserves all 270 physical digests/counters and 7.2171085 mm deepest observed penetration. Median 27.070→25.937 ms, p95 413.328→416.695 ms, worst 2123.409→1970.799 ms are mixed, still far outside real time. Whole-world teardown now exits in 54.731 s instead of timing out after exports. Exact witness/scratch improvements do not close the latency gate. |
| R-03 | unsupported by evidence | Source inspection shows repeated LRU scans, but no preserved controlled large-residency reproduction proves an unacceptable cost. No performance correction is claimed. |

### P1-02 — Packaging and clean-machine usability

| Finding | Status | Actual result / remaining boundary |
|---|---|---|
| D-01 | improved | One shared17-tool inventory drives staging/probes. Actual final host Linux SDK verifies17 tools/212 files, embedded identity, hashes and dependencies. Native Windows Python packaging controls pass15, but current C++/SDK execution is incomplete under guest disk pressure; a synthetic inventory is not a Windows qualification. |
| D-02 | resolved | SDK finalization occurs after public target declarations and uses an explicit shared inventory rather than incidental target-discovery order. |
| PL-04 | deferred | Android packaging remains explicitly Linux-hosted. Desktop support does not imply a Windows-hosted APK toolchain. |
| PL-05 | resolved | App name/package/version derive from the project or explicit arguments; engine identity remains separate. Required release app identity is validated before building. |
| PL-06 | resolved | Development/release presets validate app identity/version, private keystore/alias/password environment and clean engine provenance. Credentials remain outside source. |
| PL-08 | improved | Curated SDK/source-engine distributions exclude historical projects/research where appropriate while protecting the repository/history. Static tools still duplicate engine/ICU bytes; no compact universal SDK claim is made. |
| L-07 | deferred | M93 residency operates on whole resources/cooked mip chains, not independently streamed mip residency. |
| D-03 | open | Actual final host SDK probes verify all17 shipped tools, hashes and dependencies on Linux/glibc2.43. The declared Ubuntu24.04/glibc2.39 CI artifact and current native Windows C++/SDK still require their own receipts. Host closure is not universal clean-machine portability. |

### P1-03 — Composed lifecycle contracts

| Finding | Status | Actual result / remaining boundary |
|---|---|---|
| E-01 | improved | Pass2 removes blanket origin refusals for published conserved-liquid owners/parcels, atmosphere reference frames and body-owned combustion state. Final87 parent+2×8 cold checks observe wave/ledger bytes, real submersion/solid loading/optics/flow, radial/non-Y translation, cold physical endpoint and thermal/gas continuation. Unpublished liquid rejects nonzero shifts until normal publication. Active legacy PBF remains unsupported; atmosphere/combustion origin support does not imply modern slot-save support. |
| E-02 | deferred | These pins currently guard unpreserved intent/dependencies. Supported retained bodies/terrain are exercised; universal suspension was never promised. No removal without a lossless continuation contract. |
| E-03 | deferred | M97 local volumetric editing remains the supported representation. Planetary heightfield and arbitrary planet-wide excavation are separate capabilities, not secretly provided by origin support. |
| E-04 | improved | RepresentationIssue centralizes host/client support admission and names the blocking component. The supported entity subset was not expanded. |
| E-05 | deferred | Complete-world restore and explicit game/content migrations remain authoritative. Automatic world/content merge would require policy the engine cannot invent. |
| E-06 | deferred | Native topology and opaque retained participant records remain restricted; explicit M98 migration/error/recovery controls preserve original bytes. |
| R-01 | improved | Final combined tests check actual async terrain/collision/navigation publication, failed replacement retaining the solid, origin changes, cold restoration and destroyed pending owners (34 composed checks). Additional87+16 origin/cold checks and atomic capture controls expose ownership/failure paths. Selected lifecycle coverage improves; no all-component combination certification. |
| R-02 | open | No bounded combined debugger/network timeout experiment has established exact behaviour. Documentation says debugger freezes world work; remote-peer policy still needs an integration contract. |
| DX-07 | unsupported by evidence | The historical observation remains genuine, but no causal mechanism or current reproducer was established. Ordinary loopback/multi-process passes do not explain or invalidate that phone observation. |
| L-01 | deferred | Original failure/threshold and independent fluid research are preserved; no fluid numerical repair is part of this pass. |
| L-02 | deferred | New-format continuation corrections do not manufacture absent historical cache/state. The original a106 snapshot and failing reproduction remain. |
| L-03 | deferred | Current replication remains the explicit M99 subset; admission diagnostics name unsupported physical owners. |
| L-04 | deferred | Typed save evolution does not expose safe arbitrary rewriting of opaque native topology. |
| L-05 | deferred | Unsupported dependencies/intent remain pinned rather than lost. Selected extensions require physical continuation proof. |
| L-06 | deferred | There is still one authoritative bounded local physical chart; world origins and tiles do not create independently simulated distant charts. |

### P1-04 — Incremental architecture cleanup

| Finding | Status | Actual result / remaining boundary |
|---|---|---|
| M-01 | improved | Pass1 extracted identity/validation/SDK/process helpers. Pass2 extracts the unchanged 20 mechanical regression target contracts and shared application-test sources. Large RuntimeWorld/ScriptSystem/editor/physics files remain. |
| M-02 | improved | Shared SDK/support inventories reduce selected duplicated lists, but there is still no universal lifecycle registry. New composed tests cover selected omissions without certifying all component paths. |
| L-08 | deferred | Opaque compatible mesh paths batch; other material/pose/shadow paths retain explicit fallback/selection limits. |

### P2-01 — Editor onboarding and actionable diagnostics

| Finding | Status | Actual result / remaining boundary |
|---|---|---|
| R-05 | improved | Origin, replication and package diagnostics now identify concrete blockers. There is no universal editor preflight registry for every subsystem combination. |
| DX-01 | improved | A complete no-input render/camera starter and ordinary project-from-scratch VM/reload workflow pass20 checks on final combined Linux. Actual editor Play/Stop/debugger and a first-project relocated standalone pass separately. Direct human editor create/debug/export usability remains pending; no hidden FPS controller was added. |
| DX-06 | resolved | Transport errors include endpoint, native cause and corrective action; actual conflict/refused controls passed without changing connection semantics. |

### P2-02 — Current documentation and support matrix

| Finding | Status | Actual result / remaining boundary |
|---|---|---|
| DX-02 | resolved | Current M96 documentation now explicitly records acceptance in the M99 baseline while retaining scoped platform limitations. Historical evidence remains unchanged. |
| DX-03 | improved | Final live API/type/drift checks agree on473 public symbols and40 exports, with TypeScript5.9.3,51 examples and4 negative drift controls. Pass1 full cookbook remains historical. Current guidance distinguishes support boundaries; no claim that every prose paragraph or historical chronology is machine-certified. |
| DX-04 | improved | The current ownership/support guide leads ARCHITECTURE.md and links historical sections. The extensive history remains intentionally preserved, so the file is not a concise standalone handbook. |
| DX-08 | improved | API drift/types/live enumeration and packaged-document inventory checks exist. A complete semantic prose/status checker and repository-wide link verification do not. |
| CAP-02 | deferred | Animation, UI, debugger and renderer limits remain documented; this pass does not promise morph targets, full skeletal keyframe editing, universal instancing or arbitrary expression debugging. |

### P2-03 — Platform and release qualification

| Finding | Status | Actual result / remaining boundary |
|---|---|---|
| PL-02 | open | Actual Linux controls pass24 established packaging plus16 new distribution tests, and native Windows Python passes15 after preserved failures/follow-ups. Final Linux host SDK17-tool qualification passes. Current native Windows C++/SDK remains incomplete under guest disk pressure, and Ubuntu24.04 CI/new final-source APK are pending separate receipts. No Windows GPU/performance or new phone acceptance is claimed. |
| PL-03 | deferred | This remains the deliberate acceptance boundary. Linux host measurements are authoritative for the current performance gate; VM behaviour is functional evidence only. |
| PL-07 | deferred | Physical phone acceptance covers the recorded USB-forwarded route; no direct Wi-Fi/Internet, broad controller or device matrix certification follows. |
| L-09 | deferred | Trusted project scripts and bounded optional transport validation do not constitute a sandbox or Internet-service security audit. |
| L-10 | open | Historical platform/human acceptance remains scoped. Final Linux selected automated contracts and native Windows Python controls pass. Actual new Android artifact, older-Linux CI and current native Windows C++/SDK need separate receipts; controller/GPU/device/human qualification is not invented. |
| CAP-01 | open | Historical player/vehicle compatibility paths remain. Focused modern-component passes are not a complete legacy-gameplay regression gate. |

## Additional implementation-discovered findings

These are separately introduced Pass2 findings, never fabricated original IDs.

- **BUILD-SAVE-DIAGNOSTIC — resolved:** Actual new SaveService capture records the shared running buildId exactly. Independent legacy headers omitting the informational diagnostic restore unknown. Final storage41 and actual application write14/fresh read20 pass; no native format/content compatibility version changed.
- **LIQUID-BAKE-QUATERNION — deferred:** An authored float quaternion baked without the same normalization used by RuntimeWorld produced an equilibrium-up difference 3.350149682e-7, above the unchanged 1e-7 equivalence threshold, before any origin shift. Original fixture/failure are preserved; the origin-contract proof uses an exactly represented non-Y axis permutation. No numerical tolerance or old bake fingerprint is changed.
- **LIQUID-BAKE-SURFACE-KNOTS — deferred:** A separate non-axis rotated basin produces nonmonotonic Surface1 knots during the unchanged bake path. Preserve its original fixture/failure separately from origin-translation validation. No bake implementation, protected fluid research, accepted data or tolerance is modified in this pass.
- **LIQUID-ORIGIN-UNPUBLISHED — resolved:** Nonzero origin changes now refuse while authored liquid owner geometry is unpublished, instead of checking only the incomplete runtime state map. No-op remains allowed and normal publication permits supported changes; no blocking I/O or invented bounds. Final87+16 origin/cold checks pass.
- **CCD-MESH-ARRIVAL — resolved:** Finite-patch swept SAT interval arrival for box/convex against stationary triangle meshes. 193 focused checks pass; exact witness/pair/unwind suite passes 120,283. Full curved continuation remains separately failed, and unsupported shape/motion combinations are not generalized.
- **WORLD-TEARDOWN-DENSE — resolved:** Final whole-world teardown no longer runs a dense wake graph separately for every body immediately before discarding the same world. Matched 270-step process exits successfully in 54.731 s; normal individual entity deletion retains its original wake semantics. Final combined lifecycle checks now pass; this does not close dense-step latency.
- **VALIDATION-SCHEDULING-ASSERTION — resolved:** A stale scheduling unit assertion failed 5/6; explicit resolved/unresolved/manual controls now pass 6/6. Before-source and failure remain preserved; no runtime/physical condition changed.

## Ranked remaining engineering work

1. **Curved contact continuation:** initial collision arrival now passes 193 checks;
   the full 90-step glancing contract still fails 4 unchanged assertions. Establish
   a generic contact episode/one-sided continuation mechanism without weakening
   finite-edge/backface/physical acceptance or causing broad cost regressions.
2. **Dense-contact latency:** unchanged physical digests support scoped improvements,
   but median 25.937ms, p95 416.695ms and worst 1970.799ms remain an open practical limit.
3. **General rotated liquid authoring:** preserve both numerical failures and exact
   bake data; address quaternion normalization and nonmonotonic knots separately.
   FLU-001 stays deferred and untouched; this pass changes no fluid integration.
4. **Qualification/validation closure:** retain the corrected 6/6 scheduling controls,
   append actual Android and post-commit older-Linux CI receipts, and finish native
   Windows C++/SDK when guest capacity permits. None is implied by fixture-only tests.
5. **Selected cross-system contracts:** bounded debugger/network pause, large-residency
   eviction and a real legacy-gameplay compatibility workflow remain unmeasured.
6. **Explicit support boundaries:** general replicated assemblies, universal regional
   suspension, arbitrary opaque-native migration and multiple simultaneous physical
   charts are not silently provided. Legacy a106 saves cannot recreate missing history.

## Maintainability and onboarding scope

The unchanged 20 M79–M83 targets now live in `cmake/tests/MechanicalTests.cmake`,
with shared test/application linkage in `cmake/JudasTestTargets.cmake`. Six actual
CMake-P parity/negative controls pass and selected native targets build/run. This
is an incremental reduction of duplication, not a universal component registry or
a rewrite of RuntimeWorld/ScriptSystem/editor/physics ownership.

The starter uses ordinary public JavaScript, a visible object and an explicit
camera. Its 20 checks observe actual endpoints and failed/repaired reload; actual
automated editor and relocated standalone paths pass separately. Human editor
onboarding, controller/GPU/device usability and all prose remain separately scoped.

## Remaining coverage


- Selected26-case combined Linux gate plus32 supplemental positive commands do not provide exhaustive per-member or all-component-combination certification.
- Full90-step one-sided curved-mesh continuation remains4 failing assertions; two ordinary non-axis liquid-bake contracts remain genuinely failed.
- Dense-contact p95/worst remain far outside real time despite matched physical invariants and scoped optimizations.
- Late tooling scheduling assertion 5/6 is preserved with 6/6 follow-up; post-commit clean CI remains separate.
- Current native Windows C++/SDK, older-Linux CI artifact and actual new Android/device qualification remain incomplete or pending separate receipts.
- Debugger plus live networking timeout behaviour is unverified.
- Large-residency eviction scaling has no matched controlled measurement.
- Repository-wide semantic documentation/status correctness is not mechanically proved.
- Public Internet/Wi-Fi/controller/security and first-project human-usability qualification remain separate.

## Checkpoint readiness

The selected Linux corrective work is independently checkpointable under the
Experimental workflow, with known failures prominently retained. The late tooling unit failure is preserved with its passing follow-up. Pending
artifact qualification must be resolved or explicitly classified before claiming
a consolidated release pass. Main promotion and Alpha publication
are not authorized or inferred. See the [one human checklist](integration/REPORT.md#consolidated-human-acceptance-checklist).


Source-only review also notes a **suspected pre-existing extreme-origin delta
mismatch** (`ORIGIN-DELTA-LIMIT` in the JSON): absolute admission and physics delta
bounds may disagree between opposite extreme chart endpoints. It has not been
reproduced and is not an established new regression or a shipped correction.


The final source inventory now contains **53 files**, with all hashes verified.
Only packaging verifier/fixture files and one scheduling unit test follow the
unchanged native 26 candidate. A canonical Android ABI alias omission is corrected:
the same actual compiled/signed arm64 APK bytes pass the independent verifier,
and Linux distribution controls pass 16/16. Final rebuilt provenance/device/CI
qualification remains separate; no old native receipt is relabelled.


[Final separate packaging status](PACKAGING_STATUS.json) records the subsequent
final-source APK/SDK qualification. Pending wording in earlier table entries is
historical to the native/tooling consolidation; platform artifact results do not
constitute new device/human acceptance or universal portability.
