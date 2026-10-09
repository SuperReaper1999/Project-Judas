# Materials and scene appearance (M57)

Judas provides material/rendering primitives. Project assets own appearance; JavaScript decides why it changes. See [material authoring and colour pipeline](../MATERIALS.md), [entities](entities.md), [presentation timing](effects-camera.md) and [lifetime](lifecycle.md).

## `Entity.material(slot = 0)` / `Material`

Returns a wrapper, with public opaque `entityId` and integer `slot` (0–63). Operations reacquire a live render component. Stale entities throw `ReferenceError`; absent render components and invalid arguments throw `TypeError`. Slot index is primitive order, not glTF's material-table index. Primitives have slot 0. Imported slots use their imported source unless assigned a registered material asset.

- `state`: copied `{asset, ready, model, alphaMode, baseColor, metallic, roughness, emissive, emissiveIntensity, uvScale, uvOffset, overridden}`. Empty asset means imported/default source. `ready` is false while an assigned asset is pending/failed; inspect resource/editor diagnostics. Model is `legacy | pbr | unlit`; alpha mode is `opaque | mask | blend`.
- `assign(asset)`: registered `.judasmat` ID, or empty string to restore imported/default source; returns true. Existing overrides remain.
- `set(parameters)`: merges instance overrides, returns true. Supports `baseColor: {x,y,z,a}` (linear RGB and unencoded alpha, each 0–1), `metallic` / `roughness` (0–1), `emissive: {x,y,z}` (linear, nonnegative ≤100000), `emissiveIntensity` (0–100000). No per-frame shader compilation or resource recreation.
- `clearOverrides()`: removes all parameter overrides on this slot, returns boolean. Keeps assigned asset. Shared definitions remain immutable.

Runtime edits are transient to this world, including across component disable; scene reload/Stop reconstruct authored values. Shared-source editing is an intentional editor operation, not a script write to disk. No implicit save of runtime material state. An explicit [M61 slot](saves.md) preserves runtime material assignments/overrides and scene appearance; ordinary reload/Stop still restores authored data.

```js
const surface = entity.material(0);
surface.set({roughness: 0.8, baseColor: {x: 0.2, y: 0.35, z: 0.1, a: 1}});
surface.clearOverrides();
```

## `world.appearance` / `world.setAppearance(settings)`

Read a copied scene configuration or merge a patch and return `true`:
`backgroundColor`, `linearRendering`, `exposure`, `environmentAsset`, `environmentIntensity`, `environmentRotation: {w,x,y,z}`, `environmentBackground`.
Exposure is a linear multiplier (strictly positive ≤10000); environment intensity is 0–10000. Environment is an empty ID (no IBL) or a registered baked `.judasenv` asset. Rotation is a finite nonzero quaternion, normalized by Judas, independent of gravity/camera orientation. Boolean fields require booleans. Invalid configuration throws `TypeError`.

`linearRendering` opts into floating-point lighting and display resolve; it does not change physics. Environment background visibility is independent of IBL. Settings are transient; reload restores authored settings. UI renders after display resolve and ignores exposure. Use `presentationUpdate` for camera pose, not material state persistence. Example: `world.setAppearance({exposure: 1.5})`.

M65 instance overrides also accept `uvScale:{x,y}` and `uvOffset:{x,y}`. They reuse
the existing material shader parameters: defaults scale1/offset0 remain unchanged.
`world.setAppearance({backgroundColor:{x,y,z}})` / appearance.backgroundColor set
linear scene clear colour. An enabled environment background takes precedence; HDR
exposure/tone mapping applies, so these are not exact display RGB values.
Physical contact materials are independent [resources](physics.md#gravity-physical-materials-and-runtime-joint-configuration-m65).
