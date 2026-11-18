# Current V2 limitations and implementation findings

[CURRENT_FINDINGS.json](CURRENT_FINDINGS.json) owns these dispositions. Qualification
is scoped in [development readiness](DEVELOPMENT_READINESS.md); a limitation is not
silently resolved by removing an old report or passing an unrelated test.

<!-- judas-findings:start -->
| ID | Status | Current scope / disposition |
| --- | --- | --- |
| TERRAIN-EDITOR | scoped measured improvement | Hidden Game rendering and editing-preview LOD were corrected. Current whole-frame SDK editor/standalone observations are recorded in docs/WHOLE_ENGINE_PERFORMANCE.md; different viewport sizes and workloads do not establish universal editor/runtime parity. |
| BALANCE-RECOVERY | deferred | Humanoid get-up and animation blending remain limited; no claim of general natural physical recovery. |
| PLATFORMS | scoped | Linux is the sole official platform. Native Linux integrated development and Ubuntu baseline checks passed at the recorded V2 baseline. Windows results are historical and Windows is unsupported. Android and browser runtime remain unqualified for this snapshot. |
| GPU-FALLBACKS | required | Portable rendering and CPU particle simulation/collision remain legitimate capability fallbacks, not historical compatibility. The whole-engine audit qualifies a real 5,000-particle CPU fallback separately from unsupported required-GPU authoring. |
| MK2-BOUNDS | documented | Content-dependent dense contact and CCD safety limits remain; removal of MK1 diagnostics does not change MK2 algorithms. |
| GPU-CAPTURE-REPEATABILITY | unconfirmed | Prior one-slot particle first-capture and hidden-window terrain readback failures did not recur in isolated repeats. The terrain comparison now owns an image target; no universal driver/particle repeatability repair is claimed. Retain live assertions and investigate a new reproduction. |
| MOD-TERRAIN | unsupported | The current mod format does not admit native Terrain V2 assets; ordinary cooked mesh/scene mods and base-game terrain remain. Retired M75 fork/cook workflows are not retained. |
| WHOLE-FRAME-BUDGET | measured limitations | Whole-engine audit improved ordinary gameplay without changing accepted physical outcomes. Severe CCD impacts, IK tails and queue-inclusive completion still miss defined 120/60 FPS or 5 ms goals; see docs/WHOLE_ENGINE_PERFORMANCE.md. Functional or relative p99 passes are not universal performance qualification. |
| PRIVATE-ASSET-FOOTPRINT | measured limitation | Large active private model/image sets can exceed resource cache targets because live pinned data cannot be evicted. This is content-dependent footprint, not evidence of unbounded growth; private assets remain outside Git/SDKs. |
| LEGACY-PRESENTATION-TEST | unregistered stale target | CMake still declares judas_script_presentation_tests, whose source copies removed projects/shooter_game and projects/boundary_demo and missing docs/evidence/m52 originals. It is absent from the supported validation manifest. Documentation cleanup does not repair or certify this target; reconcile or retire it as a separate implementation task. |
<!-- judas-findings:end -->
