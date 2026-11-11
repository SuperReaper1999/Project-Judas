# Lighting and gameplay exposure

Lucifer uses ordinary scene light components. `entity.light` reads parameters;
`entity.setLight(patch)` changes enabled state, intensity, colour, range, cones,
shadow request and priority atomically. Parent an authored light to a prop or
vehicle to follow it. Games own damage, power, batteries and vehicle controls.
Disabling illumination takes effect in the next presentation; selected dirty
shadows are redrawn in that same presentation. Quality allocation can limit the
number of shadowed sources, but never delays an invalidated selected map.

Use the Light component Inspector to choose Point or Spot, enable shadows, set
range/colour/intensity and adjust cones. Spots face local -Z. The sun remains
scene appearance state, controlled through `world.setAppearance`. Lighting can
now be previewed with shadows in Edit mode. Intensity retains Judas's relative
units; it is not calibrated lumens/lux. Emission in a material does not implicitly
create a light source.

## `world.lightExposure(position, options = {})`

Returns a **current CPU simulation sample** for stealth, sensors or gameplay.
It does not read GPU pixels and is independent of the current camera, visual
shadow budget, graphics tier and rendering visibility. Positions use the current
local world chart; `world.toLocal` converts absolute coordinates.

```js
const sample = world.lightExposure(this.entity.transform.position, {
  ignored: [this.entity], maxLights: 32, maxOcclusionQueries: 16,
  sunOcclusionDistance: 128
});
if (sample.complete && sample.relativeIlluminance > 0.2) {
  // Game-defined threshold and behaviour; Judas does not implement stealth.
}
```

Options:

| Field | Default / bounds | Meaning |
| --- | --- | --- |
| `maxSources` | 1024 / integer 0–4096 | Maximum registered local sources examined, including disabled/unpublished slots. |
| `maxLights` | 32 / integer 0–64 | Maximum relevant contributors retained, in stable potential-illumination order. |
| `maxOcclusionQueries` | 16 / integer 0–64 | Maximum physical visibility rays. |
| `sunOcclusionDistance` | 128 / (0,4096] metres | Explicit finite directional-light occlusion range. |
| `normal` | omitted | Optional finite nonzero receiving normal; otherwise an orientation-independent exposure estimate. |
| `occlusionLayers` | all / names or `null` | Physical collision layers that block the sample. |
| `ignored` | empty / up to 64 published Entities | Receiver or other game-owned collision bodies to exclude. Emitter-owned bodies are automatically excluded. |

Results:

- `unit: "relative"`, `color`, `relativeIlluminance`: ambient plus **known visible**
  direct contributions, using renderer distance/cone attenuation and optional
  incidence. Scalar luminance uses linear Rec.709 weights.
- `upperBound`: possible relative illumination including unchecked contributors;
  **null** if the source scan could not bound unexamined lights.
- `complete`: every relevant contribution was evaluated within the declared
  source, contributor and physical-query budgets. Never treat an incomplete
  dark lower estimate as proof of darkness.
- `sourcesExamined`, `occlusionQueries`, `unresolvedSources`: actual work and
  outstanding coverage.
- `contributors`: `{entityId, potential, visible, occlusionKnown, occluded}`.
  Source ID `"0"` denotes the sun. Returned arrays/values are detached snapshots.
- `sampledAtSeconds`, `ageSeconds`, `stale`: simulation timestamp, zero age and
  false at return. If the game retains a result, it owns its increasing age.

This is a gameplay approximation, not renderer readback or global illumination.
Only published physical geometry blocks rays; transparent glass or decorative
render-only geometry needs a deliberate collision-layer policy. Sensors are
excluded. Directional occlusion beyond the specified range is outside the
query's contract. There is no material transmission, reflection, exposure tone
mapping or image-based-lighting contribution to this estimate. A query-count
budget does not guarantee fixed wall time against arbitrary collision meshes.
Use bounded game-specific sampling cadence rather than querying every NPC on
every rendered frame. Invalid inputs throw; no GPU work is issued.
