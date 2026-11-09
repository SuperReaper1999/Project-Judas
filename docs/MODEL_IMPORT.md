# Model import and reliable reimport

Introduced against M66 starting checkpoint
`3e5147a4ed97102200da91b4181c97b2a98942ba`. The accepted content contract remains.
accepted M68 cache/readiness changes are described in [Scalability](SCALABILITY.md).
Judas normalizes content; scripts decide what the content means.
[Animation](ANIMATION.md) · [Materials](MATERIALS.md) · [Collision](COLLISION_AUTHORING.md)
· [JudasJS](JUDASJS.md) · [M66 results (historical)](https://github.com/SuperReaper1999/Project-Judas/tree/2ee4203a0053cc545eab4a184c627f4f5507c4bd/docs/evidence/m66/REPORT.md)
· [Editor/import-time retargeting (M74)](ANIMATION_RETARGETING.md)

## Import, inspect, place

**Clean checkouts:** the import lab's scene uses the original character, which is
local-only (see [Import lab and original assets](#import-lab-and-original-assets)).
Without those files the lab scene fails to build (`unknown asset id`), and so does the
[imported-model example](judasjs/examples/imported-model.js). The workflow below still
applies to your own sources and the CC0 fixtures in `tests/fixtures/m66`.

Open your project and choose **File → Import assets…**. Pick an FBX, glTF, GLB or
OBJ file and review the cooked destination inside Assets, such as
`models/actor.judasmodel`. **Cook & import model** copies the author-owned source
under `Sources/`, saves an editable JSON recipe under `Imports/` and starts the
worker. **Import & tools → Model import / reimport** shows progress and the
accepted preview. A valid import also appears in the ordinary asset list.

Suspicious scale now pauses editor publication for an explicit review in
**Import & tools**. The warning shows the normalized rest-pose X/Y/Z dimensions
in **metres** and explains a possible 100× centimetres/metres mismatch. Choose
**Keep this scale and import** for an intentional large level or tiny prop, or
**Cancel / review units**, change **Source metres per unit**, and retry. The
previous accepted model remains intact until confirmation; nothing is silently
rescaled. The source-only registration option explicitly bypasses normalization
and this scale review.

This is authoring advice, not knowledge of an object's intended real size. It
checks selected rendered parts, including their rest-pose skin/bind transforms:
dimensions of 100 m or more, nonzero dimensions of 1 cm or less, and effective
geometry transforms around 100× or larger warrant review. A distant world
position is not a large dimension. Correctly normalized centimetre FBX rigs are
not warned merely for retaining a compensated 100× hierarchy scale; a correct
0.01 unit override that produces ordinary metre-size geometry clears size advice.
Warnings remain in import reports through receipt/cache reuse, including CLI
success-with-warnings (exit 2). CLI publication remains an explicit author/tool
operation and does not display the editor confirmation card.

Use **Source and recipe settings** in that section to review units, basis,
sampling and remaps. To configure these before the first cook, use **Copy source
and create import recipe**, review the settings and then choose **Import / reimport / retry**.
A missing dependency leaves an editable recipe and a diagnostic. Add an explicit
project-relative dependency remap and retry. Import searches approved sources
and dependencies and does not fetch files from the network.

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

**Different compatible skeletons:** M74's [retargeting workspace](EDITOR_GUIDE.md#animation-retargeting)
uses one reusable `Imports/*.judasretarget` correspondence/reference profile.
The target recipe adds a `motions[]` entry with `retargetProfile`, source take
and ordinary output clip name. This extends motion assembly, rather than adding
a runtime animation-clip attachment system. Different names, proportions, local
axes and unmapped helpers are handled through explicit full hierarchy keys and
reference frames; **Copy / add compatible motion** remains the original
same-compatible-skeleton path.

Source normalization and target normalization are independent authoring inputs.
Retarget output is resampled **LINEAR** local TRS tracks in the ordinary target
`.judasmodel`. Animated STEP discontinuities are diagnosed as unsupported for
this transfer; ordinary importing/playback of STEP tracks remains supported.
An already-extracted source motion track is explicitly rejected before cooking,
so the author must choose a source retaining its original skeletal locomotion.
The transferred operation then uses the existing preserve/in-place/extract root
policy, not another root-motion system. See [M74](ANIMATION_RETARGETING.md) for the exact recipe,
shared CLI, measured tolerances, invalidation and supported limits.

## Same CLI, same service

```sh
build/judas_model_import_cli --create /absolute/project /absolute/actor.fbx Assets/models/actor.judasmodel
# A .judasproj file is also accepted in place of /absolute/project.
build/judas_model_import_cli --recipe /absolute/project/Imports/actor.judasimport
build/judas_model_import_cli --inspect /absolute/project/Assets/models/actor.judasmodel
build/judas_model_import_cli --collision /absolute/project/Assets/models/ramp.judasmodel 0 /absolute/project/Assets/collision/ramp.judascollision --remove-degenerates --orient
```

Recipe publication: exit **0** success, **2** success with warnings, **1** failure.
Collision failure does not invalidate the visual model. Direct `SOURCE OUTPUT
[MOTIONS...]` is a fixture/inspection operation; ordinary project imports use
`--create` / `--recipe`. It does not register assets by itself.

## Explicit rig unit normalisation

The local V2 authoring tool can remove inherited centimetre/metre rig scale
without changing the character's physical size. This is an explicit **derived
asset** operation, not a change to ordinary imports or an automatic repair.
Use a cooked model to retain its current clip names, events and root motion:

```sh
judas_model_import_cli --normalize-rig Character.judasmodel Character_Metres.judasmodel
```

FBX, glTF and GLB sources are also accepted through the existing importer. The
output is always a new **Judas `.judasmodel`**, not an edited FBX or Blender file.
The tool refuses existing destinations and never changes the source, project,
recipe, scene, saved game or installed SDK. Keep the JSON output as its receipt:
it includes source/output hashes, coordinate scale, sampled-pose count and
maximum skin/bone matrix comparison error. Exit zero means successful conversion;
exit one includes a diagnostic and does not replace an existing destination.
Publishing uses a same-filesystem hard link from a completed temporary file;
filesystems that cannot create hard links fail explicitly.

The conversion chooses positive uniform coordinate units for each rest joint.
For joint `i`, its new global matrix is `oldGlobal[i] * scale(1 / units[i])`.
Local translations, fixed affine nodes, animation translations/scales (including
cubic tangents), inverse binds and vertex positions are converted consistently.
Consequently `newSkin * newVertex = oldSkin * oldVertex`; normal directions are
preserved under the positive uniform coordinate conversion. Animated authored
stretch/squash is retained. World-space extracted root motion, event payloads,
clip times, names, topology, materials and bone identities remain unchanged.
Before publication, native pose evaluation compares rest, every clip key time
and three interior samples per distinct key interval. These are sampled checks
of the algebraic invariant, not a claim to exhaust all continuous animation.

Initial support is deliberately limited to positive, uniform, shear-free rest
frames and skinned models before LOD generation. Reflections, non-uniform rest
frames, shear, singular/extreme scales and failed equivalence checks reject with
diagnostics. Review unsupported rigs in the source modeller. The archive encoder
performs its normal structural validation before output publication.

**Adoption needs a deliberate game-side pass.** Bone origins retain their model
positions, but joint coordinate units change. Old `0.01` socket compensation,
local weapon offsets/scales, IK/ragdoll profiles and external animation/retarget
signatures must be reviewed. Do not silently replace an existing registered
asset or expect old saved references to adopt a new asset identity. Register the
derived model through ordinary **Import & tools**, or `--create` / `--recipe`,
under a new asset name, test it, then switch the chosen entities. Recook collision
for the new model; existing collision assets are not rewritten. Its old recipe
record is deliberately cleared because it is no longer that unmodified product.

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
implemented and reports that choice. No runtime source constraint solver, universal/runtime
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

## Optional runtime cooking (M93)

Rigid imported models can additionally be cooked to `.judasmesh`, preserving
material parts, source topology and embedded map roles while adding render-only
LODs and compressed streams. Source import/reimport remains authoritative and
separate. Animated assets continue through the normal import path; rigid cooking
rejects them. Textures can use `.judastex` mip chains. See
[asset cooking/residency contracts](ASSET_PIPELINE.md) and
[The Packed Museum (historical)](https://github.com/SuperReaper1999/Project-Judas/tree/2ee4203a0053cc545eab4a184c627f4f5507c4bd/projects/asset_lab/README.md).
