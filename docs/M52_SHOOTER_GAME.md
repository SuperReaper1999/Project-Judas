# M52 — Spring Range

**Status through M69:** M52 is operator-accepted and checkpointed. Candidate/pending
statements and measurements below record the original milestone review state,
not a current outstanding acceptance gate. Later contracts take precedence;
start with [Architecture](ARCHITECTURE.md) and [JudasJS](JUDASJS.md).

Starting checkpoint: `73698d1a1f928c7210679e58fdb5259d2a196c7b`.
Candidate awaiting operator gameplay/visual/listening acceptance; no checkpoint made.

## Play and edit

Project: [`projects/shooter_game/shooter_game.judasproj`](../projects/shooter_game/shooter_game.judasproj).
Scene: `projects/shooter_game/Scenes/range.judas`.
Run `.cache/m52-release/judas projects/shooter_game/shooter_game.judasproj`.
Open the same project in the editor for Play/Stop or Export Project.
Exact unmodified exported package is at `/tmp/Judas_M52_Smooth_shooter/judas`.

[Project source tour and controls](../projects/shooter_game/README.md).
WASD/mouse: move/aim; LMB: semi-automatic fire; Space: script launch;
V: first/third person; R: real scene reload; Escape: pause. Logical controller
bindings are authored too; hardware acceptance remains the operator's job.

## Game architecture

All game-specific behaviour is project JS:

| Script | Responsibility |
|---|---|
| `player.js` | M35 input -> M49 motor velocity/launch; camera selection |
| `camera.js` | First-person eye and sphere-cast shoulder boom |
| `weapon.js` | Two ordinary M44 rays, impulse, audio/particles |
| `target.js` | Observe physical hinge; ready/cooling/rearm rules |
| `round.js` | Scene-local target registry, score/hits/shots/unique completion |
| `hud.js` | M41 HUD/pause policy; M43 reload and session best |
| `math.js` | Small project math helpers |

One player survives camera switching. Third person selects a point through the
reticle, then raycasts from the player's eye toward that point, checking cover
again. Camera parallax can change aim on switching. Tests include cover that the
camera can see beside but the player's shot cannot pass through.

Twelve plates use normal dynamic bodies (4 kg), M45 hinges, body-local X axes,
limits [-0.08, 1.2] rad, and implicit springs (65 N m/rad, 6 N m s/rad) at rest 0.
A world hinge anchor coincides with each visual base. Shots apply 8 N s at the
real query hit point. Gravity, inertia, contacts and the existing constraint solver
produce motion; target scripts never write plate transforms. A target scores once
while ready, then rearms after at least 0.55 s and near-rest angle/angular velocity.
Rows score 100/150/200 points. Hit all twelve once to clear the range; keep practising
or restart. Reload reconstructs player, targets, score, effects and UI; bounded
session best survives. The 10 kg crate reacts physically without scoring.

M41 authored anchors scale the HUD and pause panel. Modal pause stops fixed gameplay,
releases pointer capture and consumes menu input. Resume restores capture. Sounds
are original offline PCM assets; shot/impact effects are ordinary M37 bursts.
Primitive arena geometry and original textured plate meshes are normal assets.
First-person cosmetic avatar meshes are moved below the arena; they have no bodies.
The third-person mesh follows locomotion separately, without owning the motor.

## Generic API gaps found through play

The existing `PhysicsWorld::ApplyImpulseAtPoint` was not available to project JS.
M52 adds `Entity.applyImpulseAtPoint(impulse, point)` to the existing wrapper
and dispatcher, with normal safe entity/body validation and finite-vector checks.
Units: world-space N s and metres. No new solver, game component or game flag.
Types, reference, inventory, drift checker and an executed generic cookbook example
were updated together. No private native dispatcher is used by project scripts.

Human testing then exposed fixed-step camera/model jitter, including the earlier
M51 spacecraft. The renderer already interpolated motors and bodies, but JS read
only authoritative transforms, and the cosmetic avatar was copied before its
motor advanced. The repair exposes **readonly `Entity.presentedTransform`** in a
**`presentationUpdate(dt, alpha)`** callback after fixed simulation, before view,
audio and draw. Both camera and body-free avatar now consume the same existing
interpolation, including orientation. The current boundary demo's spacecraft
camera uses that phase too. Shooting/forces/motor intent retain authoritative
`transform`; neither physics nor fluid mechanics changed. This is generic pose
presentation, not shooter-specific smoothing.

