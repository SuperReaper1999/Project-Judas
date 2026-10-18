# Pass 3 maintainability workstream

Baseline: `f647d7fd95f60d3a37dba70a51f4e45dc41c2922` on Experimental. This report is a scoped engineering record, not whole-engine certification or human acceptance.

## Changes

- `ScriptBindingsUI.cpp` owns the existing nine UI bridge operations. `ScriptBindingsJoint.cpp` owns the existing seven joint operations. Each is a real translation unit, behind a small typed internal interface; neither sees `ScriptSystem::Impl`. ScriptSystem retains VM ownership, instance/lifecycle state, callback phases, native-operation ordering and error isolation. Exact operation whitelists preserve the former unknown-operation path. UI borrows live owner/slot fields instead of copying them, preserving nested-coercion observations. No public JudasJS binding is added.
- `ScriptBindingValues.h` shares the exact former String/Number/ReadVec conversion implementations. Length-bearing strings, coercion order, numeric bounds, value assignment and QuickJS release behavior are retained. This prevents copies diverging as more bridges are extracted.
- `editor/RenderPreview.cpp` owns runtime appearance/material/visibility preview and its undo/redo history. Only the two existing drawing functions cross the private editor header. History is still per document/world; authored scene state and Stop behavior are unchanged. The stale EditorPanels header claim that no panel touches RuntimeWorld was corrected to explain this actual boundary.
- The complete WorldOrigin.cpp transaction module is mechanically formatted after the parent's functional OriginLimits correction: 111 to 288 lines, all 2,001 tokens preserved and all 16 lines over 140 columns removed. Its separate MECHANICAL_ORIGIN.patch records that boundary.
- Public ScriptSystem lifecycle, state, collision/trigger, origin and fracture callback code is mechanically reformatted to readable control flow. New bridge/preview modules are formatted with pinned temporary clang-format 18.1.8, 100-column target, four spaces, no include sorting, no string-literal rewriting. Untouched monolithic binding sections were not wholesale formatted.
- 175 ordinary engine-test declarations now use the existing `judas_add_engine_test` helper. Four coherent CMake modules collect local views/input, rendering/assets, developer/animation/UI tools, and world/persistence/network declarations. Production tools and their target order remain present; SDK inventory finalization stays after all declarations. There is no target-array framework or documentation generator.
- API extraction reads the two explicitly named bridge files, and the inventory's source-authority string is updated. The public module text, declarations, operation set and reference API are unchanged.

## Readability evidence

| File | Before | After | Meaning |
| --- | --- | --- | --- |
| CMakeLists.txt | 2,032 lines / 106,759 bytes | 1,597 lines / 77,892 bytes, before unrelated Pass 3 target additions | Repeated source/link/warning boilerplate centralized; conditional flags retained |
| ScriptSystem.cpp | 2,107 lines; 502 over 140 columns | 2,634 lines; 416 over 140 columns | Formatting intentionally adds lines; lifecycle control flow is readable and UI/joint details move to cohesive files |
| EditorPanels.cpp | 1,606 lines / 142,914 bytes | 1,496 lines / 131,361 bytes | Preview/history ownership is isolated |
| New ScriptBindingsUI.cpp | Existing 84 dense lines | ~355 readable lines, no lines over 140 | Text/asset/bounds/lifetime branches can be audited independently |
| New ScriptBindingsJoint.cpp | Existing 42 dense lines | 202 readable lines, no lines over 140 | Participant validation, stale handles and configuration are explicit |
| New RenderPreview.cpp | Existing ~110 dense lines | 324 readable lines, no lines over 140 | Preview updates and undo/history are together |

Concrete examples: the former single-line fracture callback and slot replacement cleanup now show lifecycle/error paths across normal lines; UI text validation distinguishes UTF-8, NUL/newline and editable-field limits; joint create/configure checks remain adjacent to participant ownership rather than buried in unrelated dispatch branches.

These are incremental improvements. ScriptSystem still has a large native dispatcher, RuntimeWorld/EditorApplication remain substantial, and component lifecycle bookkeeping is not universally consolidated.

## Behavior-preservation evidence

