# Pre-Alpha assessment

**Baseline:** `9cf491a13dbb1cd82ab9ab9b916e255af2d579d2`

`Experimental`, `main` and their locally recorded origin references are aligned. The tracked worktree is clean.

**My assessment:** Judas has a substantial, usable engine foundation. The main Pre-Alpha work should now be **making its behaviour, integrations and releases dependable**, rather than adding another collection of features.

I recommend incremental architectural corrections, with targeted redesign where measurements justify it. I do **not** see evidence supporting a wholesale engine rewrite.

This assessment used source inspection and preserved evidence. **I did not modify files, build, run tests, benchmark, commit or push.** Historical measurements below were not freshly reproduced.

## 1. What is already solid

| Area | Assessment | Important boundary |
|---|---|---|
| Engine/game separation | Strong foundation: JavaScript owns gameplay; native systems provide reusable mechanisms. | Historical compatibility controllers still exist and need their own regression coverage. |
| Physics and machinery | Broad capabilities: rigid bodies, joints, characters, articulation, deformation, structures, wheels and powertrains. | Dense-contact worst cases remain substantially outside real-time budgets. |
| Large worlds | Precision-safe planetary terrain, runtime LOD, local collision frames and live origin changes are implemented. | Not every subsystem can participate; one active physics chart is not an entire simultaneously simulated planet. |
| Persistence | Versioned archives, migrations, recovery, cold saves and retained regional state are meaningful implementations. | Native topology changes and migration of opaque inactive-region state remain restricted. |
| Rendering/assets | Instancing, per-view light selection, cooked textures, mesh LOD and residency management are implemented. | Lighting, residency and batching still have explicit limits. |
| Animation/UI/tools | Animation events, root-motion integration, timeline tools, debugging, editable text and virtual lists now exist. | Several advanced workflows remain bounded rather than universal. |
| Multiplayer | Local multiplayer and a functional optional online foundation exist. | Online replication deliberately covers a narrow entity subset. |
| Portability | Linux, Windows and Android have real implementations and validation history. | Latest-platform coverage is uneven; historical passes do not certify every subsequent feature. |

Useful architectural foundations include the authored-scene/runtime separation, generation-safe handles, explicit simulation ordering, owner-thread publication and private preparation before publishing replacement state. These are worth preserving.

Sources: [Scene ownership](/home/conner/Documents/GitHub/Project-Judas/src/Scene.h:36), [simulation ordering](/home/conner/Documents/GitHub/Project-Judas/src/Simulation.cpp), [save architecture](/home/conner/Documents/GitHub/Project-Judas/src/WorldPersistence.h).

## 2. Highest-priority confirmed findings

### A. Collision evidence contains an important false-green result

The preserved M91 mesh-cost run reports:

- Primitive floor and flat sheets: **zero fast-box crossings**.
- All three curved-trough resolutions: **24 fast-box crossings each**.
- Overall executable result: **311 checks, zero failures**.

The crossing measurement is explicitly **reported rather than asserted**. Consequently, a passing executable does not establish correctness for that case.

The relevant collision implementation files have not changed since that evidence. This is a strong unresolved finding, although I have not freshly reproduced it or established its root cause during this assessment.

**Recommended action:** first validate the crossing oracle against the intended mesh collision contract, then establish a discriminating failing regression and correct the generic mechanism. Preserve the shapes, speeds, timestep and original evidence.

Sources: [preserved result](/home/conner/Documents/GitHub/Project-Judas/docs/evidence/m91/raw/verified-native/impact_mesh_cost.log), [diagnostic-only test](/home/conner/Documents/GitHub/Project-Judas/tests/ImpactMeshCostTests.cpp:130).

### B. Dense-contact physics still has severe latency tails

M91 produced genuine improvements, but also preserved substantial limits:

| Recorded workload | Result |
|---|---|
| Tuppence ball mode | p95 improved **30.805 → 5.023 ms** |
| Tuppence coin mode | p95 **28.533 → 27.309 ms**; worst remained approximately **110 ms** |
| Dense 1,024-body pile, matched 155 steps | p95 **63.740 → 45.495 ms** |
| Longer final 240-step pile run | p95 **157.037 ms**, maximum **479.904 ms** |
| Instrumented final 270-step diagnostic | Worst step **2,023.486 ms** |
| Ordinary M91 watch-tour workload | p95 **3.038 ms** |

