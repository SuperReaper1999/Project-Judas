# Runtime scene transitions (M43)

Current public JavaScript signatures, examples and lifetime rules: [JudasJS reference](JUDASJS.md). This document retains milestone architecture and evidence context.

Judas owns world lifetime. Project JavaScript chooses when to change scenes.

```js
import { scenes, session } from 'judas';
scenes.current;                    // "Scenes/a.judas"
scenes.registered;                 // sorted project-relative scene paths
session.set('hasKey', true);
scenes.load('Scenes/b.judas');     // queued, never destroys this callback
scenes.reload();                   // fresh authored current scene
session.get('hasKey');             // true after transition/reload
session.delete('hasKey');
```

## Registry and authoring

The existing project `scenes-dir` registers its saved `.judas` files recursively;
the configured startup scene is also registered. Paths are canonicalized inside
the project root; requests must exactly match a registered project-relative path.
Absolute paths, traversal and unknown names are rejected. The API is main/runtime-thread only. The registry is captured
at session start. Restart Play to discover newly added scene files. Project settings
shows the runtime scene list and existing startup-scene selection.
M38 already exports this same scene set and all registered assets. No new project,
scene or save schema is needed; packaging paths do not affect fingerprints.

## Lifetime and failure

The first valid request wins until the outer frame boundary. Later valid requests
in the same frame do not replace it. Unknown requests throw a useful JS error
without queuing. After simulation, callback delivery, UI and rendering finish,
Application and editor Play use the same `SceneSession::Apply` operation.

Loading/building and ordinary player construction are checked in an unstarted
candidate before ending the live scene. A malformed/unbuildable candidate reports
its error and leaves the live scene intact. On success, old scripts stop, scene UI
and audio stop/release, InteractivePlay detaches, and the old RuntimeWorld is destroyed.
The freshly constructed world begins normally. No additive worlds are simulated.
Requests during outgoing destruction callbacks are rejected. Force/physics order
and event generation are unchanged.

The project Reset action queues a real reload from the saved authored file; save
editor edits before testing reload. World entities, spawned prefabs,
script heaps/state, UI, audio, particles and contact history are fresh. Direct
non-project reference harnesses retain their existing manual reset operation.
Editor Stop discards the played world/session and keeps its original authored
document unchanged, even after playing other scenes.

## Session and saves

Session values are detached JSON copies, not live JS objects or entity handles.
Values support null, booleans, finite numbers, strings, arrays and plain objects.
M40 validation limits apply: depth 16, 4096 nodes, no accessors/cycles/functions;
the session is bounded to 256 keys and 64KiB total key/value bytes, key length 128.
Update nested data by getting a copy, editing it, and setting it again.

The store belongs to one project runtime/Play session and is destroyed on exit,
project change or Stop. Scene changes/reloads retain it. It is not automatically
saved, and scene-local script persistence never migrates to another scene.

Transitions load the authored baseline without automatically applying saved deltas
(reload must reset runtime state). Initial startup's existing save overlay is
unchanged. F6/F7 target the new current scene's save path; FTFT1 strict authored
baseline validation is unchanged. `JUDAS_WORLD_STATE=none` disables saves for the
whole standalone run; a custom initial override applies to initial startup only.

## Demo and scope

`projects/scene_demo/scene_demo.judasproj`: A has a green gate, B has a purple gate
and blue floor. E in A sends a prefab courier through the real sensor; its JS event
sets a session key and requests B. E in B returns to A. P spawns another courier;
R reloads. Escape opens authored UI with Return/Reload/Quit buttons. Session key
and visit count remain visible while scene entities reset.

Sensors use M42's discrete rigid-body overlap contract. This demo uses a normal
prefab rigid body, not the custom sweep-only player, as the trigger participant.
No continuous triggers, background streaming, loading screen, persistent entities,
additive scenes or cross-scene state migration are included. Scene loading is
synchronous at the safe frame boundary and may visibly pause for a large scene.
