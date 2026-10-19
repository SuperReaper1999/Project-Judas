# Pre-Alpha documentation, API and test cleanup

Input: Experimental `be1b7d634c674dca34f3a36299da510492ca2aea`.
Main remains the accepted `e30d5f9bb72a1a341d3eff7485b8d6b3eb6f8ea9`.
This is a scoped cleanup, not human acceptance, M100 or an Alpha release.

## Current guidance

The [documentation index](README.md) groups developer workflows and subsystem
references. [Architecture](ARCHITECTURE.md) now describes current owners, phases,
source modules and integration boundaries. Its previous 10,248-line chronology
is preserved verbatim after a historical banner in
[Architecture history](ARCHITECTURE_HISTORY.md), with a payload-hash receipt.
The archived README, FTFT status and all prior evidence remain unchanged.

Current guides correct obsolete candidate/CCD labels, editor script reload/update
notifications, modern save callbacks, Android export flags, and the scope of
Windows/Android evidence. Historical failure logs are not rewritten as passes.
[Current Pre-Alpha status](PREALPHA.md) separates the corrected original four
curved-mesh contracts from continuing CCD limits and dense-contact stalls.

## API consistency

The runtime API is unchanged. Source inspection corrects two declaration defects:

- `world.setView` accepts a range with only `near` or `far`; native defaults fill
  the omitted field. The shared `CameraRange` typing now matches that behavior.
- `Entity.physicalMaterial` can return `null` when no physical material is
  available; scripts must guard the result.

The API checker retains source/live enumeration, getter/setter, structured-result
and example checks, and adds callable argument bounds plus explicit dynamic
exceptions and compile-only positive/negative type contracts. The inventory links
component/world operations to their actual subsystem reference pages.

The native interfaces and public runtime registration are preserved; no new
binding, compatibility alias, feature or C++ runtime behavior is introduced.
Native headers are actual interfaces, not a stable binary ABI promise.

## Test consolidation and dead wiring

The active native test targets are retained. Similar names do not imply redundant
coverage: the allocation-instrumented physics benchmark is deliberately distinct,
and shared executables can exercise different arguments/contracts. Standalone
historical adapters remain tied to their original research/evidence.

One identical early Python CI invocation is removed; CTest remains its single
authoritative invocation. The focused manifest is validated by both CLI and CMake
before registration. Malformed arguments/manual flags/groups, unknown placeholders
and duplicate invocations now fail instead of being silently misinterpreted.
Configure-only tests inspect real CTest registration, including known-failure
opt-in and unique-target build dependencies. No failing fixture is removed. The 313 literal native target declarations and
312 entry points remain; the focused manifest stays byte-identical at 45 cases /
39 binaries (44 ordinary, one explicit manual failure).

The only removed native build statement is a redundant GLM self-assignment.
Android's explicitly selected target and desktop fallback order remain intact.
No runtime dead-code claim or broad refactor is made without proof.
The clean build also exposed two existing misleading-indentation warnings in
RuntimeWorld material setters. Separating their statements changes whitespace
only; the complete file matches its parent after whitespace removal. Material
regressions and final recompilation qualify that narrow readability correction.

Source archives now include current top-level JSON documentation. The shared SDK
inventory includes the current machine-readable finding register; real staging/
relocation and failed-replacement controls preserve its bytes and the last-good SDK.

## Documentation maintenance

`python3 scripts/check_current_docs.py` checks an explicit current-reference set:
local destinations, Markdown anchors and repository path boundaries. CI also
requires linked destinations to be tracked. Positive and negative unit controls
exercise missing files/headings, untracked references and escaping paths.
A small documentation workflow runs this on Markdown changes without triggering
a full native build. The API checker remains a separate structural contract.
Neither tool proves prose semantics, numerical fidelity or platform acceptance.

## Validation

Final-source [results](evidence/prealpha-doc-cleanup-20261019/RESULTS.json),
[source fingerprints](evidence/prealpha-doc-cleanup-20261019/SOURCE_FREEZE.json)
and [binary fingerprints](evidence/prealpha-doc-cleanup-20261019/BINARY_FINGERPRINTS.json)
are preserved with exact commands/logs.

| Check | Observed result |
|---|---|
| Clean Release build | New build directory; final rebuild/runtime build have zero warnings. Two initial warnings and the token/object preservation proof remain recorded. |
| Focused gates | 43/47 passed in the restricted environment. Socket creation independently fails with EPERM there; the same four networking binaries pass natively. All 47 selected contracts qualify across those recorded environments, not one falsely green CTest run. |
| API/types | 40 exports, 473 symbols, 244 callable bounds, 17 selected shapes, 51 JS examples typechecked, seven drift negatives; actual VM enumeration matches. |
| Executed cookbook | 38 cases / 312 checks, zero failures. |
| Debugger/reload | Core scripts 35, debugger/reload 49, editor debugger 20, four real consumer workflows 37 checks; all pass. |
| Materials | 40 checks pass; complete RuntimeWorld object bytes identical before/after whitespace-only source correction. |
| Tooling | 26 gate/registration/CMake, 10 docs-parser, four TypeScript-loader, 28 packaging and 16 distribution controls pass. |
| Editor | Offscreen and actual X11 desktop batch/undo/Play/Stop/save pass; authored scene remains identical. Offscreen relative-mouse limitation is preserved; native follow-up has no capture failure. |
| Standalone | Five assets/three scenes exported; read-only moved package starts from unrelated `/tmp` cwd and closes its own window normally, exit 0. |
| Preserved failure | Half-turn liquid query still exits 1 at its original assertion: cell 9 becomes 10. It is not counted as an engine pass. |

All 1,358 frozen source/tool/API inputs still match; 49 actual executable hashes
are recorded. Binaries embed the tested dirty `be1b7d63` candidate identity, not a
retroactively substituted checkpoint hash. This is a development export, not a
clean signed release. Initial missing Ninja and omitted runtime-target setup
failures are preserved with their narrow follow-ups.

No new performance claim is made; the dense measurements in current guidance
are explicitly carried forward. Windows/Android evidence was source-reviewed,
not freshly executed or upgraded to hardware acceptance. The full production
suite was not rerun.

## Remaining concerns

See [every original-audit finding](PREALPHA_FINDINGS.md) and
[current limits](PREALPHA.md#remaining-readiness-concerns). Dense-contact latency,
FLU-001, legacy missing continuation, half-turn liquid query identity, unsupported
compositions and platform hardware coverage are not closed by documentation work.

## Human review

1. Start at the documentation index and follow First Project, Architecture and JudasJS.
2. Search an API, inspect its declaration and copy a cookbook example into an editor.
3. Open a current project, Play/Stop, inspect an error and deliberately reload a script.
4. Verify current limits and platform evidence distinguish supported behavior from acceptance.
5. Launch the relocated standalone and inspect its identity/controls.
6. Review the one retired CI invocation and retained unique/native/failing coverage.

Main promotion requires explicit human acceptance of this exact candidate.
