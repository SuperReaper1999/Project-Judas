# M95 — animation production workflow

Current optional Judas animation workflow, based on accepted M94. Linux automated
validation and operator acceptance are separate. See [evidence](evidence/m95/REPORT.md).

## Content → timeline → runtime

1. Import a skinned glTF/GLB or supported FBX through the ordinary model importer.
2. In Model import, populate clip selections. Choose trim, output name and the
   existing `preserve`, `inPlace` or `extract` root-motion policy.
3. Open **View → Animation tools → Animation timeline**, choose the project's `.judasimport`, then
   **Load / reimport selected recipe**. Normal asynchronous cooking, validation
   and Edit-only publication apply. Preview is independent of gameplay.
4. Choose a clip. Play, scrub, orbit and inspect its skeleton/root path. The marker
   track shows the cursor and events; clicking a marker selects it, other clicks
   scrub. Add at cursor, edit seconds/name/data/ID, move to cursor, delete or undo.
5. **Save event track to recipe** writes only that clip's marker array. **Load /
   reimport** publishes the edited asset. Switching clips, assets or projects preserves an unsaved draft until an
   explicit Save or Discard choice. Closing the timeline retains its draft.
   Disk conflicts reject saving. Invalid
   times/IDs reject saving and cooking. **Discard draft / reload recipe** recovers
   disk content. Recipe edits have their own bounded draft undo, not Scene undo.
6. Test in Play, then export normally. Runtime needs only the cooked model and
   registered game scripts; source art and importer are not runtime dependencies.

During Play, the timeline switches to read-only inspection of the selected live
animated entity: phase, clock ownership, blend fraction, marker sample and
diagnostics. Pause to select it in the hierarchy; Stop to edit content.

Opening/scrubbing/previewing never moves scene entities or emits gameplay events.
The timeline is an event editor and clip inspector, not a skeletal keyframe DCC.
Use Blender or another source tool to author bone animation. Retargeting remains
in the existing [M74 workspace](M74.md).

## Event format and timing

Each selected output clip in `.judasimport` may contain:

```json
"events": [
  {"id":"left-foot", "name":"footstep", "time":0.3, "data":"stone"},
  {"id":"contact", "name":"contact", "time":0.6, "data":"right-hand"}
]
```

Seconds refer to the **output clip after trimming**. Up to 1024 markers per clip;
IDs are unique within that clip, ID/name are nonempty and at most 128 bytes,
`data` is opaque UTF-8 text at most 4096 bytes. Embedded NUL is unsupported
by the current text UI/bridge; encode binary data yourself if needed. The engine never interprets names
or data. Times must be finite and within `[0,duration]`. Marker array order is
not an execution-order guarantee: ties use stable marker ID ordering.
Existing marker tracks survive trimming (times rebased); explicit recipe events
replace them after trim. Author target-output events after retargeting; retarget
baking does not infer source gameplay marker meaning.

Forward playback reports `(from,to]`; reverse reports `[to,from)`. Loop crossings
and multiple wraps are included. A zero-time marker fires when a loop crosses
zero, not implicitly on Play/Seek. A duration marker and zero marker are distinct
and can both fire at the same loop boundary. Author one if only one is wanted.
Pause, seek, Stop and pose/IK recomposition produce no new events. Rendering does
not advance the clock. Nonloop playback clamps to its endpoint.

Base, outgoing fades and enabled layers report their own markers with `clip`,
`source`, `weight` and `reverse`. Source labels are `base`, `outgoing:N` and
`layer:<id>`. JS chooses its duplicate/weight policy; the engine does not decide
which footsteps or combat actions matter. Weights use the fade's fixed-step
midpoint. They describe blend influence, not probabilities or damage strength.

At most 256 crossed markers are published per instance step, with at most one
million skipped cycles. Overflow returns **no partial marker list**, sets
`overflow:true` and an animation diagnostic. It never silently drops the tail
while claiming complete delivery. Reduce authored density/control speed rather
than rely on overflow for game logic. Event timestamps are clip times; effects
are submitted at the authoritative fixed boundary, not a substep callback.

## Fixed-step root motion and events

`entity.animation.advance(dt)` explicitly advances the existing mixer once,
returns `{motion:{translation,rotation},events,overflow}`, then pauses automatic
playback. Call it once per entity per `fixedUpdate`; resume/play/crossFade restores
the ordinary automatic clock. Missing assets, invalid intervals (must be
`0 < dt <= 1`), calls outside `fixedUpdate`, and a second advance in the same tick
fail safely. No second solver or parallel script clock is introduced.