The ordinary demonstration passing comfortably does not close the dense workload.

Contact generation already has parallel work. “Add multithreading” alone is therefore not an adequate plan. The evidence points toward expensive repeated impact-island discovery, geometry queries and event processing requiring further architectural investigation.

**Recommended action:** establish supported workload envelopes and investigate incremental island/event work before another broad optimization pass. Preserve contact fidelity and CCD requirements.

Source: [M91 measurements and limitations](/home/conner/Documents/GitHub/Project-Judas/docs/evidence/m91/REPORT.md:78).

### C. Build identity is unsuitable for unnumbered stabilization releases

The editor’s current identity is inferred from milestone numbers in recent Git commit subjects, with an **M76 fallback** when suitable history is unavailable.

Problems include:

- Unnumbered Pre-Alpha corrections cannot be identified reliably.
- Source archives can acquire the fallback identity.
- Reconfiguration watches `main` references but does not explicitly watch the Experimental branch reference.
- Runtime `--build-info` identifies platform/configuration, not the exact engine build.
- Export runtime probing cannot establish precise engine/tool compatibility.
- Update notification compares milestone numbers from `main`, so newer unnumbered stabilization builds can remain invisible.

These are concrete release-engineering gaps.

Sources: [identity generation](/home/conner/Documents/GitHub/Project-Judas/CMakeLists.txt:362), [runtime identification](/home/conner/Documents/GitHub/Project-Judas/src/main.cpp:20), [export validation](/home/conner/Documents/GitHub/Project-Judas/src/ProjectExporter.cpp:54), [update notification](/home/conner/Documents/GitHub/Project-Judas/src/editor/EditorMilestoneNotice.cpp).

### D. Windows SDK packaging has fallen behind the engine

The Windows SDK uses an explicit staging list that omits newer tools, including:

- `judas_cook`
- `judas_script_check`
- `judas_save`
- `judas_server`

The SDK configuration is included before later targets are declared, so its one-time target scan also misses later additions.

This establishes a **distribution completeness problem**, not a claim that the latest Windows runtime crashes.

**Recommended action:** make tool installation, dependencies, notices and documentation come from a maintained packaging inventory, with clean-machine checks for every shipped executable.

Source: [Windows SDK staging](/home/conner/Documents/GitHub/Project-Judas/cmake/JudasWindowsSdk.cmake).

### E. Several important systems still cannot compose freely

Confirmed restrictions include:

- Live origin changes reject active liquids, legacy fluid, atmosphere and combustion.
- Regional suspension pins unsupported liquid groups and certain active navigation/articulation states.
- Runtime excavation supports bounded local volumes, not arbitrary editing of every planetary terrain representation.
- Online replication excludes vehicles, powertrains, articulation, deformables, terrain and several other systems.
- Saves do not automatically merge newly authored world content into an old complete-world snapshot.
- Migrations cannot generally rewrite opaque retained native-region state.

These restrictions frequently protect correctness. They should not simply be removed.

**Recommended action:** publish a cross-system support matrix, improve preflight diagnostics, then extend selected high-value combinations with explicit contracts.

Sources: [origin restrictions](/home/conner/Documents/GitHub/Project-Judas/src/WorldOrigin.cpp:14), [streaming restrictions](/home/conner/Documents/GitHub/Project-Judas/src/WorldStreaming.cpp:171), [runtime terrain bounds](/home/conner/Documents/GitHub/Project-Judas/src/RuntimeTerrain.cpp), [network scope](/home/conner/Documents/GitHub/Project-Judas/src/NetworkSession.cpp:16), [save compatibility boundaries](/home/conner/Documents/GitHub/Project-Judas/docs/M98_SAVE_EVOLUTION.md).

## 3. Maintainability assessment

### Confirmed structural pressure

Important behaviour is concentrated in large, densely written files:

- `PhysicsWorld.cpp`: approximately 3,800 lines.
- `ScriptSystem.cpp`: approximately 2,100 lines, with substantial long-line density.
- `RuntimeWorld.cpp`: approximately 1,700 lines.
- `EditorApplication.cpp`: approximately 2,100 lines.
- `CMakeLists.txt`: approximately 2,100 lines and 324 executable declarations.

