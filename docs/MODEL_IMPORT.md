# Model import and reliable reimport (M66 candidate)

Current authoring contract, implemented against starting checkpoint
`3e5147a4ed97102200da91b4181c97b2a98942ba`. Human review is pending.
Judas normalizes content; scripts decide what the content means.
[Animation](ANIMATION.md) · [Materials](MATERIALS.md) · [Collision](M64_COLLISION.md)
· [JudasJS](JUDASJS.md) · [M66 results](evidence/m66/REPORT.md)

## Import, inspect, place

Open `projects/import_lab/import_lab.judasproj` in `build/judas_editor`.
In **Assets → Model import / reimport** choose the original file and a
project-relative output such as `Assets/models/actor.judasmodel`. **Copy source
and create import recipe** retains author-owned sources under `Sources/` and a
named JSON recipe under `Imports/`. Review units/basis and sampling, then import.
A missing dependency does not prevent creating an editable recipe: import reports
it; add an explicit project-relative dependency remap and retry. Import does not
search arbitrary directories or fetch files from the network.

The job parses/cooks/hashes on existing workers. Progress and Cancel are visible.
Only Edit mode publishes the complete accepted generation. During Play, Stop to
publish. The prior accepted generation remains inspectable if a replacement fails;
its status is stale, and export rejects it until source/settings/dependencies agree.

The accepted preview provides clip selection, playback, scrub, skeleton/identity
list, per-part visibility, orbit, bounds, one-metre ruler, basis axes and extracted
motion trail. **Place accepted model** creates a normal entity with a mesh and
Animation component. Normal duplication, undo, prefab creation and material
inspectors apply. This is one entity with immutable multipart mesh data and a
shared skeleton, not six independently driven clip players.

**Copy / add compatible motion** copies an animation-only file, chooses its source
take and gives it a project clip name. FBX takes are named by their source; the
original files use `mixamo.com`. An invalid choice reports available takes. Recipe
clip entries select, rename, trim, loop and choose root policy. Keep all clips by
omitting `clips`. No Blender/NLA merge is required.

## Same CLI, same service

```sh
build/judas_model_import_cli --create /absolute/project /absolute/actor.fbx Assets/models/actor.judasmodel
build/judas_model_import_cli --recipe /absolute/project/Imports/actor.judasimport
build/judas_model_import_cli --inspect /absolute/project/Assets/models/actor.judasmodel
build/judas_model_import_cli --collision /absolute/project/Assets/models/ramp.judasmodel 0 /absolute/project/Assets/collision/ramp.judascollision --remove-degenerates --orient
```

Recipe publication: exit **0** success, **2** success with warnings, **1** failure.
Collision failure does not invalidate the visual model. Direct `SOURCE OUTPUT
[MOTIONS...]` is a fixture/inspection operation; ordinary project imports use
`--create` / `--recipe`. It does not register assets by itself.

## Reviewable recipe fields

Paths below are relative to the project root. Do not use machine paths in authored
runtime data. Recipes belong immediately under `Imports/`.

```json
{
  "format": "JudasImport", "version": 1,
  "assetId": "the-existing-32-hex-Judas-asset-ID",
  "source": "Sources/actor/actor.fbx",
  "output": "Assets/models/actor.judasmodel",
  "settings": {
    "unitMeters": 0, "sampleRate": 60, "allowBaseMesh": false,
    "basisRotation": [0, 0, 0, 1],
    "dependencyRemaps": {}, "nodeAliases": {}, "materialAliases": {}
  },
  "motions": [{"source":"Sources/actor/walk.fbx", "take":"mixamo.com",
    "name":"Travel", "jointRemaps":{}}],
  "clips": [{"sourceClip":"Travel", "name":"TravelExtract", "trim":[0,1],
    "loop":true, "rootMotion":{"policy":"extract", "node":"__root/Hips",
      "translation":[true,false,true], "rotationAxis":[0,1,0]}}]
}
```

