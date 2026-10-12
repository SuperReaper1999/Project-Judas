# PHY-CLOSURE-001 — independent query-proxy publication isolation

## Review boundary

This new receipt follows the initial `REVIEW.md`; it preserves that report, the first fixture-authoring failure, and the before-fix executions. The reviewer inspected source and prepared copied observers only. The parent owns compilation and execution. No runtime source, original acceptance fixture, threshold, cadence, fluid implementation or earlier evidence was edited by this reviewer.

The original minimal probe actually contains **6 checks**, not the 7 described in the initial review prose. Its preserved before-fix result is 6 checks / 1 failure. V2 adds two checks for a query-only proxy used as a physical joint endpoint; its preserved result is **8 checks / 1 failure**. The added mixed-world fixture uses normal `Scene::InsertObject` identity allocation; its initial rejected direct-vector insertion remains preserved.

## Reproduced cause

- Before-fix source: `PhysicsWorld.cpp` SHA-256 `0a59fd11626f9099806f9eae67879125e257f688814249a701324abae6bb81ff`.
- The unrelated query-only proxy reset preserved the actual completed contact rows but unconditionally cleared the global restored-publication token. The negative affected-body teleport correctly invalidated both readiness and its old readable rows.
- Normal `Simulation` ordering refreshes M70 motor query proxies before `PreparePhysicalBalances`. A distant independent motor therefore made the restored M77 task treat a genuine completed publication as unavailable.
- The V2 copied M77 writer passed **25 checks / 0 failures**. Fresh-process reading failed **13 checks / 1 failure**. The first restored task clock stayed at `0.35` instead of advancing; the final task reported failure. This is an interaction between retained corrections, not an inferred API gap.

Exact before-fix commands, archive identity, raw output and source identity are in `../linux-mechanism-before-v2/RESULTS.json`. The initial authoring rejection is in `../linux-mechanism-before/`.

## Narrow generic correction reviewed

`PhysicsWorld::Impl::InvalidateRestoredContinuation` at `src/PhysicsWorld.cpp:265` now determines whether the mutated body belongs to the physical publication context. An ordinary body remains physical. A query-only body also becomes physical if any existing joint uses it as an endpoint. Matching joint continuation guesses are still cleared before publication is conditionally demoted.

This preserves the required ownership distinction:

1. Independent query geometry cannot contribute physical completed contact rows and cannot demote an unrelated articulation's restored publication.
2. A mechanically connected query proxy can affect the joint equations and retains conservative invalidation.
3. Ordinary body mutation retains the previous conservative publication invalidation.
4. Per-joint seed invalidation remains unchanged; the correction does not retain guesses for a mutated endpoint.

The helper's callers establish a valid live body before indexing the slot. `DestroyBody` invalidates before removing attached joints. Disabled joints are conservatively counted as attachments. Query-only bodies cannot be converted to dynamic motion through `SetMotionType`; response/contact eligibility excludes them. These source invariants justify the narrow exception without scene, entity-name or controller-type checks.

The new 14 checks in `PhysicsContinuationTests.cpp` cover the actual positive physical contact, independent query-proxy pose/filter/tag/disable/destruction, ordinary physical mutation, and the query-only joint-endpoint negative case. This read-only receipt does not claim those checks passed; the parent's final execution receipts are authoritative.

## Exact reviewed source identity

| File | SHA-256 |
|---|---|
| `src/PhysicsWorld.cpp` | `6a60747e4bb970821e2ebb68adbe78ea89f6fdc3db9137b5160290ca1da2f193` |
| `src/WorldPersistence.cpp` | `14a5ce3a78eebb2db801f91ddd91dc35808f23ff637a797c7cbb599d2e756ad4` |
| `tests/PhysicsContinuationTests.cpp` | `8dbbc7da8a66fb12484c53fac054ce84fc5c1c5aab4d1b0efb0183fa5bfc1572` |

## Remaining risks and untested scope

- This is a first-restored-publication ownership repair, not a new live contact-observation API or a guarantee of arbitrary nonlinear controller continuation.
- Original format-1/2/3 saves such as `a106...` still omit the numerical/publication context and remain an explicit failed legacy continuation case. No missing state is reconstructed by this helper.
- Real body changes elsewhere remain globally conservative; avoiding unnecessary invalidation for unrelated real bodies would need a different dependency/publication design and is outside this repair.
- Joint attachment scanning remains the existing bounded-by-world-joint-count operation. The change adds no extra scan, but does not establish a large-world scaling guarantee.
- A new internal body type contributing physical responses despite `queryOnly` would violate the premise and would need revised eligibility and regression coverage.
- Native Windows execution, the final mixed current-body witness, the combined final regression matrix, and performance are parent-owned and were pending at this source-review boundary. Source review does not substitute for those results.
- The unchanged original standing helper can report stale cached observations after failure. The supplied endpoint observer separately checks current rigid-body state, task success and actual current contacts.

`MixedCurrentPhysicalEndpointObserver.cpp` is copied from the frozen final-current-standing observer (`715dd4ae...`) and adds only the same distant independent M70 motor via `Scene::InsertObject`. Its current-body equations, no-op diagnostic hooks, inputs, timestep, step counts and acceptance thresholds are unchanged. The observer SHA-256 is `df72a228637da53eb48d0628272af1734590331d2875e92b787a1d0ef6b5b4d4`; `MIXED_CURRENT_OBSERVER_IDENTITY.json` records the exact base and run guidance.

No second substantive ownership/invalidation defect was established in the read-only angular, CCD/contact, Boolean-query and persistence review. This conclusion is scoped to the inspected source and supplied contracts, and does not assert universal absence of defects.