Size alone is not a defect. The greater concern is **manual cross-system bookkeeping**.

Adding a component can require coordinated changes across serialization, prefabs, runtime construction, persistence, streaming, origin changes, editor controls, bindings and export. Some registries already exist, but coverage remains distributed.

### Recommended direction

1. Preserve explicit simulation phases and ownership.
2. Extract subsystem-specific binding and runtime modules behind typed interfaces.
3. Extend existing registries to make lifecycle/support obligations auditable.
4. Add tests that detect an omitted integration.
5. Separate build targets into manageable subsystem files.
6. Format and simplify touched code incrementally.

I would avoid a new ECS, universal serializer or broad rewrite merely to reduce file sizes. Those would introduce substantial migration risk without established benefit.

Sources: [runtime ownership](/home/conner/Documents/GitHub/Project-Judas/src/RuntimeWorld.cpp), [binding implementation](/home/conner/Documents/GitHub/Project-Judas/src/ScriptSystem.cpp), [existing component registry](/home/conner/Documents/GitHub/Project-Judas/src/editor/ComponentEditors.h:13).

### Risks requiring targeted reproduction

These are **suspected risks, not newly demonstrated defects**:

- Missed lifecycle updates when new components bypass one manual integration path.
- Debugger pauses interacting poorly with networking timeouts.
- Resource eviction becoming expensive with large residency sets.
- Blocking export-runtime probing hanging on an unresponsive executable.
- Unsupported subsystem combinations producing late failures instead of early authoring diagnostics.

Each needs a bounded reproduction before implementation.

## 4. Developer experience and documentation

The current tools are substantially better than the early engine: real debugging, declarations, API coverage checks, animation timelines, modern UI controls and resource tooling exist.

Remaining practical problems:

- New-project onboarding still requires substantial manual assembly of modern character, camera and input behaviour.
- The current M96 guide still says human review is pending despite acceptance.
- Some current documentation describes an older API surface.
- Architecture documentation mixes current guidance with extensive historical chronology.
- At least one final-check runner depends on TypeScript installed beneath an older milestone cache directory, making fresh-machine reproduction fragile.
- Networking exposes low-level connection errors where users need clear host/listener/endpoint guidance.

The M99 phone evidence also records an unexplained listener closure before successful USB-forwarded testing. That observation deserves a bounded reproduction; it is not yet an established networking root cause.

Sources: [current getting-started guide](/home/conner/Documents/GitHub/Project-Judas/docs/judasjs/getting-started.md), [stale M96 status](/home/conner/Documents/GitHub/Project-Judas/docs/M96_UI_TOOLKIT.md:3), [validation prerequisite](/home/conner/Documents/GitHub/Project-Judas/scripts/m99_final_checks.py:58), [phone follow-up](/home/conner/Documents/GitHub/Project-Judas/docs/evidence/m99/HUMAN_VALIDATION.md).

**Recommended documentation policy:** concise current guides, immutable historical evidence, and automated checks for API coverage, links, packaged documentation and current status. Documentation should accompany a capability change rather than rely on occasional large realignment passes.

## 5. Portability and distribution readiness

### Linux

The strongest acceptance platform. Existing evidence is extensive, but historical suite passes should be supplemented by a small, reproducible current release gate.

### Windows

Native support and previous functional evidence are real. SDK completeness and latest-feature coverage need attention. VM timings must remain diagnostic, not native-host performance claims.

### Android

Physical-device acceptance exists, including M99 gameplay. However:

- The packaging workflow is Linux-hosted.
- Default packaging identity still originates from the original Android demonstration unless overridden.
- Custom signing is already supported; it should become an explicit release preset.
- Latest multiplayer phone acceptance used **USB forwarding**, not direct Wi-Fi or Internet connectivity.

Do not infer validation beyond those boundaries.

### Repository weight

Tracked evidence alone contains approximately **44,700 files and 2.24 GiB of file contents**. That is not a measurement of compressed clone size, but it is a significant developer-distribution burden.

Prefer curated SDK/runtime releases and clear research/evidence access. **Do not delete or rewrite protected history to achieve smaller packages.**

## 6. Proposed Pre-Alpha work plan

Sizes describe relative scope, not time promises.

