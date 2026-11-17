# Import-time animation retargeting

Import → map → retarget → preview → bake → use. Retargeting transfers an existing
performance through the shared importer; project scripts decide when clips play.
It is an authoring operation, not a runtime retargeting service.

## Design and ownership

One version-1 `JudasRetarget` JSON authoring profile, saved as
`Imports/*.judasretarget`, is referenced by `motions[].retargetProfile` in the
existing `JudasImport` recipe. It is not a new runtime asset or animation player.
The target recipe appends the baked clips to its immutable `.judasmodel` beside
native clips. Geometry, materials, skin weights, inverse binds and rest skeleton
are untouched. Runtime needs only that ordinary cooked model.

Profiles record source identity, target asset identity, key-sorted skeleton
signatures, exact escaped hierarchy-key pairs, local reference corrections,
source-to-target model alignment, positive translation scale and explicitly
translated target joints. Source arrays/ordinals are not authored references.
Unmapped source ancestors still affect observed model-space motion. Unmapped
target joints keep their reference locals and carry descendants normally.

Let `S(t)` and `Sr` be source joint model-space rotations, `Tr` its target
reference rotation and `C` the profile's source-to-target model alignment:

```
D = S(t) inverse(Sr)
T(t) = C D inverse(C) Tr
targetLocal = inverse(currentTargetParentRotation * targetFixedRotation) T(t)
```

`targetFixedRotation` is the retained fixed-affine node rotation, or identity
when the node has none. Full ancestor paths are evaluated. Static reference corrections postmultiply
rest **local** rotations; they never rewrite bind/rest/weights. The declared
source reference therefore transfers to the declared target reference. Target
local translations/scales are preserved by default; opted-in translation deltas
include local translation deltas along intervening unmapped source helpers up to
the nearest mapped ancestor, expressed in reference parent frames, alignment and
the authored scale; mapped ancestor movement is not counted twice. Profile model axes
are asset coordinates, never global gravity or camera up.

Root extraction/in-place selection remains the existing recipe `rootMotion`
policy, applied once after transfer. The selected motion root/translation axes
and optional twist axis are explicit. Pelvis/body bob need not be discarded.
Runtime `animation.rootMotion` remains a model-local interval query, not movement.

The normal immutable CPU job/staging/publication path hashes source, profile,
target and clip settings. Changed skeleton identities/reference data require a
mapping review; changed clip content/settings require rebaking. Failed/cancelled
or stale jobs preserve the last accepted output. Preview is authoring only;
Renderer owns GPU resources and the played world is not used as a preview engine.

## Supported envelope

Compatible articulated hierarchies with different names, proportions, local axes,
reference poses and helper counts; explicit manual mapping, positive uniform
normalization, ordinary quaternion/local tracks. No runtime retarget service,
automatic anatomy detection, arbitrary topology conversion, motion generation,
contact warping, face solution or skeletal keyframe editor. Participating shear,
reflection/singular transforms and animated scale are diagnosed explicitly.
Nonconstant participating STEP channels are rejected: LINEAR emitted tracks cannot
honestly preserve a discontinuity. Continuous LINEAR/CUBICSPLINE sources are
sampled on the declared grid plus source keys/endpoints; curved motion between
samples is an approximation. Already-extracted source root tracks are rejected
explicitly; use an unextracted source for this first version.
Mapped joints use the importer hierarchy bound (4096), not IK/physical-animation's IK solve bound.
Profiles are bounded to 4 MiB; sampling to 240 Hz, one hour, one million sample
times, 64 million sampled source-plus-target node evaluations and four million
keys. All bounds apply together: sample count multiplied by the combined full
source/target hierarchy size must fit the node-evaluation budget. Source keys and
trim endpoints added to the grid count toward these bounds. They are checked
before allocating the emitted key arrays.

## Profile and recipe reference

The profile is JSON, with a strict `format: "JudasRetarget"`, `version: 1`.
There is one format and no runtime profile registration. All keys are escaped full
`SkeletonJointKey` paths (`%25` for percent and `%2F` for slash in a node name).

| Field | Meaning |
|---|---|
| `sourceIdentity` | Canonical project-relative motion source, matching the recipe entry |
| `targetIdentity` | The target recipe's existing 32-character asset ID |
| `sourceSignature`, `targetSignature` | SHA-256 of key-sorted rest/affine/bind hierarchy content; not transient indices |
| `mapping` | Unique one-to-one `{source,target}` full-key pairs; nearest mapped ancestors must correspond |
| `modelAlignment` | Normalized quaternion `[x,y,z,w]`, source model frame to target model frame |
| `translationScale` | Positive scalar, default 1 after model-import unit normalization |
| `translationJoints` | Target keys whose source/helper local translation deltas are transferred |
| `sourceReference`, `targetReference` | `{joint,rotation:[x,y,z,w]}` local rest-postmultiplying calibration corrections |

