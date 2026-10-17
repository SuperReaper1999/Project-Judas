# Pre-Alpha engineering pass 1

Accepted input: `9cf491a13dbb1cd82ab9ab9b916e255af2d579d2` (M98/M99).
Development channel: **Experimental only**. No main promotion, human acceptance,
M100 work or Alpha publication is established by this report.

## What ships

| Workstream | Implemented result | Evidence / boundary |
|---|---|---|
| Build identity | Explicit `0.1.0-prealpha`, independent main/Experimental/Local identities, exact source/toolchain/configuration provenance and native CLI queries | 24 isolated configuration/archive controls; 55 native identity/process/update checks |
| Updates | Optional channel ancestry notification replaces milestone-title guessing and authenticated GitHub CLI dependence | Offline/missing utility/invalid comparison are nonfatal; no updater or repository writes |
| Export | Bounded shell-free runtime probe, strict exporter/runtime matching and runtime SHA-256 provenance | Missing, malformed, oversized, stalled and mismatched tools fail; actual relocated game runs |
| Native distributions | Shared 17-tool inventory, repaired Windows SDK omissions, verified preparation/publication and corruption checks | 23 packaging controls; actual Linux SDK staging verifies 207 payload files; no universal portability claim |
| Source distributions | Deterministic tracked-blob archives, inventory hashes and no-Git provenance with change detection | Clean/dirty/unknown/archive/added-dependency controls; unrelated untracked files are explicitly excluded |
| Android distribution | Project-owned app identity; development/release signing presets, required release credentials, ABI provenance and APK/checksum receipts | Python and native compilation checks are separate from phone acceptance; final artifact scope is recorded below |
| Validation | Opt-in focused native manifest/CTest gates with per-executable provenance, bounded subprocesses, fresh machine-readable evidence and honest failing-contract classification | 22 focused Linux gates pass; 6 runner negative-control tests; stale binary genuinely rejected |
| Developer tooling | Optional pinned TypeScript setup replaces a hardcoded M90 cache prerequisite | 4 resolver controls; TypeScript 5.9.3; type/drift and live VM enumeration agree |
| Integration diagnostics | Socket errors name endpoint/cause/action; replication admission names unsupported component/ownership; origin rejection names the actual blocking subsystem | Real bind-conflict/refused connection and negative replication checks; supported admission semantics unchanged |
| Composed lifecycles | New adversarial terrain/resource/origin/save/destruction regression | 34 checks verify actual collision/navigation/field endpoints and last-good publication, including positive controls |
| UI compatibility | Restored length-bearing label text, including embedded NUL, while preserving TextInput validation | The full cookbook exposed an M96 regression against the accepted M58 contract; original failure retained; no example expectation weakened |
| Maintainability | Shared SDK registry, extracted build/validation CMake modules, shared process probing and Windows argument quoting | Incremental changes; existing large runtime/editor translation units remain substantial maintenance debt |

No JudasJS bindings or gameplay semantics were added. The UI correction restores
an existing label contract; it does not relax editable-field input bounds. Project scripts/assets,
accepted fluid implementations, independent research and protected historical
records are unchanged. Current README, architecture/index and platform guides
link the new contracts. Copyright remains Conner / SuperReaper1999; MIT terms
are unchanged.

## Physics investigation: confirmed failures, rejected repairs

See [the complete collision report](physics/REPORT.md), its exact commands,
patches, paired measurements and [hashed evidence manifest](physics/MANIFEST.json).

The curved-mesh crossings are real. The old test printed the crossing count but
did not fail, and its circle-distance oracle could also misclassify high legal
flight. The replacement measures signed normal depth against the actual cooked
triangle sheet, retaining the original 5 cm threshold and physical inputs.
Independent absent-collider, legal-flight, finite-edge, rear-entry and rotated
controls prevent an oracle-only apparent pass.

On retained production physics:

- Two-step arrival: **20 / 16 / 16** crossings for coarse / medium / dense.
- Original 90-step motion: **24 / 24 / 24** crossings.
- Flat arrival controls have no crossings.
- The integrated manifest uses a 60-step articulation prefix: **372 checks,
  4 failures** for each arrival/continuation invocation. The research report's
  193-check receipts use their recorded shorter prefix; the fast-box contracts
  and crossing results are the same. Neither invocation is counted as passing.

The demonstrated arrival mechanism uses a local concave response plane as though
it separated the complete mesh. Changed nearest features can therefore produce
an unsafe advance. Subsequent glancing/contact continuation is also unresolved.
Three coherent repairs passed first arrival but failed cost/remaining-contact
acceptance. They were reverted, not merged as partial fixes:

| Rejected representation | Matched cost regression |
|---|---|
| Full per-triangle continuation | Ordinary 256-body mesh p95 approximately 2 → 60 ms |
| Narrow per-triangle first arrival | Coarse jointed trough total 298 → 459 ms |
| Scalar distance/speed advancement | Coarse 304 → 478 ms; dense 829 → 1,491 ms |

