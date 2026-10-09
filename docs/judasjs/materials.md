# Materials and scene appearance (M72 candidate)

[Index](../JUDASJS.md) · [Rendering contracts](../MATERIALS.md) · [M72 ownership and review](../M72.md) · [Render visibility](entities.md#render-visibility-m72)

The [material example](examples/materials.js) uses the ordinary public handles;
[M72 validation status](../M72.md#validation-status) records current execution.

Judas provides material, light and environment primitives. Project JS chooses
transitions, sun paths and the meaning of a visual change. All values below are
detached readbacks or validated writes; no GPU handles or shared-source edits are
exposed.

## Entity.material(target = 0) / Material

`entity.material()` returns the existing `Material` facade. Its public
`entityId` and `slot` fields are ordinary JS data; treat identity as opaque.
Every native operation reacquires the live entity and Render component. Stale
owners throw `ReferenceError`; missing components and invalid targets throw
`TypeError`.

| Target | Meaning |
| --- | --- |
| Integer `0..63` | Existing primitive-order material slot. The default is `0`; this is not a glTF material-table index. |
| `"*"` | Whole Render instance. |
| Other nonempty string | Exact stable imported part identity from `entity.modelParts`. The model must be ready and the identity must exist. Generated mesh filenames are not part identities. |

Primitives use slot 0. An imported part starts from its imported material unless
authoring assigns a registered `.judasmat`. Authored whole-instance, numeric-slot
and stable-part values resolve first. Runtime patches then resolve whole → numeric
slot → stable part, so a more specific runtime field wins over the whole patch.
Absent fields inherit. Two entities sharing the same model/material remain
independent. A whole-instance readback uses the first primitive's resolved base;
it cannot describe every different source material in a multipart model.
At most 64 runtime target bindings can be held by one Render component; clearing
a target retires that binding and its resource demand.

### Writes and reset

`assign(assetID)` selects a registered material for this runtime target; `""`
selects the imported/default base. It returns true. Existing factor/map patches
remain. `set(parameters)` atomically merges a validated patch and returns true;
invalid input throws before publishing any part of the batch.

| Parameter | Contract |
| --- | --- |
| `baseColor: {x,y,z,a}` | Linear RGB and unencoded opacity, all `0..1`. |
| `metallic`, `roughness` | `0..1`. |
| `emissive: {x,y,z}`, `emissiveIntensity` | Nonnegative, finite, at most 100000. Emission does not light neighbours. |
| `alphaMode` | `"opaque"`, `"mask"` or `"blend"`. |
| `alphaCutoff` | `0..1`; used by mask mode. |
| `normalStrength` | `0..8`. |
| `occlusionStrength` | `0..1`; affects ambient/environment lighting. |
| `doubleSided` | Boolean. |
| `uvScale`, `uvOffset` | Complete finite `{x,y}` vectors; existing per-instance UV controls. |
| `textures` | Partial object with `baseColor`, `metallicRoughness`, `normal`, `occlusion`, `emissive`. Each value is a registered texture asset ID or `""` to explicitly remove that map. Omitted roles inherit. |

Opacity multiplies factor alpha by base-colour texture alpha. Opaque mode writes
depth and ignores opacity for coverage. Mask mode discards below cutoff in colour
and shadow passes. Blend mode draws after opaque, back to front by object-origin
depth, with depth testing and without depth writes or shadow casting. Set blend
mode to make partial opacity visible. This is bounded object sorting; intersecting
transparent surfaces, refraction and coloured transparent shadows are unsupported.

`clearOverrides()` removes the runtime patch at this target, including runtime
material assignment, and returns true. Authored assignment/factors/maps reappear,
with any remaining runtime patches at other targets still applied. This M72 reset
supersedes M57's narrower parameter-only reset that kept runtime assignment.
Runtime calls never rewrite material files, model imports or scene/prefab sources.

### Readback and asynchronous textures

`state` returns a detached snapshot containing `asset`, `ready`, `status`, `error`,
`model`, `alphaMode`, `alphaCutoff`, `baseColor`, `metallic`, `roughness`, `emissive`,
`emissiveIntensity`, `normalStrength`, `occlusionStrength`, `doubleSided`, `uvScale`,
`uvOffset`, `textures` and `overridden`. Models are `legacy`, `pbr`, `unlit`.
`overridden` reports whether this target has a runtime patch, not whether its
authored source has instance settings. `textures` reports the five registered
selections; imported embedded images do not gain new asset IDs.
`status` is `ready`, `pending` or `failed`; `ready` is true
only for ready state. `error` supplies the bounded resource diagnostic on failure.
Readback is not a mutable reference to renderer state.

Registered replacement maps load through the normal ResourceManager. While a map
is pending or failed, that role inherits its source map as a coherent fallback;
it does not retain a prior runtime replacement. An explicit empty selection removes
it. Current durable selections resolve for each rendered sample, and resource
generations retire obsolete work. A newer
selection, reset, entity destruction or region unload cannot publish an old map
into the live instance. Factors and opacity do not reimport a model, compile a
shader or allocate a private copy of shared geometry.

## world.appearance / setAppearance / resetAppearance

The root RuntimeWorld owns scene appearance. `world.appearance` returns a copied
snapshot. `world.setAppearance(patch)` validates the entire patch before updating
the world and returns true. Sequential valid writes use ordinary last-write-wins
behaviour. Changes are visible to subsequent readbacks and the next rendered
sample; secondary cameras neither advance a transition nor change this state.

| Setting | Contract |
| --- | --- |
| `sunEnabled` | Boolean; disabling removes direct sun light and its supported shadows. Ambient/environment light may remain. |
| `sunDirection: {x,y,z}` | Finite nonzero world-space direction **toward the light source**, normalized on write. It is not light travel direction or a compulsory world-up/sun orbit. |
| `sunIntensity` | Finite `0..10000`, an engine radiance multiplier, not certified photometric units. |
| `sunColor`, `ambientColor` | Complete linear RGB `{x,y,z}`, each channel `0..10000`. Assigned IBL replaces fallback ambient. |
| `backgroundColor` | Linear RGB clear colour, each channel `0..10000`; an enabled loaded environment background takes precedence. |
| `linearRendering` | Boolean; opts into the existing HDR/linear world path. |
| `exposure` | Finite `0.000001..10000`. Independent display control; it does not change direct lighting. |
| `environmentAsset` | Registered `.judasenv` ID or `""` for none. |
| `environmentIntensity` | Finite `0..10000`. |
| `environmentRotation: {w,x,y,z}` | Finite nonzero quaternion, normalized on write. Independent of gravity. |
| `environmentBackground` | Boolean; background visibility is independent of environment illumination. |

Direction and quaternion squared length must be finite and greater than `1e-12`;
numerically negligible vectors are rejected before normalization.

`environmentStatus` and `environmentError` are read-only resource observations.
Status is `unloaded`, `queued`, `loading`, `cpu-ready`, `ready`, `failed` or
`cancelled`, matching ResourceManager. An empty `environmentAsset` reports `ready`
because no environment resource is required.
These two fields are ignored by `setAppearance`, so restoring a captured full
appearance snapshot remains supported. Invalid writable fields throw `TypeError`
without a partial update. A resource failure is reported after an accepted asset
selection; use the status/error observations rather than treating selection as
synchronous loading.

`world.resetAppearance()` restores the root scene's authored settings and returns
true. Additive region defaults do not take ownership of the root sun/environment.
Whole-scene replacement/reload and Play/Stop reconstruct authored defaults. Modern
[M61 save slots](saves.md) preserve supported live appearance, material selections
and visibility; [M59 retention](streaming.md) preserves per-entity instance state.
GPU handles/loading caches are rebuilt rather than serialized.

## Main and auxiliary views

The main view consumes real sun lighting and the existing directional shadows;
exposure/output conversion follows the world pass. M33 auxiliary views share
ordinary geometry/material/transparency submissions and the main-focused shadow
maps. They keep their authored resolution, cadence and render masks; no independent
shadow focus or main-view quality guarantee is added. Modern auxiliary targets
store scene-linear radiance without exposure/tone mapping; the final main view
converts it once. Feedback protection and target ownership remain in force.
Visibility governs supported submissions in every view. UI/debug display colours
remain independent of world exposure. Point/spot counts and controls retain their
existing bounded authoring surface; M72 adds no new light classes or counts.

Physical contact materials remain independent [resources](physics.md#gravity-physical-materials-and-runtime-joint-configuration-m65).
