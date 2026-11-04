# Editor application decomposition

Starting point: Experimental 45fe558388710d46d6f346b6e19ca2be9e76d0e6.
Original source/header snapshots are under ../baseline/.

## Production responsibilities

- EditorApplication is startup/frame/shutdown coordination and thin cross-workflow delegates.
- EditorProjectWorkflow owns file-browser, new-project and unsaved-document/modal state. Project operations, request processing and standalone launch each have their own cohesive implementation source. Cross-workflow dependencies are explicit EditorProjectServices borrows; no application pointer is retained or included by those implementations.
- EditorPlaySession owns the sole RuntimeWorld and InteractivePlay, their startup/teardown and actual frame/save/scene transition ordering. Contexts borrow observations and do not own sessions.
- EditorSceneViewport owns editor camera, scene target, picking, gesture/gizmo state and authored planetary/region caches. It depends on EditorContext plus specific terrain/tilemap tool borrows.
- EditorAssetPreview owns UI/model targets, preview UI/localization, immutable preview observations and authored scene asset leases.
- Workspace-preference I/O and measured profiler observations are separate small coordinator-adapter implementations.

All eleven coordinator/production/fixture sources passed serial -fsyntax-only before root's docking changes; no heavy library build was performed by this subtask. New owners have explicit destructors where unique_ptr members require complete native types. Root is responsible for final real builds and workflows.

## Preserved implementation inspection

application-functions.json retains all original method sources.
pre-docking/ retains the first owned implementation before root integration.
BODY_PARITY.json compares 21 actual implementation bodies after declared owner/context renaming and C++ adjacent-literal normalization: 18 match exactly. These are source checks, not behavioral proof.

Three rows are intentionally not hidden:

1. UpdateGizmo: review caught a local/member rename collision (old m_drag versus local drag). Root was instructed to rename the local candidate and assign the member correctly. The initial bad extracted snapshot is preserved; it is not claimed accepted.
2. CancelCameraNavigation: the old nullable m_host guard becomes unnecessary because EditorContext requires a live EngineHost reference.
3. RefreshProjectLists: optional host-pointer ternaries become direct borrowed context services under the same nonnull invariant.

Start/Stop and loop extraction preserve the original ordering by inspection. Specific ownership adaptation: Play's overlay callback captures a stable EditorPanelState pointer, not the temporary EditorContext. Terrain guards remain in the coordinating StartPlay adapter. The world-state path expression is the same project/default rule. These have not been claimed normalized-token-identical.

## Qualification fixture ownership

EditorAutomation owns opt-in flags, clock, interaction samples, expected scene bytes and per-feature counters. Its ordinary hooks remain Initialize, BeforeRequests, AfterRequests, AfterRender. AfterRender delegates in original order to four cohesive sources: terrain/async readiness, asset workflows, workspace/navigation, and simulation/Play/Stop. The separate stabilization fixture retains its own state and normal editor request usage. It never supplies an alternate physics, publication or resource path.

## Paused debugger extension

ScriptTools drawStoppedShell/presentStoppedShell are optional presentation-only callbacks. The existing standalone/fullscreen fallback remains. The host callback is invoked inside the stopped UI loop, never through the ordinary application frame. GL/FBO preservation, F5/F10/F11 actions, source snapshots, state inspection and clock reset remain in ScriptTools. The existing functional test now exercises one original fallback stop and one hosted shell stop; its original native-state invariants remain unchanged.
