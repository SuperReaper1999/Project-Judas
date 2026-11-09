# M36 — Prefab assets, instances and overrides

A `.judasprefab` is a normal `JudasScene 3` document with exactly one root,
parent-local child transforms and ordinary components. Scene settings in the
prefab are not imported into its instances. Normal `.judasmeta` identity and
AssetDatabase import/track/move rules apply. Nested prefab links are rejected;
creating a prefab from a selected hierarchy explicitly flattens its provenance.

Each placed instance has a stable root, source-ID → scene-ID mapping, AssetId,
and explicit serialized-property overrides on its members. Cached component
values in the scene file are conveniences for editing, never source authority:
editor open and RuntimeWorld construction resolve source data again and then
apply overrides. Source additions allocate new mapped IDs; existing child IDs
are unaffected by object order or additions. Deleting/reintroducing a source
child preserves its reserved scene ID, but removed-child overrides are not kept.

Overrides use the existing object serializer's keys and values, including
component headers. No second component serializer exists. Removing a component
records removal of its block; adding a component records its ordinary fields.
Changing a placed instance never silently saves its source. Root transform is
instance placement, independent of the source's root transform.

## Editor workflow

1. Build a hierarchy using the Inspector's **Parent (local transform)** field.
2. Select its root and choose **Create prefab from hierarchy**. The new asset
   appears under `assets/prefabs/`; rescan the Asset Browser if needed.
3. Select the prefab asset and choose **Place prefab instance**.
4. Edit ordinary Inspector fields. The prefab section lists overridden property
   keys and offers **Revert property**. Root placement uses normal transform UI.
5. To deliberately change the source, select an instance root and choose
   **Apply this hierarchy to prefab source**. Overrides on that source instance
   are cleared; all non-overridden properties in other instances refresh.
6. **Reload prefab sources** reloads external source edits. These authoring
   changes use scene undo/redo; source-file writes themselves are not undone.
7. In Play, an instance root offers **Spawn independent prefab** and
   **Destroy last spawned hierarchy**. Stop destroys runtime state and voices;
   authored instances and assets remain unchanged.

The hierarchy panel remains a flat list with explicit parent identities rather
than a new tree UI. Linked instances should be deleted/duplicated from their
root. Per-instance structural child deletion is not an override feature.

## Runtime and hierarchy semantics

`RuntimeWorld::SpawnPrefab(AssetId, SceneTransform, error)` resolves the source,
preflights the complete hierarchy and creates ordinary entities through the
same component-construction loop as scene loading. It returns the root EntityId.
`DestroyHierarchy(root, error)` destroys its members without changing source or
other instances. Invalid/missing assets report errors; source resolution is
transactional.

Runtime creation supports ordinary prop body/render/celestial components plus
body-free renderables, lights, audio emitters and render cameras. Scene-global
and gameplay ownership components (player start/listener, gravity regions,
terrain/fluid/atmosphere, vehicle/combustible, door/switch) remain scene-authored;
spawning a hierarchy containing them reports a capability error before creation.
They are supported in placed scene instances, subject to existing singleton rules.
Runtime camera/parent dependencies must admit a creation order; cyclic creation
references fail safely. Streaming prefab files is outside M36: small authored
prefab documents load synchronously at authoring/spawn boundaries.

Parent transforms compose ordinary TRS: translation, normalized rotation and
component-wise scale. No affine shear is represented. Body-free children follow
parent presentation; child physics bodies are independent ordinary bodies at
resolved world poses, with no implicit weld/joint. Explicit rigid attachment is
still a separate engine concept. Rendering remains downstream of physics.

## Persistence

Canonical authored fingerprint schema **5** adds hierarchy/provenance/override
metadata. RuntimeWorld hashes the effective source-resolved authored scene, so
relevant prefab source changes invalidate old deltas. Save format remains 2 and
scene format remains 3 (new optional fields). Earlier fingerprint schemas are
unverifiable under the new contract and are rejected, never migrated or deleted.
Runtime-created hierarchy definitions and state use ordinary created-entity
records, not live prefab objects. Parent references/cycles are preflighted before
applying saved deltas.

## Demo and checks

`judas assets/scenes/prefab_demo.judas`, or open that scene in `judas_editor`.
Two blue-base/green-lamp assemblies share one prefab; the right base explicitly
overrides its colour to yellow. A lamp cap demonstrates a second child level.

Focused tests: `judas_prefab_tests` and `judas_prefab_application_tests`.
The latter runs real async startup, GL rendering, fixed steps, spawning,
destruction and shutdown. Automated output is not human visual acceptance.
Results and commands: `docs/evidence/m36/`.

Nested prefabs, variants, structural instance overrides, automatic merge of
incompatible component changes and source-file undo are future work. An override
that becomes invalid after an incompatible source edit produces a useful error;
it is not silently retargeted to another child.