`--create` supplies a real valid ID; the illustrative ID above is not executable.
`unitMeters=0` uses FBX metadata; glTF/OBJ assume metres visibly. A positive override
is deterministic, not a character-height guess. Basis quaternion is x/y/z/w and
rotates after metadata normalization. Import axes do not prescribe gravity.
`settings.selectedParts` optionally selects exact stable part identities. Default imports all.
`settings.nodeAliases` maps an exact new hierarchy key to its retained local node
name (one name, not a full replacement path). Ancestor names determine the final
path; alias renamed ancestors too when needed. `settings.materialAliases` maps
new material keys to retained material keys. Ambiguous identity or deleted required
joints blocks acceptance until resolved.
Motion `jointRemaps` maps source hierarchy keys to target hierarchy keys. Verified
hierarchy/bind compatibility is still required; this is not general retargeting.
`dependencyRemaps` maps reported source names/paths to approved project files.
Trim bounds must lie in the selected take. Loop metadata is authored separately
from instance playback looping.

## Supported inputs and bounds

| Input | Current supported path |
|---|---|
| FBX | Pinned ufbx 0.23.1, commit `26a482ae66871d7de36eb722aa060bce95bce274`, MIT. Metadata units/axes, parser-evaluated pivots/inheritance, multipart linear skinning, static attachments/instances, named takes, compatible motion files. |
| glTF/GLB | Existing pinned cgltf 1.15, MIT. Multiple nodes/skins/armatures, per-skin inverse binds, embedded/external approved buffers/images, STEP/LINEAR/CUBICSPLINE, UV0/UV1 and eight weights. |
| OBJ/MTL | Existing tinyobjloader. Triangles/polygons, grouping, normals/UVs, diffuse/normal/emissive maps and supported scalar factors. Direct legacy OBJ loading remains available. |

FBX uses evaluated affine defaults plus animated local TRS rather than projecting
required shear into an inaccurate TRS. Mesh-specific bind matrices remain separate.
Static mesh nodes receive rigid skin bindings so their hierarchy motion shares the
ordinary final-pose path. Multiple independent armatures remain distinct branches.
Normals use inverse transpose; winding/tangent handedness follow reflections.

Limits are resource bounds: 4096 hierarchy nodes, 16384 per-skin palette entries,
eight influences, two million vertices/six million indices, 256 clips/four million
track keys. GPU palette delivery additionally checks GL texture-buffer capacity.
FBX source 256 MiB/parser allocation 512 MiB; image/dependency 64 MiB; hierarchy
depth 256; cooked archive 256 MiB with bounded CBOR nesting/events/containers.
Invalid indices, cycles, nonfinite/singular affine/bind data and unsupported excess
weights fail. Loss is never silently attributed to a successfully preserved rig.

Dual-quaternion skins, morph/cache deformations and unsupported extra UV channels
are rejected. Explicit `allowBaseMesh` permits a lossy undeformed base where
implemented and reports that choice. No runtime source constraint solver, general
retargeting, Draco, DCC shader graph reproduction or arbitrary FBX deformation.
Per-part front-face selection assumes consistent reflection within that part;
mixed per-vertex reflection inside one skinned part is not a fidelity promise.
Two-bone IK/ragdoll still require their existing rigid/physical mapping constraints;
retaining affine render data does not authorize dynamic nonuniform collider scale.

## Appearance and stable parts

Imported material defaults retain supported M57 colour/data-map paths, scalar
metal/roughness, normal, emission, opacity/cutout, double-sided and UV/sampler
settings. FBX Phong/graph conversion is an approximation with warnings. Separate
metal/rough maps are packed only with compatible UV transforms. Opacity combines
source factor and colour; textures carry their real alpha. Tangents use pinned
MikkTSpace and the normal map's selected UV set. UVs beyond UV1 are diagnosed.

Embedded/external images become cooked runtime content. Source-relative Windows
separators and Unicode work on Linux. Ambiguous matches require remapping.
Normal map/data channels remain linear; colour maps use the normal colour path.
Imported defaults, separate author-owned `.judasmat` assets and instance overrides
have different ownership. Reimport does not overwrite author material files.

[Entity.modelParts / setPartVisible](judasjs/entities.md)
addresses exact part keys; all instances share immutable content but own hidden
part sets, material overrides and playback. Skinning uses eight influences and a
size-aware GL3.3 texture-buffer palette in main, shadow and secondary-camera passes.
Bounds conservatively include all palette-transformed mesh bounds; no full-vertex
bounds scan per frame. GPU material definitions drop encoded source image bytes
once uploaded, so draw-time material copies do not copy photographs every frame.