Changed names/ancestor paths, rest proportions, affine nodes or binds invalidate
signatures and need mapping/calibration review. Mere node/palette reordering does
not. Clip keys, source content, profile calibration or recipe settings need a rebake.
Material-only changes do not require a different skeleton identity. The existing
logical target asset ID survives compatible rebakes.

Use an explicit profile for your source and target skeletons; saved profiles
are authored content, not automatic anatomy detection.

```json
{
  "source": "Sources/source.gltf",
  "take": "Travel",
  "name": "TravelExtract",
  "retargetProfile": "Imports/broad.judasretarget",
  "sampleRate": 60,
  "rootMotion": {
    "policy": "extract",
    "node": "Q00/Q01",
    "translation": [true, false, true],
    "rotationAxis": [0, 1, 0]
  }
}
```

This is the broad lab recipe's actual target root key. For another target, select
its **actual target root key** from the picker/profile; the example axis is
asset-local, not universal world-up. `trim:[begin,end]` is optional and output time
starts at zero. Optional `settings` describes the **source's** model-import unit/basis
normalization independently of the target recipe. Output names must be unique;
collisions fail before publication. The editor has an explicit choice to replace
an existing *retarget recipe operation* with the same name; native target clips
are not overwritten. Source clip selection must match an imported take exactly.

`preserve` retains skeletal motion. `extract` removes the declared model translation
axes/twist once and records the normal motion track. `inPlace` performs the same
selected-channel removal without storing locomotion; undeclared bob stays. No
entity transform or motor is automatically driven. Extracted loop interval queries
compose the existing motion frames. Already-extracted sources must be supplied in
unextracted form; retargeting does not silently forget their separate motion track.

## Editor workflow

Open the lab project, then **Asset Browser → Import & tools → Animation retargeting**
or **View → Animation retargeting**.
Enter `Sources/source.gltf`, `Imports/broad.judasimport` and
`Imports/broad.judasretarget`. Load source + target on the worker, load the profile,
validate, select Wave and Refresh comparison. Source is on the left, target on the
right, with the same time. Play, scrub, show declared reference and inspect skeleton
axes. Preview separation/orbit never enters output motion.

Mappings use the existing full-key skeleton picker. Reference corrections can use
quaternions or Euler-degree controls. Translation selection and scale are explicit.
Undo/Redo/Cancel affect the bounded local draft. **Save profile**, **Save recipe
operation** and **Bake / reimport target model** are distinct transactions.
Save/reopen a profile before using Bake. Scene Undo does not undo filesystem writes.
A changed draft is visibly stale until comparison is refreshed. Closing the panel
or changing project cancels its pending work; last-good cooked assets remain active.
Entering Play suspends the preview and defers completed publication until Edit;
it does not cancel the authoring jobs. Workers never run game scripts, physics or
raw OpenGL.

## Shared command-line workflow

Run from the repository root with the built tool:

```sh
build/judas_model_import_cli --retarget-validate /path/to/Project/Sources/source.gltf /path/to/Project/Imports/broad.judasimport /path/to/Project/Imports/broad.judasretarget
build/judas_model_import_cli --recipe /path/to/Project/Imports/broad.judasimport
```

`--retarget-profile SOURCE TARGET_RECIPE NEW_PROFILE SOURCE_ID TARGET_ASSET_ID`
creates an empty manual draft and prints keys/takes/signatures; it will not overwrite
an existing profile. `--inspect-motion SOURCE` prints the full source hierarchy.
Standalone profile inspection uses default model-import source metadata normalization;
custom source normalization belongs in the recipe/editor. Validation/cooking errors
return nonzero. CLI and editor call the same C++ service, not a second Python backend.
Retarget mathematics and validation remain in the shared native service.

## Runtime and authoring limits

No automatic contact preservation for different proportions, automatic anatomy,
many-to-many mapping, gameplay root application or performance repair. Translation
scale is author-selected, not inferred from a name or world-Y height.
Nonuniform/sheared/reflected participating transforms and animated scale are not
converted by this service. Positive uniform static transforms and importer
unit/axis normalization are valid.
Reference/bind equality and off-grid sampling tolerances are separate measurements.

A hierarchy may contain up to 4096 nodes (existing import limit); palette entries
follow the existing 16384 bound. Profile limit: 4 MiB, at most 4096 mappings/reference
corrections. A clip: up to 3600 seconds, 240 Hz, one million distinct float sample
times, 64 million sampled source-plus-target node evaluations and four million
emitted keys. The node budget includes unmapped helpers; a larger hierarchy lowers
the allowed sample count. A cooked assembly: up to 256 clips/four million keys,
plus the existing archive bounds. These are intersecting ceilings, not latency
promises.
Cancellation is checked while sampling. Reported working memory is an estimate,
not complete allocator accounting. Runtime has no retarget evaluation or source
skeleton demand; ordinary joint-palette uploads still occur for animation skinning.
The existing Windows import/cache staging path can exceed the ordinary Win32
path limit in deeply nested project/test directories. retargeting's native cook proof
uses a short test root; it does not claim general long-path support.
