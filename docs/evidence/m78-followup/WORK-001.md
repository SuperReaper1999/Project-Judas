# WORK-001 correction design

Discovery uses normal SceneSession registration plus the explicit published kit scene index, typed asset closure and normal CollectNavigationGeometry fingerprints. It cannot infer unpublished private base scenes. Published kit inputs remain immutable; an affected published scene reports that an owned fork/replacement is needed.

After a terrain cook the editor lists saved-scene dependent navigation status. Refresh and explicit rebuild controls are separate from terrain publication. Unsaved scene edits must be saved first. The equivalent CLI is `judas_navigation_bake project.judasproj --check-dependents` / `--rebuild-dependents`.

Rebuild uses existing ResolvePrefabs/FlattenHierarchy/BakeNavigation. New immutable ordinary navigation assets and changed scene references are staged together, source fingerprints are rechecked, and existing PublishAuthoringOutputs performs guarded rollback-capable publication. Cancellation is checked before/after each bounded surface bake and before publication, not inside each Recast tile. Filesystem-wide crash atomicity is not claimed. Failure/cancellation does not overwrite the old navigation assets. Old unused immutable bakes are retained, not silently garbage-collected. Prefab-owned navigation surfaces require an owned authored override rather than editing a prefab behind the user's back.

## Final proof

Linux and native Windows: **38 checks passed**, including failure in a later surface after an earlier bake, cancellation after real baking, stale second-scene package rejection, both-scenes rebuild, and appearance-only freshness. Independent owned-project fork leaves the source project unchanged. The existing 15-check terrain pipeline also passes.

The normal exported base accepts the new shared terrain package through the shipping mod store; a moved `/tmp` application observer loads both registered replacements, queries the edited real collision height, finds Detour paths, renders, reloads and invalidates old handles: **20 checks passed**. Inputs use the exact frozen published kit/base relationship; an initial mismatched-base rejection was correct and is retained. No private unpublished base-scene discovery is claimed. Native Windows new editor/moved-package application execution remains unverified; native discovery/rebuild tests are not a substitute for those cases.
