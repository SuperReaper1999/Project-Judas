# M67 genuine failures and corrections

These original records are retained; passing results do not erase failures.

| Record | Cause / correction |
|---|---|
| first-expanded-checks.log / roundtrip-followup.log | Static triangle volume assertion was inappropriate; JSON normalization discarded negative zero used by unchanged legacy fingerprints. Correct independent expectation and preserve -0. |
| reference-remap-first.log | Zero template identity remapped null texture-camera references to the generated entity. Remap only nonzero source identities. |
| cookbook-first.log | Example used nonexistent cameraInfo rather than public Entity.camera; test dereferenced null after VM error. Correct public example and guard failure reporting. |
| lazy-body-count-first.log | Count preceded normal asynchronous resource/proxy readiness. Bounded ordinary Prepare/WaitAll before count. |
| document-family-first.log / document-family-followup.log | Unsupported script soft-reference fixture and parent-property expectation corrected; parent edits use dedicated cycle-checked command. Real editor equality omitted organizational metadata; equality now includes it without changing fingerprint schema. |
| cold-load-first.log | SaveService participant-count guard omitted optional view-projection chunk. Admit the explicit bounded chunk; save/fingerprint versions unchanged. |
| cold-observer-first.log | Save-restore observer ran before first normal script synchronization. Wait for restored instances; no fake lifecycle ordering guarantee. |
| geometry-repeatability-first.log | Same new geometry recipe inputs minted different asset IDs. Immutable new revision IDs now derive from recipe ID, content digest and product role; matching accepted revisions retain their existing IDs. Focused shared evaluator/cook checks and editor/CLI follow-up cover final bytes. |
| moved-pause-first.log | Actual exported workshop immediately resumed because Resume focus and opening back were handled as activation. Correct project JS event-type/one-frame opening guard; add positive paused assertion. Runtime UI/input unchanged. Affected application/cold-load/editor/package checks refreshed; full suite not repeated. |
| ui-output-path-first.log | Editor follow-up runner omitted its fresh output directory, so the requested scene save failed. Create the directory before launch; no serializer/runtime correction. Successful final editor run saves the same named scene and retains authored Play-Stop identity. |
| workflow-export-first.log | Runner supplied nonexistent data/ instead of the engine root containing assets/fonts. Export correctly failed. Correct invocation, then affected exports/startup only; no runtime correction. |
| ../final/production/build.log | First clean Release build stopped on unresolved scene-codec symbols in standalone InputSystem targets. No suites ran. Shared bounded JSON/input helpers remove scene/runtime linkage from input-only consumers. Same clean build resumed in final-followup; nothing was accepted from stale binaries. |

Before the resumed gate, destination-byte capture was moved before expensive CLI
conversion, the world-building window was given a readable initial size, and the
authoring editor smoke stopped covering the viewport with the profiler. These
changes are included in that gate's frozen source, not unrecorded late changes.

Later visual inspection found that controls and the canvas were buried inside the
short asset-browser region. The UI document now opens in a separate resizable
two-column window. Final actual-editor screenshots show the normal RTL/localized
RuntimeUI canvas beside its controls; shared command and Play-Stop/save checks pass.
Native human widget-click acceptance remains pending rather than being inferred from
those screenshots. Only the affected editor/offline/export checks were repeated.
