# Independent read-only frame correction review

Reviewed current `src/LiquidTypes.cpp`, `src/WorldRegions.cpp`,
`src/RuntimeWorld.cpp`, `src/WorldOrigin.cpp`, `src/Prefab.cpp`,
`src/OrientedGravityVolume.h`, `src/WorldGravity.cpp`, `src/WorldStreaming.cpp`,
and `tests/LiquidBakeFrameTests.cpp`. No build/test execution or source edits.

## Corrected construction paths

- Initial runtime object construction computes `rotation = normalize(authored)`
  once, then uses it for the field, oriented box volume and stored GravityRegion.
- `PrepareLiquidBake` now does the same for each field and owner-local inverse.
  It retains original registration order, first-applicable region selection,
  magnitude and ordinary gravity implementations. It does not introduce a new
  field model or relax the 1e-7 directional comparison.
- Switching bake box containment from BoxVolume to OrientedGravityVolume aligns
  existing authored box semantics with runtime. This intentionally corrects
  previously wrong selection for noncubic rotated zones; the independent inside
  and outside controls in LiquidBakeFrameTests meaningfully distinguish them.
- `RebuildRegionGravity` now shares one normalized rotation across acceleration,
  oriented volume and its stored GravityRegion. The earlier intermediate version
  normalized acceleration only, which could later reintroduce the original bug
  through origin reconstruction from its raw cached quaternion. Current code
  closes that path without changing origin reconstruction.
- StageRegionObject now normalizes slotless/static extra-record state.rotation,
  matching StateFromDefinition at root construction. This makes liquid-only
  published poses use the same owner frame as baking.

## Origin test setup correction

`RequestOrigin` rounds targets to integral 512 m anchors. The old new test's
(128,-64,32) request was a no-op but queried a translated point afterward.
The revised (512,-512,512) request actually shifts the chart and compares actual
selected equilibrium before/after. This corrects the new test setup; it does not
justify modifying the existing origin mechanism. Original failed run should remain.

## Boundaries / additional checks

- Current tests compare root liquid-only and rigid owners, original non-axis and
  half-turn quaternions, actual staged publication, nested parent frame, radial
  center/frame, box inside/outside and rebuild→real-origin shift. These cover the
  demonstrated normalization paths without asserting exact quaternion bytes.
- FlattenHierarchy normalizes composed parent/child rotations; presentation
  recomposes hierarchy separately. The nested test is useful evidence for the
  sampled frame, not a proof for every arbitrary nesting/precision arrangement.
- Existing unsupported non-unit liquid scale remains relevant: PrepareLiquidBake
  itself does not call ValidateLiquidComponents on its flattened owner. A unit
  child beneath a scaled parent could reach the bake function with effective
  non-unit scale while RuntimeWorld rejects that flattened scale. This is a
  pre-existing authoring preflight gap, not introduced by the frame correction.
  A focused direct-scale/scaled-parent negative control and early use of the
  existing liquid validator would be a bounded improvement; otherwise document it.
- Per-entity gravitySelection overrides are rigid/motor force selection, whereas
  current conserved liquids query the spatial world equilibrium. The correction
  preserves that ownership; it should not silently reinterpret body force overrides
  as separately authored fluid equilibrium fields.
- Root currently selects canonical FaithfulGravity at the exact canonical vector;
  bake/rebuild use UniformGravity. Their Equilibrium magnitude representation can
  differ by about 4.2e-7 (9.81 vs float-derived9.8100004196), within existing1e-6
  contract. This pre-existing representation difference is not an orientation fix
  and should not motivate unrelated gravity semantics changes.
- Region precedence sorting in RebuildRegionGravity is unchanged. These fixes
  should not be represented as new proof of all overlap/registration-order cases.

Assessment: no blocking semantic issue found in the current changes. The intended
changes align bake, initial publication, regional publication and origin frame
reconstruction. Final behavior still requires the root's actual combined test run.