`animation.sample` is a detached observation of the last evaluated fixed sample,
not a queue. A held-clock fixed interval publishes an empty sample; frame-only
inspection never creates a new interval. Automatic playback also publishes a sample. Read it once in the
next `fixedUpdate` if using automatic playback; frame reads may repeat it and
catch-up may replace it. For gameplay requiring every event, use `advance` and
consume its result immediately. Do not consume both paths for the same sample.

The CharacterMotor recipe uses unit entity scale; normalize size/units at import.
`advance` samples the clip/mixer clock and marker interval before motor motion;
it does not promise a final world-space IK pose inside that callback. M70
world-space references resolve after the pre-physics motor. Legacy world-space
limbs resolve after the ordinary motor. These stages may recompose a zero-time
pose without advancing clips or emitting markers again; the IK solve itself is
deferred until the appropriate moved-root reference frame. Read final joint
poses in the normal completed-step/presentation phases.

Root motion remains model-local metres/quaternions. It blends base/fading clip
intervals using midpoint weights; layers never implicitly move the world root.
Large turning deltas through a fade are an approximation, not exact integration
of a continuously blended trajectory. Extracted source motion is removed from
the pose at cook time; do not apply it again as a transform teleport.

```js
fixedUpdate(dt) {
  const a = this.entity.animation;
  if (!a?.info.ready) return;
  const sample = a.advance(dt);
  // rotate() is project quaternion-vector math, not an engine global function.
  const q = this.entity.transform.rotation;
  this.entity.character.displace(rotate(q, sample.motion.translation));
  for (const event of sample.events) {
    if (event.source === 'base' && event.weight >= .5 && !event.reverse)
      this.handleMarker(event); // sounds, effects and game rules live in JS
  }
}
```

`Character.displace(worldMetres)` adds one-step displacement to normal velocity,
gravity and support motion through the **existing capsule sweep/slide/step path**.
Blocked displacement does not become persistent velocity; external velocity
receives collision response on each actual contact plane. Push impulses retain
the existing finite mass/budget model. Inspect `actualDisplacement` after the
motor resolves (next fixed callback), and decide stride/animation policy in JS.

`Character.orient(worldQuaternion)` supplies orientation intent without resetting
position, support or velocity. Gravity alignment/rate remains authoritative. Use
it for a heading or compose the returned root rotation in JS. Capsule orientation
uses existing overlap recovery; this is not a new continuous angular-sweep solver.
Both new motor operations belong in `fixedUpdate`, before motor resolution.
They do not poll input, choose clips, jump, swim, own a camera or own bones.

For kinematic mechanisms, compose the same model-local motion into an ordinary
world target and call existing `entity.moveKinematic(target,dt)`; dynamic bodies
should use existing physical forces/motors, not root-transform writes. The root
sample is reusable, but it does not change a body's authority mode automatically.

## Saves, streaming and ownership

The existing playback/mixer/layer phase and pause state remain persisted. An
optional versioned `animation-observations` save participant is emitted only
when a loaded animation asset contains markers or extracted motion. It retains the last
sample without replaying it. Old saves without it start with an empty observation.
Streamed records for those assets include the same observation and accept the
previous record format. Asset identity/strict save compatibility still applies.
A sample is an observation, never a pending command or durable native handle.

New fixed-step motor intents are consumed once and are not persistent velocity.
They cannot be submitted from frame/UI/contact callbacks after motor resolution.
Restoration clears transient intent. Stop, scene replacement, destruction and
resource replacement retire instance observations; no marker data mutates shared
assets. IK, retargeting and ragdolls keep their existing pose/physics ownership.
An animation sample does not implicitly reactivate or move a passive ragdoll.

## Lab and limits

[Motion Studio](../projects/animation_studio/README.md) provides original skinned
walking/running/jump/interaction/strike clips, marker-driven feedback, a physical
strike target, root-motion collision, blending, flat/rotated scenes and a watch
tour. Its compact source art is a readable authoring example, not finished game
animation. Gameplay is project JS.

Deferred: morph-target animation, bone-key editing in the editor, state-machine
or motion-matching tools, foot locking/stride warping, exact blended SE(3) root
integration, automatic dynamic-body root authority, and sub-fixed-step event
callbacks. Existing DCC/import/IK/ragdoll limits continue to apply. Windows and
Android M95 functional/visual validation are outstanding.