## Root policies and gameplay

`preserve` retains source travel. `inPlace` removes selected model-space translation
axes and optional twist. `extract` stores that motion and the complementary pose.
The author chooses the node; no hardcoded Hips/Walk/Skate rule. The initial motion
track is identity, retaining source initial offset in the complementary pose.
For selected twist `R`, extracted translation is `delta + p0 - R*p0` so applying
extracted rigid motion reconstructs source placement about the original pivot.
Unselected vertical/bob/rotation remains in the pose. Nonlinear bake defaults to
60 Hz with 1e-6 key reduction; independent source samples are checked with explicit
error tolerances. Exact source discontinuities retain distinct sample times.

[animation.rootMotion](judasjs/animation-ragdolls.md) returns an interval rigid
transform in model space, with loop crossings and reverse intervals. It is a
named clip query, **not live blended mixer root motion**. No automatic root movement
or physics teleport occurs. Lab **B** converts WalkingExtract displacement into
CharacterMotor velocity; the wall stops it. Existing gameplay defaults stay in
place unless scripts explicitly consume an extracted track.

## Reimport, saves, streaming, package

The self-contained `.judasmodel` version-1 archive carries rig/mesh/clips/materials,
provenance and stable mapping manifest. Existing AssetDatabase identity is reused.
Content and tool revision hashes omit timestamps, pointers and absolute paths.
Geometry/material cache products are independent of compatible motion products.
Unchanged reimport reuses accepted data (decodes to preview, no source reparse).
Changing a motion/dependency invalidates its dependent products. Missing/malformed
inputs, cancel or edits while staged leave the accepted generation intact.
Generated cache deletion leaves author recipes intact and reproduces accepted bytes.
Current cache corruption reports an error; remove generated cache and retry.

Publication replaces one coherent archive at Edit/CLI boundary; sources/recipes
are retained. No automatic garbage collector deletes author data. Source edits
require reimport before export. Modern save fingerprints use ordinary asset content;
changed rig/asset bytes reject incompatible slots before replacing the world.
Legacy fingerprint schema remains 5; nonempty authored hidden-parts fields are
conditional so default legacy scenes retain their prior fingerprint.

Snapshot streaming retires an ordinary animated region using the existing M61
instance pose serializer (clip/mixer/layers/external/source/final poses). Revisit
gets fresh generation-safe handles. The bounded retained record participates in
modern saves: streaming participant version 3 only when animation snapshots exist;
versions 1/2 remain supported. Active ragdolls and visual return transitions stay
pinned until physics authority ends. No runtime FBX cook or streaming redesign.

M38 exports cooked runtime assets and regular dependencies, not `Sources/`,
`Imports/`, caches or import tools. Player contains archive decoding, not ufbx.
Moved, read-only packages use the normal writable save location. Authoring dependency
licenses/provenance remain in `third_party`; runtime notices include the archive
JSON library. Asset rights apply to source **and** cooked derivatives.

## Import lab and original assets

Original character: 65 meaningful source bones, 72 source hierarchy nodes, six
retained parts, 109 mesh-specific skin palette entries; 73 runtime hierarchy nodes
include one generic root-track helper. Approximately 1.801 metres tall. Push,
Cruise, Walking, WalkingExtract and PushInPlace use the original separate files.
No join, finger folding, bone deletion, external unit surgery, root stripping or
NLA clip assembly. Manual choices are import settings, take names, root policy,
material remaps and a physical subset.

Raw/cooked original character and artist half-pipe stay local/ignored pending
redistribution rights. CC0 analytic fixtures and sources are tracked. To assemble
from lawful original files, use the normal panel/CLI; `scripts/m66_create_lab.py`
only constructs this demonstration with the public CLI (no DCC conversion).
The generated `scripts/m66_create_consumer.py` clone retains the accepted Three
Games/reduced-rig tricks and adds a full-rig comparison beside Skate. Reduced rig
trick poses are deliberately not name-retargeted to the original skeleton.

Controls: **G** clips/crossfade, **J** arm mask, **K** additive spine, **I** IK,
**F** ragdoll/return, **T** part, **P** prefab, **B** extracted motor intent,
**U** stream load/retire/revisit, **F5** reload, **F6/F7** modern save/load,
**Esc** pause, mouse + **WASD** separate inspection camera.