`src/PhysicsWorld.cpp` and `src/CollisionGeometry.{cpp,h}` are byte-identical to the
accepted baseline. **No physics speedup or CCD correction ships.** The stronger
tests and rejected experiments are useful deliverables, not resolution of the
release blocker.

### Dense-contact cost

The exact preserved M91 1,024-body scene completed all 270 fixed steps and valid
quality/profiler exports. Every trajectory digest and collision counter matched
the original evidence. The 60-second watchdog expired during teardown after the
exports: application exit is **not** a pass.

| Current matched run | Cost |
|---|---:|
| Median fixed step | 31.560 ms |
| p95 | 481.568 ms |
| Worst | 2,119.208 ms |
| Worst-step impacts / geometry queries | 51 / 501,599 |
| Worst island bodies / rows | 973 / 2,517 |

Worst-step inclusive categories: geometry 933.5 ms, island discovery 723.6 ms,
TOI/search 327.7 ms, solving 167.2 ms. These overlap and must not be summed.
The dominant remaining problem is a dense event cascade with repeated geometry
and island work, not rendering. Existing profiler counters were sufficient to
establish this; no profiler rewrite or arbitrary quality reduction was justified.

For contrast, the ordinary Tuppence consumer run measured fixed median **1.1784
ms**, p95 **5.1036 ms**, maximum **33.5700 ms**, frame-work p95 **6.1928 ms** over
1,048 samples. This is a current workload observation, **not** a comparable
before/after speedup or a guarantee for the dense scene. Host/build metadata and
raw per-workload evidence are preserved.

## Final combined Linux validation

This was an incremental Release build with the actual changed engine/tools and
selected dependencies rebuilt. It was not a clean full-engine build or full
production-suite run. Final affected builds report no compiler warnings/errors;
existing third-party CMake minimum-version deprecation messages are distinct.
GitHub's clean Ubuntu CI workflow is configured but has not been observed passing
as part of this local report.

- **22/22 focused gates:** collision geometry/contact/solver/timing/continuity,
  CharacterMotor, physical continuation, contacts, composed publication,
  origin physics/assemblies/participants/streaming, edited terrain, save
  evolution, actual network transport/session/boundaries/JS and GL resource handoff.
- Supplementary regressions: impact robustness **4,412**; kinematic **96**;
  imported ragdolls **93**; M77 physical balance **119**; joints **29**;
  contact cache **13,327**; prefab **37**; save storage **38**;
  ragdoll **36** checks, all passing. Script debugging/reload **49** checks pass
  in its required repository working directory.
- Actual public surface: **473 symbols / 40 exports / 311 native operations /
  17 callbacks**, with 4 negative drift controls and 16 structured result checks.
  Fifty cookbook files are type-checked; full real-VM cookbook execution passes **312** checks after the narrow UI repair. Text/localization, localization lifecycle and UI production regressions pass **49 / 41 / 60** checks. Live enumeration is a separate real-VM
  check, not proof of every member's behaviour.
- Two independent Connected Workshop processes pass; real application checks
  pass **34** checks. Cross-version v1 creation, v2 and v3 cold restoration pass
  **16 / 15 / 16** checks. Missing-mod and invalid native-state restoration fail
  safely as intended (**8 / 7** checks), preserving original bytes/live world.
- Editor authored scene is identical after Play/Stop and explicit save succeeds.
- Ordinary Tuppence startup, UI/gameplay, physical interaction, pause/save/load,
  mode transition and shutdown pass **30** checks.
- Desktop export includes verified runtime provenance; the renamed Unicode/path-
  with-spaces package runs from an unrelated working directory and captures a frame.
- Both curved-mesh contracts return nonzero as described above. FLU-001 is
  deliberately not rerun or counted as passing.

Reproduction: focused build/CTest commands are in [PREALPHA.md](../../PREALPHA.md).
[reproduce_integration.py](reproduce_integration.py) records exact supplemental,
application and relocation commands without building or modifying fixtures.
The manifests, logs, executable hashes and results are under [validation/](validation/).
Post-checkpoint clean-identity/package/archive receipts and generated artifacts
are retained in `.cache/prealpha/` and reported in the operator handoff; those
artifacts must identify the source commit they actually built.

### Genuine failures and follow-ups retained

- Original curved crossings and each rejected repair are preserved, not rewritten.
- Initial real socket tests were sandbox-denied. The authorized loopback-enabled
  rerun passes; the original logs remain classified as environment failures.
- The new atmosphere negative-control fixture initially omitted its required
  celestial body. Added the normal static body; the supported-build control and
  unchanged-chart rejection now pass. No atmosphere runtime correction was made.
