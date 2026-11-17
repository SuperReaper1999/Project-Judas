# Tags, categories and filtering

Tags describe entities; collision layers describe bodies; render layers describe
presentation. They are three independent registries, not one shared layer.

## Authoring

Open **Project settings → Tags and layers** to create/rename/delete categories.
Save the project. IDs are persisted bit positions, never row positions: names
can change without retargeting references. Each registry has 64 lifetime IDs;
deleted IDs are retired and are not reused. Collision/render ID 0 is reserved
Default. Deleting an assigned category requires repairing the assignment before
Play/export; construction fails clearly instead of silently reinterpreting it.

The entity Inspector exposes multiple tags and one render layer. Body, Door and
Player start inspectors expose collision layer and mask. Camera inspector exposes
its render mask; Scene settings exposes the main/editor-preview mask. Masks offer
All (including future layers), None and individual named checkboxes.

Physical contact requires **both** masks to include the other body's layer.
There is no second project collision matrix. Defaults are layer 0, all permitted
layers, empty tags and all camera layers; old projects/scenes need no migration.

## Runtime API

Resolve names once via `world.Categories().tags.Find("target")`; retain the stable
ID. `TagsOf`, `HasTag`, `AddTag`, `RemoveTag` and `QueryEntities(required, excluded,
candidateSet)` include ordinary static/body-free/dynamic entities. Queries return
stable IDs in ascending order, include unloaded entities and exclude destroyed
ones. Runtime tag mutations are transient; rebuild/RestoreAuthoredState restores
the authored set. They are not new M29 saved-state fields. Created definitions
carry their authored categories through normal creation/save reconstruction.

`PhysicsWorld::SetCollisionFilter` updates a live generation-safe handle and clears
its cached contacts. Rigid broadphase pairs, induced/event candidates and cached
support expansion reject disallowed pairs before geometry/solver work.
`SetPlayerCollisionFilter` controls ordinary player capsule queries. Particle-fluid
collision remains its existing approximation, not a layer-controlled rigid body.

`PhysicsQueryFilter` independently chooses included/excluded layers, required/
excluded tags and ignored body handles. `QueryBodiesInAabb` returns conservative
broadphase candidates (not exact overlaps). Explicit-filter `SweepPlayerShape`
uses the existing capsule geometry and can hit mask-zero/query-only bodies.
There is no new raycast subsystem. A null sweep filter retains physical player
policy; an explicit filter bypasses physical masks. Handle generation prevents a
stale ignore/filter reference from aliasing a new body in the same slot.

Renderer checks the camera mask, then the same visibility/particle frustum. WorldPresentation
supplies entity layers for meshes, terrain, doors/switches, compounds, atmosphere,
fire visuals and particle emitters. Main and camera-target passes set independent masks;
ending a target restores the previous mask. Debug/UI overlays are independent.
Engine-owned player/torch and aggregate fluid visuals use Default. Shared shadow
maps still include all layers; lights/occlusion are not layer-filtered. Filtering
never controls physics, gameplay, audio or particle ageing.

## Serialization, prefabs and saves

Project format 1 has an optional quoted `classification` record with its own
version 1. Scene format 3 has optional decimal unsigned 64-bit masks and stable
layer IDs. Defaults are omitted. Older readers reject unfamiliar keys rather
than silently lose assignments. No format/fingerprint version bump is needed:
canonical schema 5 adds `Judas.Classification.1` only for nondefault assignments.
Default pre-classification baselines retain identical fingerprint bytes; changed assignments
invalidate saves. Registry names are not scene content; assignments use stable IDs.

Prefab object blocks and the existing generic property override path carry tags,
layers and masks. Source changes affect non-overridden properties; overrides and
revert use their normal serialized keys. Effective resolved source content feeds
the authored baseline fingerprint. Project context is passed through editor Play,
standalone startup and export, including prefab source validation.

## Validation and scope

The native classification/query fixtures validate filters, round trips and runtime
observations. Use the source validation manifest to select registered gates; no
retired milestone launcher is required. Tag queries are explicit entity scans,
not a constant-time ECS index or an implicit faction system.