| Priority | Workstream | Scope | Completion evidence |
|---|---|---:|---|
| **P0** | Explicit build identity and two release channels | Medium | Exact source/build identity visible in editor, runtime, tools, APK and package manifest; archive builds work without Git history. |
| **P0** | Reliable regression and release gates | Medium | Focused test groups, explicit expected failures, reproducible prerequisites, failing negative controls and archived machine-readable results. |
| **P0** | Curved-mesh collision correctness | Investigation-dependent | Validated crossing oracle, preserved original failure, generic correction and affected collision/CCD regressions. |
| **P1** | Dense-contact predictability | Large | Matched workloads with median/p95/worst costs and physical invariants; documented practical limits. |
| **P1** | Packaging and clean-machine usability | Medium | Complete Linux/Windows SDKs, coherent Android presets, relocated exports and every shipped tool launching independently. |
| **P1** | Composed lifecycle contracts | Large, staged | Selected save/stream/origin/mod/resource combinations pass cold-process and failure-injection tests with actual restored endpoints. |
| **P1** | Incremental architecture cleanup | Medium, ongoing | Smaller subsystem boundaries and fewer duplicated integration lists without behaviour changes. |
| **P2** | Editor onboarding and actionable diagnostics | Medium | A fresh developer can create, debug, save and export a small game without engine-source knowledge. |
| **P2** | Current documentation and support matrix | Medium | Searchable current architecture/API/platform guidance with automatic drift checks. |
| **P2** | Platform and release qualification | Medium | Explicit Linux, Windows and Android results; missing hardware coverage remains visible. |

### Recommended execution order

1. Establish trustworthy build identity and focused verification.
2. Resolve the collision correctness finding.
3. Address dense-contact architecture using preserved workloads.
4. Repair packaging and high-risk composed lifecycle gaps.
5. Consolidate developer workflows and documentation.
6. Run a bounded integrated release candidate qualification.

Maintainability improvements should accompany these changes rather than delay them behind a separate large refactor.

## 7. Proposed main / Experimental release infrastructure

Keep the existing branch policy:

- **Experimental:** development builds after automated validation.
- **main:** exact operator-approved release candidates.
- No automatic promotion or updater.

Introduce one shared build identity containing:

- Explicit engine version.
- Channel.
- Immutable build/candidate identifier.
- Source commit and dirty-state flag.
- Platform, architecture and build configuration.
- Relevant compatibility identities.

**Engine version, save compatibility, content compatibility and network protocol version must remain separate concepts.**

Use isolated output directories for each channel/platform. Every distribution should include a manifest and checksums identifying its runtime, tools and content.

For update notifications, compare published channel build identities rather than milestone titles. Keep notifications optional, offline-safe and read-only, without requiring GitHub CLI authentication or downloading updates.

Promotion must preserve provenance. If branding requires rebuilding an artifact, record and validate that new artifact rather than claiming it is byte-identical to the tested one.

## 8. What should remain explicitly limited

These do not all need solving before an honestly scoped Alpha:

- **FLU-001:** deferred legacy-PBF settling issue; unchanged.
- Historical cold saves lacking continuation data: no invented reconstruction.
- General replication of every Judas physical system.
- Arbitrary native-topology save migration.
- Unrestricted suspension of every assembly.
- Multiple distant simultaneous physics charts.
- Full per-mip texture streaming.
- Every renderer feature being instanced or shadowed.
- Security certification for untrusted scripts or public Internet servers.
- Unperformed hardware/platform validation.

These should have clear support boundaries and useful diagnostics.

## Release-readiness conclusion

**Judas is ready for a focused Pre-Alpha stabilization programme, but I would not yet call it Alpha-release-ready.** The strongest blockers are unreliable release identity, an important collision diagnostic that does not fail its test, and severe preserved physics latency tails.

The existing engine should be consolidated, not discarded. My recommendation is to approve the plan above, beginning with build/channel infrastructure and collision verification.

**Nothing was changed. M100 was not started. Protected history and independent research remain untouched.**

<oai-mem-citation>
<citation_entries>
MEMORY.md:252-252|note=[Read-only audit boundaries and evidence scope]
</citation_entries>
<rollout_ids>
</rollout_ids>
</oai-mem-citation>