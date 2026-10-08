# M46 — skeletal-animation foundation

**M70 candidate extension:** shared multi-target IK and optional partial/full-active
physical regions now extend the historical clip/pose/passive-ragdoll scope below.
The older milestone statements remain their historical contracts. See
[M70](M70.md) for current phases, authority and limitations, and the
[JudasJS reference](judasjs/animation-ragdolls.md) for exact public interfaces.
M61 already persists animation/articulation runtime state; M70 extends that
ownership for physical modes and pending requests.


Current public JavaScript signatures, examples and lifetime rules: [JudasJS reference](JUDASJS.md). This document retains milestone architecture and evidence context.

## Ownership and pipeline

`Skeleton` stores the full node hierarchy/rest local transforms and a skin
palette with inverse bind matrices. `SkeletalPose` is joint-local TRS data,
independent of playback. `SampleClip(skeleton, clip, seconds)` is addressable
without rendering or stepping physics. `ResolveSkinMatrices` composes the
hierarchy and inverse binds.

The normal mesh ResourceManager reads/decodes on a job worker, retains one
immutable `SkeletalAsset`, and uploads an optional joint/weight vertex buffer
on the Renderer/context thread. Static meshes keep their old vertex layout.
Each ordinary RuntimeWorld entity with an Animation component owns independent
playback, final pose and matrix palette. The fixed step samples once; main,
shadow and secondary camera passes consume that result. No renderer changes
simulation, and no skinned vertex changes authoritative collision geometry.

Clip playback **does not own the final pose**. RuntimeWorld resolves the clip
source into its final pose at `UpdateAnimations`; `SetFinalPose` can supply a
validated independent pose after that boundary. A later producer/combiner can
replace that resolution policy without changing assets or GPU skinning. M46
implements one active clip, not blending, IK, ragdolls or physical bone mapping.
M45 rigid joints are distinct from skeleton nodes.

## Historical M46 import and limits

**Superseded by accepted M66:** [Model import](MODEL_IMPORT.md) documents the
current multipart/affine/eight-weight import path and size-aware palette. The
following retains the original M46 scope; its 48-joint/one-mesh restrictions
do not describe accepted M66.

Import `.glb` or `.gltf` as an ordinary Mesh asset. cgltf is pinned to 1.15
(MIT, Johannes Kuhlmann); source/header/license/provenance are under
`third_party/cgltf`, and the full notice is also in exported runtime notices.
Supported: one skinned mesh node/skin, triangle primitives, positions/normals/UV0,
JOINTS_0/WEIGHTS_0 (four nonnegative normalized influences), hierarchy/rest TRS,
inverse binds and named translation/rotation/scale clips. Missing normals are
generated. STEP, LINEAR (normalized quaternion slerp) and CUBICSPLINE (Hermite,
normalized rotations) are supported. Matrix rest nodes may not receive TRS
animation; shear is rejected. Maximum 48 skin joints/128 total nodes.

Use self-contained GLB or data-URI embedded-buffer glTF. External buffer files,
multiple mesh nodes/skins, Draco, morph targets and additional influence sets
are rejected with useful resource errors. The normal Judas authored texture/tint
is used; glTF material/PBR/image import is not implemented. Mesh-node transform
is not applied a second time to skinned geometry: joint world transforms times
inverse binds produce asset-space vertices, then the authored entity transform
places them in Judas. No root-motion gameplay or animation collision is implied.

## Authoring and playback

Editor: import/choose the mesh, add an **Animation** component, set enabled,
play-on-start, clip name, loop, speed and starting time. The regular entity
transform places/rotates/scales the result. Prefabs/overrides serialize the same
component fields. Disabled playback holds the last resolved visual pose;
play-on-start false starts paused at the authored sample.

```js
const animation = world.entity('10').animation;
if (animation.info.ready) {
    console.log(animation.clips); // [{name, duration}, ...]
    animation.play('Wave');       // named play restarts that clip
    animation.loop = false;
    animation.speed = 2;
    animation.seek(0.5);          // seconds; loop wraps, non-loop clamps
    animation.pause();
    animation.resume();          // continue current time
    animation.stop();            // rest pose, time zero
}
```

Playback can run backwards via negative speed; a non-loop reaches its endpoint
and stops. To restart an ended clip call `play(name)`. Unknown named clips and
invalid/stale entity handles throw safely. Before async readiness play/seek/stop
return false; query `info.ready`. An unknown authored clip holds rest pose.
Runtime changes never rewrite authored settings. Play/Stop, scene reload and
project teardown discard instance state; normal asset lifetime/refcounting
protects shared data. M38 exports registered mesh assets and notices unchanged;
self-contained buffers require no export-specific resource path.

Canonical fingerprint schema remains **5**. A tagged `Judas.SkeletalPlayback.1`
extension includes authored component settings only when present. No-animation
baselines are unchanged; runtime time/pose changes are not authored changes.
This retains the existing asset-reference compatibility policy rather than
inventing content identity for an animation player.

## Demo and checks

`projects/animation_demo/animation_demo.judasproj`: an original three-joint
segmented bar (CC0), Wave and Stretch clips. Orange is controlled through JS;
blue shares the asset but has independent playback and an oblique world rotation.
P spawns an ordinary animated prefab. G selects the other clip and toggles
loop/non-loop; J pauses/resumes; K seeks to 0.5 seconds; C toggles 1x/2x; Esc pauses
the game. WASD/mouse use normal project input.

Run `python3 scripts/m46_validation.py` for the one clean Release/production
candidate gate plus editor/standalone/moved-export smoke. Focused executables:
`judas_skeletal_animation_tests` (CPU references, actual GL/async/lifecycle/perf)
and `judas_animation_application_tests` (normal Application/JS/reload/pause).
Evidence is under `docs/evidence/m46`. Automated pixel/state checks do not grant
human visual acceptance.

### Human checklist

- No detached/exploding vertices; hierarchy bends sensibly.
- Switch Wave/Stretch; test pause/resume, seek, speed and one-shot completion.
- Two instances animate independently; the rotated instance remains correct.
- P creates independent animated instances.
- Reload/Stop leaves no stale animation state.
- Moved exported standalone matches the demo.

### Later candidate extension

M47 extends the historical single-clip M46 scope with a resolver, crossfades,
ordered/masked and additive contributions. `SetFinalPose` now submits a validated
external contribution instead of bypassing resolution. M48 maps passive physical
articulations into that resolver. See [POSE_COMPOSITION.md](POSE_COMPOSITION.md)
and [RAGDOLLS.md](RAGDOLLS.md); the import and renderer ownership above are retained.