Exact APIs used: `input.axis`, `input.pressed`, `input.pointerCapture`; entity
`transform`, `presentedTransform`, `valid`, `hasTag`, `character`, `angularVelocity`,
`applyImpulseAtPoint`, `burst`, `playAudio`; character `state`, `up`, `velocity`;
`world.entity`, `world.setView`, `world.clearView`; `physics.raycast`,
`physics.sphereCast`, `physics.joint`, joint `state`; `ui.get`,
`ui.debugOverlayVisible`, `ui.quit`, document `modal/get`, element `text/visible`;
`scenes.reload`, `session.get/set`; lifecycle start/update/fixedUpdate/presentationUpdate/uiUpdate/onUI/destroy.
No AI, navigation, weapon, damage, score or camera policy was added to C++.

## Validation and performance

One fresh Release build in `.cache/m52-release`, zero compiler warnings.
Focused game: **36 checks**, including actual round completion.
Normal asynchronous Application: **14 checks**, including GL world/UI screenshots,
input, both cameras, pause/resume and queued reload. Script regression **34**;
joint regression **19**. Surface/impulse/stale cookbook: **9 checks each**.
TypeScript 5.9.3 + live VM enumeration: **16 exports / 148 public symbols / 93
native operations**, three negative drift controls pass. Editor Play/Stop restores
an IDENTICAL authored scene. Normal M38 export and relocated startup from `/tmp`
pass: **15 assets, one scene, 7,529,005 bytes**, about **0.03 s** export.
The jitter follow-up adds **44 presentation checks**, after **36 baseline checks**
reproduced the old mismatch using preserved original scripts. Camera/model position
error in sampled moving frames went from up to 0.0625 m to below 0.000001 m;
spacecraft position and quaternion orientation match the rendered pose as well.
M51 boundary regression **26**, game **36**, application **14**, script **34**,
surface/presentation cookbook **9 each**, API drift, editor Play/Stop and both
moved shooter/flight package startups passed. No unrelated production suite rerun:
this adds a presentation phase/binding around existing interpolation, not a
physics-policy change. Original candidate and failure records remain preserved.

Desktop Release, normal measured frame clock, 210 frames after 30 warmup frames:
**58.39 FPS / 17.13 ms mean**, 19.01 ms p95, 21.74 ms maximum;
**0.458 ms mean authoritative fixed step**, 1.320 ms mean frame CPU,
0.312 ms mean render submission; **30 bodies / 12 target plates**.
Observed at most two fixed steps per measured frame. These are machine-specific
sanity measurements; script cost is included, not separately claimed.

[Evidence/results](evidence/m52/README.md), including real development failures and
narrow follow-ups. [Exact changed files](evidence/m52/CHANGED_FILES.txt) and
[current source fingerprints](evidence/m52/jitter-followup/SOURCE_SHA256.json).

## Limits and human checklist

Basic shoulder-camera sphere cast, without smoothing/full camera collision solving.
Semi-automatic hitscan; no recoil, ballistics, AI or enemies. Shared emitter voices
were simple retriggered effects at M52, not a polyphonic mixer. Its original
ASCII-only UI restriction was superseded by M58 Unicode/localization. M53 later
adds navigating enemies to Spring Range; this section records the M52 game scope.
Use current subsystem references for motor/query/joint and audio limits. Exports retain normal Linux system-library
requirements. Automated screenshots/state cannot establish game feel or sound.

1. Move/jump; aim and shoot in first person.
2. Watch plates physically swing back and spring upright; confirm scoring/rearming.
3. Shoot the crate: physical response, no score.
4. Switch to third person, aim with its reticle, shoot; switch back without state loss.
5. Hit several independent plates and clear all twelve.
6. Pause/resume; confirm capture and no shots through the menu.
7. Restart: round resets, session best survives.
8. Change `shotImpulse` or `speed` in JS, reload, and verify without C++ compilation.
9. Try controller bindings if hardware is available.
10. Test `/tmp/Judas_M52_Smooth_shooter/judas` equivalently, including listening.

Proposed checkpoint: `Add M52 JudasJS shooter game`. No tag.
