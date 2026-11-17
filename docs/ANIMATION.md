# Animation, poses and skinning

Use the editor to import and preview a rig, project scripts to control playback,
and the shared runtime to resolve its pose. [Model import](MODEL_IMPORT.md) owns
source conversion; [the animation workflow](ANIMATION_WORKFLOW.md) owns clip
selection, root motion and event editing. [Retargeting](ANIMATION_RETARGETING.md)
is an import-time bake, not a second runtime or an automatic rig-conversion service.

## Assets and ownership

`SkeletalAsset` contains immutable skeletons, clips, skin palettes and inverse
binds. Resources decode/precompute on workers; the renderer creates and destroys
GPU resources on its context thread. Each `RuntimeWorld` animation instance owns
its playback clocks, mixer, layers, contributions and resolved poses. Instances
share assets without sharing playback or mutable damage/physical state.

Skeleton nodes are hierarchy transforms, not rigid bodies. Imported parts can
have independent palettes and up to eight influences per vertex. The native
asset limits are 4,096 hierarchy nodes and 16,384 palette entries; hardware and
resource validation can impose a smaller usable envelope. Do not use the removed
48-joint/single-mesh import restrictions. Supported formats, affine transforms,
external dependencies and material import are specified in [Model import](MODEL_IMPORT.md).

STEP, LINEAR and CUBICSPLINE playback use the normal sampler. Rotation sampling
normalizes quaternions. Renderer passes consume the resolved skin matrices; drawing
main, shadow or camera views does not advance clips or move physical collision.

## Mixing and layers

The base mixer supports crossfades. Interrupted transitions retain the current
weighted contributors and their clocks; zero-duration fades replace immediately.
There are at most 16 outgoing contributors. Excess interruptions fail explicitly
until the current transition retires contributors.

Authored/runtime layers resolve in list order after the base. An override layer
interpolates from the accumulated local pose. An empty mask affects every joint;
an explicit mask selects local transforms, while parent transforms still affect
descendants. Use stable imported hierarchy keys. Unique leaf names are accepted;
missing or ambiguous names fail. Renaming a source joint requires reviewing masks.

Additive layers compare a clip sample to rest or an explicit reference clip/time:
translation differences, relative rotations and multiplicative scale ratios.
Zero reference scale is invalid. Native external pose producers submit copied,
validated contributions through `RuntimeWorld::SetPoseContribution`, sorted by
priority then source name after authored layers. `SetFinalPose` routes through
that ownership rather than becoming a second skeleton owner.

```js
actor.animation.play('Walk');
actor.animation.crossFade('Idle', 0.25);
actor.animation.layer('upper-body', {
  clip: 'Wave', weight: 0.7, enabled: true, mask: ['Arm/Shoulder', 'Arm/Elbow']
});
actor.animation.removeLayer('upper-body');
```

Clip names and joint keys are examples: choose names actually present in your
asset. Pause holds clip clocks. Updating a layer with the same clip retains its
clock; choose a new clip/layer when a restart is intended. Runtime layers do not
rewrite authored scene files. See [exact signatures](judasjs/animation-ragdolls.md).

## Motion, IK and physical animation

Use `animation.rootMotion` for a model-local interval query, or `advance(dt)` once
per fixed callback when gameplay must consume every root-motion/event sample.
Apply movement through the appropriate motor or body authority; root-motion
sampling alone does not move an entity. Do not consume automatic and manual
samples twice. The [production workflow](ANIMATION_WORKFLOW.md) specifies timing,
marker limits and modern save behaviour.

[Multi-target IK](MULTI_TARGET_IK.md) solves explicit simultaneous contact frames.
[Physical animation](RAGDOLLS.md) supplies authored body/constraint mappings and
explicit authority transitions. [Balance](BALANCE.md) is an additional bounded
controller, not implicit behaviour of every IK rig. There is no automatic natural
get-up controller, motion synthesis or skeletal keyframe DCC in the editor.

In the authoritative fixed step, scripts submit intent first; opted-in animation
references/IK and physical targets prepare before dynamics; resolved animation,
ragdoll poses and sockets publish afterward. Visual presentation uses the shared
interpolation boundary. Use authoritative joint/body observations for simulation
and presented joint/socket observations for cameras and visual attachments.

`ikStatus.ready` means the asset is available, not that a newly configured or
cold-restored rig has produced residuals. Inspect targets after a real fixed solve.

## Save, edit and export

Animation clocks, supported mixer/layer configuration, physical modes and pending
requests participate in normal `WorldPersistence` saves. Transient solver results
are rebuilt. Fresh-process continuation compares phase, motion, events, hand pose
and target counts through the real authoritative step. Stop returns to authored
content; Play constructs a fresh runtime. Export includes cooked model dependencies,
not a runtime requirement for source art or retarget profiles.

`projects/animation_studio` is the current editable source example; it is listed
as source-only rather than bundled in every SDK. Native fixtures live under
`tests/fixtures/animation` and `tests/fixtures/ik`, with their required licences.
Source owners: `SkeletalAnimation.*`, `WorldAnimation.cpp`, `FullBodyIK.*`, `Simulation.cpp` and the animation scripting
module. The declared API and [qualification matrix](DEVELOPMENT_READINESS.md)
separate source capability from tested device/performance scope.