- The debugger test was initially launched from an isolated directory despite its
  repository-relative project fixture. Correct working-directory rerun passes;
  neither its test nor its expectations were weakened.
- A first build command named an absent old target and was corrected. A stale
  application-object collision experiment failed before physics because of an
  ABI mismatch; it is not counted as evidence of a physics regression.
- Review strengthened probe-to-execution byte binding and source-archive extra-
  dependency detection. Windows pipe-exit draining was corrected before shipping;
  native Windows execution remains unavailable, not implicitly passed by Linux.
- Full cookbook execution originally failed **311 checks / 2 failures** because
  M96's setter rejected embedded NUL in every UI element. Historical M58 evidence
  and ordinary UIDocument validation both preserve NUL in labels; only editable
  fields reject it. The generic setter now follows that distinction. The original
  localization example/check stays unchanged; additional NUL/CR/LF/capacity
  negative controls verify TextInput rejection preserves the previous value.

## Platform and release coverage

| Platform / artifact | Actual status |
|---|---|
| Native Linux | Acceptance platform; focused and real-project checks above pass |
| Host-built Linux SDK | 17 tools, 207 payload files, approximately 895 MiB allocated (exact final sizes are in its SDK_MANIFEST.json); verified inventories and matching tool identities |
| Linux ABI portability | No missing host libraries; requires GLIBC **2.43**, GLIBCXX **3.4.32**, CXXABI **1.3.15** on this Ubuntu 26.04.1 host. Not certified for older distributions; a declared older-baseline build is still needed |
| Windows | Packaging/DLL omission controls pass synthetically. Native checks **not run**: existing VM cannot start because configured `Win10_22H2_English_x64v1.iso` is missing. No VM configuration/old-build changes made |
| Android | Signing/version/identity controls pass; arm64 NDK compilation checked. Final signed development APK build is a post-checkpoint artifact check recorded separately; no new phone/hardware acceptance or Windows GPU claim |
| Reproducibility | Metadata, inventories and source-archive bytes are deterministic under recorded inputs. No cross-toolchain/path byte-identical native-binary or APK-signature promise |

Binary checksums identify exact artifacts. Compiler identity/configuration are
recorded, but the metadata is not a complete hermetic compiler/linker recipe.
The 17 statically linked tools duplicate substantial engine/ICU bytes; this pass
makes the SDK complete and auditable, not compact. No installer, universal Linux
runtime bundling, release signing authority or public Alpha distribution is added.

## Ranked remaining work / blockers

1. **Curved concave CCD correctness:** reuse finite patch identity and a safe
   continuation representation without repeating a full per-triangle TOI solve.
   Require both retained crossing contracts and matched articulation/mesh costs.
2. **Dense event-cascade scaling and teardown:** repeated geometry/island work
   creates multi-second tails. Preserve quality/counters; profile reusable work
   and causal invalidation before another repair or parallelism claim.
3. **Declared distribution baselines:** reproducible clean Linux baseline SDK,
   native current Windows build/functional checks, Android hardware coverage and
   controlled signing/distribution credentials. Current host SDK is not universal.
4. **Composition boundaries:** liquids/atmosphere/combustion still block live
   origins; networking still excludes articulated/mechanical/terrain state and
   arbitrary runtime-spawned identities. Diagnostics are better; contracts were
   not expanded or silently bypassed.
5. **Maintainability / onboarding:** large RuntimeWorld/ScriptSystem/editor/build
   files still concentrate ownership; continue extracting measured shared
   registries and supported first-project workflows without indiscriminate rewrites.
6. **Previously deferred:** FLU-001 remains failed/deferred, the original a106 save
   lacks historical continuation data, and broad per-member/platform coverage is
   incomplete. No fluid research or old save history was invented or altered.

**Checkpoint assessment:** release/tooling/diagnostic corrections are suitable for
an Experimental checkpoint based on scoped Linux evidence. Judas is not Alpha-
ready or comprehensively validated. Main requires acceptance of the exact future
candidate; this checkpoint does not request or imply it.

## Consolidated human acceptance checklist

1. Open the editor and inspect its version/channel/build; query runtime/tool JSON.
2. Try the manual update check both online and offline; editor stays usable.
3. Open an existing game; Play/Stop, inspect/debug and deliberately reload a script.
4. Open JudasJS declarations/examples in an IDE using the documented optional setup.
5. Export a game, move its folder, launch from elsewhere and save/reload progress.
6. Run the two-process Connected Workshop and cross-version Save Evolution labs.
7. Inspect the known-failure manifest: a green normal gate must not imply CCD or
   dense-contact closure. Heavy stress workloads still have severe stalls.
8. Test the final Android APK on supported hardware when convenient; restore the
   Windows VM's external setup separately before native Windows acceptance.

No main promotion, protected-history edits, independent-fluid changes, existing-
game edits, milestone tag or M100/Alpha work belongs to this pass.
