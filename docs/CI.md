# CI and qualification

V2 development remains on Experimental; main is not automatically updated. Alpha and Experimental are build labels, not parallel engine implementations. Advanced Fluids V2 requires project opt-in in every label; the old Alpha denial policy is retired.

## Routine feedback

`prealpha.yml` builds one Alpha-labelled V2 engine/SDK and the `judas_ci_tests` target. CTest's `routine` label selects 171 native cases plus six Python contract registrations. Fifteen expensive game-level/endurance cases run during full qualification instead. `workflow_dispatch` with `full=true` can run the complete suite for a justified development change; it still does not establish release acceptance.

The validation manifest owns the two scheduling decisions: `releaseOnly` excludes a case only from routine feedback; `parallel` allows an audited independent case to overlap. Both require actual JSON booleans. All other native cases remain serial. Every case gets its own scratch and user-data directory. Test arguments, binary identities, numerical tolerances, collision coverage and scene complexity are unchanged. Different invocations of one target (cold restoration, portable mode, loss recovery) remain distinct when they catch different failures.

Compiler caches validate actual compiler inputs; dependency cache keys include toolchain/dependency identities. Source/channel identity is regenerated and SDKs are freshly staged/verified. Cache hits are not qualification. Record restore/save time and compiler statistics before claiming a speedup.

## Complete candidate qualification

`release-candidate.yml` is manual and never publishes. It freezes exact source, builds and qualifies Linux and native Windows SDKs, runs all 186 native manifest cases on Linux, cold-process readers, primitive controls and the complete Python control suite, SDK authoring/moved exports, API and ABI checks. Native Windows remains MSVC; Ninja schedules requested targets as a shared dependency graph. Its hosted tool/contracts qualification does not certify Windows hardware GL, Android or full browser gameplay.

Main promotion requires successful exact-candidate Linux AND Windows jobs and explicit operator acceptance. Routine checks, a different revision, skipped/failed jobs or a completed build alone cannot qualify promotion. Publication continues to consume already-qualified immutable artifacts through the existing operator-approved publication environment. No main promotion or publication follows an Experimental push automatically.

## Coverage decisions

Removed expectations protect only the retired Alpha fluid restriction. Four separate Python water-unavailability checks are consolidated into one all-channel execution check. Duplicate channel/generator registration variants are reduced while retaining provenance, invalid-channel, manual-failure, ordinary and multi-config coverage. Actual water runtime/editor/export tests now exercise opt-in in Alpha rather than testing refusal or returning early. Release packaging Python tests are discovered once with the other controls, rather than invoked again individually.

The 15 release-only cases remain available: sustained terrain residency, Willow Town application/life/absence, Beacon Grove application/loss, tilemap application, Utility AI application, complete 2D authoring, browser workflow, UI menu workflow, Basic Water application/cold cycle, developer workflow and camera cold suite. Their focused subsystem tests remain routine. Full qualification is authoritative for these game-level contracts.

Reference timings and raw cold/warm measurements accompany the checkpoint handover. The pre-change reference is `9176a36a`: Alpha 24m06s + Experimental 31m53s; native Windows candidate 37m58s. Those are measured job wall times, not future targets.
