# M34 — Authored engine audio

**Status through M69:** M34 is operator-accepted and checkpointed. Candidate/pending
statements and measurements below record the original milestone review state,
not a current outstanding acceptance gate. Later contracts take precedence;
start with [Architecture](ARCHITECTURE.md) and [JudasJS](JUDASJS.md).

**Human listening validation accepted by the operator on 2026-09-30.** Automated
checks do not establish perceptual quality, clicks, natural volume or stereo realism.

## Ownership and backend

Judas `AudioSystem` encapsulates vendored **miniaudio 0.11.23**, used under its MIT-0 (MIT No Attribution)
license (`third_party/miniaudio/LICENSE`; upstream also offers public-domain terms).
Copyright 2025 David Reid. The full upstream license and header notices are
preserved; MIT-0 does not impose an attribution requirement.
[Upstream](https://github.com/mackron/miniaudio/tree/0.11.23) and
[manual](https://miniaud.io/docs/manual/index.html). No backend objects appear in
scene, gameplay or editor APIs. Renderer owns no audio objects.

EngineHost owns the service; it opens the default output device lazily at the first
voice. Unused scenes start no audio device/thread. AudioSystem owns clip handles,
voices, listener and backend lifetime. RuntimeWorld owns its emitter bindings and
releases its voices on Stop, scene destruction or emitter destruction/disable.
Project handoff stops service voices before dropping old resources. Shutdown stops
voices, drains resources/jobs, closes audio, then destroys renderer/window services.
Worlds must be torn down before their borrowed host/resource services, as before.

Loaded PCM is immutable and shared. Each voice owns its backend cursor and retains
PCM until released, even if the resource entry is invalidated. Clip handles/voice
IDs are never reused during the service lifetime. Runtime state does not enter
scene serialization or saved deltas.

## Assets and loading

Import **WAV, MP3 or FLAC** in the ordinary asset browser. Audio has normal stable
`.judasmeta` identity (`type audio`); rename/move preserves scene references.
Emitters select an asset ID, never a filesystem path. ResourceManager coalesces
requests, reads/decode on its existing workers, and installs decoded clips during
its normal main-thread Pump. Existing cancellation/generation/demand semantics
apply. The decoder call is indivisible. Missing/corrupt assets expose errors;
correct the file and invalidate/retry through the existing resource path.

Whole clips decode to float PCM at 48 kHz; mono/stereo supported. There is no music
streaming or OGG decoder in this milestone. A decoded clip has a 512 MiB safety
limit; long music should await streaming support. Residency counts decoded clips;
a voice can retain old PCM after explicit resource invalidation until that voice
ends. Resource entries pinned by a world cannot be evicted during Play.

## Authoring and runtime controls

Create **Audio emitter** or **Audio listener** in World → Create, or add components
through the inspector registry. All settings participate in undo/redo and save/load.

Emitter: audio asset, enabled, play-on-start, loop, spatial, volume [0,1], pitch
[0.125,8], reference distance > 0, maximum distance > reference, nonnegative
rolloff, attenuation (none/inverse/linear). These are explicit validation ranges,
not post-update clamps. Source position follows the presented runtime entity pose;
static physics-body motion is also read from its actual current transform.

Listener: enabled and optional **Follow active camera view**. Without that flag,
its entity transform supplies position and full orientation (local -Z forward,
+Y up). With the flag, it uses the active view matrix, not PlayerController or an
offscreen camera. Gravity is irrelevant to listener orientation. More than one
enabled authored listener is a configuration error. Spatial voices are suppressed
and reported if there is no active listener; non-spatial voices remain usable.

`RuntimeWorld::PlayAudio/StopAudio/PauseAudio/ResumeAudio/SetAudioEnabled` control
an authored emitter by stable entity ID. AudioSystem also exposes reusable clip,
voice and auto-retired one-shot APIs for game code. Play restarts a clip; Stop
rewinds; Pause/Resume retains playback. Authored one-shot voices remain in Finished
state until stopped/replayed or their world ends; the service's standalone
`PlayOneShot` voices auto-release after completion. Reenabled/reconstructed emitters
restart when play-on-start was requested. Voice state is not save-game state.
Runtime-created body definitions retain their existing body/render/celestial-only
contract; use the service voice API for transient game sounds.

Only the ordinary InteractivePlay frame updates audio, once per rendered frame.
It observes the same world; secondary M33 cameras do not move the listener or tick
audio. Old scripted blocking screenshot harnesses are not audio playback evidence.
Edit mode plays no runtime sounds. Play constructs runtime state; Stop releases it
without modifying authored data. There is no editor clip-preview tool in M34.

## Spatial and non-spatial scope

Miniaudio performs stereo spatialization and attenuation; Doppler is disabled.
Non-spatial voices bypass spatialization and ignore transform changes. Inverse and
linear attenuation clamp distance at the reference/maximum limits; maximum is not
a hard source-culling radius. A linear curve with rolloff 1 reaches zero at maximum.
No occlusion, reverb, HRTF claims, acoustics, microphones or streaming are supplied.
`None` disables distance falloff while retaining panning, using the backend inverse
model with zero rolloff; its native `none` model also bypasses spatial panning.
The caller must mix reasonable voice volumes; no universal real-time guarantee is
claimed. These properties still require operator listening on an actual device.

## Compatibility

Authored scene grammar stays version 3, adding `audio-emitter`, `audio.*`,
`audio-listener`, `listener.*`. At M34, canonical fingerprint **schema 4** included every
authored audio property. M36 superseded it with current **schema 5**. The original
strict FTFT1 compatibility rejected older schema-3 saves;
existing authored scenes remain readable. No silent migration/deletion occurs.

## Demo and human listening checklist

Open `assets/scenes/audio.judas` in the editor and Play, or run
`./build/judas assets/scenes/audio.judas`. Red box = looping spatial source; green
sphere = one-shot; the centered MP3 is non-spatial. WASD/mouse moves the listener.
The original CC0 tone assets are offline-authored demonstration content, not a
runtime synthesis capability. Stop/restart Play to repeat the one-shots.

1. Can you hear the looping source?
2. Does it pan left/right as you move around it?
3. Does it get quieter with distance?
4. Does the one-shot play once?
5. Does the 2D sound stay centered/non-spatial?
6. Does stopping Play stop runtime sounds?

See `docs/evidence/m34/RESULTS.md` for executed automated results and lightweight
CPU measurements. Device-free tests run the real backend mixer/voices but neither
record audio nor assert how it sounds.

Exact cursor snapshots are exposed only in device-free tests. Production does not
read the backend's non-atomic PCM cursor during device mixing.
