# Current V2 findings

Authoritative dispositions are in [CURRENT_FINDINGS.json](CURRENT_FINDINGS.json).
Retired findings and original evidence are recoverable at their historical Git revisions.

<!-- judas-findings:start -->
| ID | Status | Current scope / disposition |
| --- | --- | --- |
| FLUID-LOCK | retired | Obsolete V1-era channel restriction removed. Accepted Advanced Fluids V2 is available through explicit project opt-in in every V2 build channel. V2 remains on Experimental; main and existing packages are unchanged. |
| TERRAIN-EDITOR | scoped measured improvement | Hidden Game rendering and editing-preview LOD were corrected. Current whole-frame SDK editor/standalone observations are recorded in docs/WHOLE_ENGINE_PERFORMANCE.md; different viewport sizes and workloads do not establish universal editor/runtime parity. |
| BALANCE-RECOVERY | deferred | Humanoid get-up and animation blending remain limited; no claim of general natural physical recovery. |
| PLATFORMS | scoped | Linux qualification is current per purge evidence. Earlier Windows/Android hardware acceptance does not certify this changed checkpoint. |
| GPU-FALLBACKS | required | Portable rendering and CPU particle simulation/collision remain legitimate capability fallbacks, not historical compatibility. The whole-engine audit qualifies a real 5,000-particle CPU fallback separately from unsupported required-GPU authoring. |
| MK2-BOUNDS | documented | Content-dependent dense contact and CCD safety limits remain; removal of MK1 diagnostics does not change MK2 algorithms. |
| PURGE | human accepted | The operator accepted the architectural purge before the Vehicles/Wheels V2 milestone. This records that decision; it does not promote main or publish a V2 release. |
| GPU-CAPTURE-REPEATABILITY | unconfirmed | One local NVIDIA terrain depth capture and one first-dispatch one-slot particle test failed in the purge full run; unchanged isolated reruns passed. Cause remains unestablished. Assertions and GPU algorithms remain unchanged; see purge qualification evidence. |
| MOD-TERRAIN | unsupported | The current mod format does not admit native Terrain V2 assets; ordinary cooked mesh/scene mods and base-game terrain remain. Retired M75 fork/cook workflows are not retained. |
| WHOLE-FRAME-BUDGET | measured limitations | Whole-engine audit improved ordinary gameplay without changing accepted physical outcomes. Severe CCD impacts, IK tails and queue-inclusive completion still miss defined 120/60 FPS or 5 ms goals; see docs/WHOLE_ENGINE_PERFORMANCE.md. Functional or relative p99 passes are not universal performance qualification. |
| PRIVATE-ASSET-FOOTPRINT | measured limitation | The local four-model yard retains about 3.12 GiB process RSS plus swap, including decoded images needed for resource recreation. Active pinned assets exceed the cache target. This is a footprint follow-up, not proof of unbounded growth; private models remain outside Git and SDKs. |
<!-- judas-findings:end -->