1. Reconstruct the original Native dispatcher from the extracted bridge branches, reversing only the explicit context field spelling. Compare all original tokens, including guard order/error strings. Then reconstruct the complete ScriptSystem and compare all **63,499 tokens**, including formatted lifecycle methods. Exact match.
2. Compare bodies of String/Number/ReadVec and seven editor preview/history functions. Exact match. Four deliberate changed-validation/ownership/diagnostic/lifetime controls are rejected.
3. Separate `EXTRACTION.patch` and `MECHANICAL_FORMAT.patch`; all seven mechanically formatted files retain exactly the same token sequences. The mechanical patch reverses cleanly against the candidate.
4. Expand the 175 helper invocations and four CMake modules, account only for the three new .cpp entries, and compare the complete original CMake token stream: **6,646 tokens**, exact match. Missing application flags, warning changes and a removed target are detected.
5. Execute original and extracted tail declarations with CMake itself, using compiler-free command observers. Linux: 131 calls / 43 targets; Windows: 126 / 41; Android conditions: 125 / 41. Sources, link arguments, warning arguments, include directories, compile definitions, properties, order and platform guards match exactly. These are declaration checks, not native Windows/Android certification.
6. Permanent CMake contract checks: 12 tests, including six retained and six new positive/negative checks. Loss of application sources, engine ownership, M92 definitions, Windows socket linkage and the Android performance guard is detected.
7. Existing API/type drift check passes with the unchanged public surface and all 51 declared examples represented.

## Matched native observations

Cache-only observers use the unchanged test assertions, timestep, inputs and fixtures. Only output paths and extra state prints differ. Test sources are checked byte-for-byte against HEAD; four unchanged Application support objects were freshly compiled once and reused. Baseline links the frozen f647 archive (SHA-256 `72abdc886dddded90ee8d5379ec81d2c0bbb7fc1d0df594c60259b7cbe1bfe69`); exact command/source/dependency/binary hashes are recorded.

Baseline passed: Script 35 checks; debug/reload 49; real UI 22; real joint Application 8 (114 total). Traces include independent lifecycle counters, impulse/velocity/input JSON, actual breakpoint execution count 25 then retained/reloaded count 85, two UI document states after actual events, and physical joint coordinate/impulse at frame 3. The identical cached observer objects were relinked against the combined functional archive: all 114 checks pass and all nine state/physical observations match the frozen baseline byte-for-byte. This comparison preceded the subsequent token-identical WorldOrigin formatting; post-format original regressions are the final-source validation, recorded by the parent. Baseline timings are diagnostic; no speedup is claimed from these checks.

Final post-format Linux validation is now recorded in the [combined result index](../final-linux/RESULTS.json): the 21-contract focused manifest and 21 supplemental records passed, including script/debug/reload/UI and cold-save cases. The cookbook passed 312 checks. API/type verification reports 473 public symbols, 40 exports, all 51 typed examples and live enumeration. Actual editor Play/Stop preserved identical authored scene bytes, and the relocated standalone export ran successfully. These scoped passes do not certify every engine feature or human preview-control acceptance. No performance speedup is claimed from observer timings recorded during concurrent compilation.

[Reproduction instructions](REPRODUCTION.md) distinguish compiler-free proofs, native build/link prerequisites, rebuilding the frozen f647 archive, the baseline override and observer modes. The evidence helper's missing-cache-parent setup issue was corrected and verified with independent old-failure/new-directory controls; no native suite was rerun for that evidence-only correction.

## Development records and scope

The first cache extraction script mistakenly searched a marker after replacing its containing range; it failed before publishing the transformed CMakeLists. Offsets are now calculated before replacement. A formatting negative control initially made no mutation because it matched old whitespace; its own failure exposed that issue. Regex-based mutation plus an explicit changed-source assertion fixes the control. Neither incident is an engine defect; the recorded first formatting-control diagnostic is explicitly transcribed in development/FORMAT_CONTROL_FAILURE.txt; no engine failure is invented.

No commit, push, main promotion, historical evidence edit or independent fluid-research access was performed by this workstream. Root owns final combined validation and publication. Suggested human checks: inspect the readable UI/joint/lifecycle source; use normal HUD/menu input; create/configure/destroy a joint; debug/reload a script; inspect runtime material/environment, preview undo/redo, then Stop and confirm authored values remain unchanged.
