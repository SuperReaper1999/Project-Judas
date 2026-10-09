# Audio playback and acoustics

Current audio API (extended in M60). Judas owns playback, spatial processing and resource lifetime; projects own clips, groups and reverb settings; JavaScript decides when and why they play. See [lifecycle](lifecycle.md), [entities](entities.md) and [world streaming](streaming.md).

## Entity audio controls

An ordinary AudioEmitter refers to a stable audio asset ID. `entity.audio` returns a detached snapshot or `null` when that entity has no emitter. A stale entity handle throws `ReferenceError`, as with other entity operations.

| Member | Behaviour |
|---|---|
| `playAudio()` | Restart that emitter's voice at time zero; returns boolean. |
| `pauseAudio()` / `resumeAudio()` | Hold and resume the media cursor; boolean, false when unavailable. |
| `stopAudio()` | Stop and rewind, preserving the emitter binding. |
| `setAudioEnabled(boolean)` | Disable and release owned voices, or permit creation again. |
| `playAudioOneShot()` | Independent buffered non-looping voice at this emitter; returns false and drops the request when its group/master is paused. Throws for unavailable/loading data, streamed policy, disabled emitter or capacity exhaustion. |
| `seekAudio(seconds)` | Non-negative media seconds, before pitch/Doppler; false if voice absent or past known duration. |
| `setAudio(settings)` | Partial runtime settings below; boolean. Invalid values throw. Does not edit source assets. |
| `setAudioVelocity(vectorOrNull)` | Explicit world metres/second, or `null` for automatic estimation. |

`playAudio()` retains its original single-voice restart semantics. Use independent one-shots for overlapping shots, clicks or impacts: at most 32 one-shots per runtime world, 128 total backend voices, 32 concurrent streams and 64 combined live-stream/retirement slots. Full capacity fails explicitly rather than stealing another sound. Destruction, region removal and full Stop release the appropriate owned voices.

### Snapshot

`enabled`, `playing`, `requested`, `ready`, `streamed`, `loop`, `starved` are booleans. `state` is `loading`, `seeking`, `ready`, `playing`, `paused`, `ended`, `starved` or `failed`. `error` is a diagnostic string or `null`. Loading an unavailable asset does not crash the world.

`position` and `duration` are **media seconds**, with `null` for unavailable/unknown data. Pitch and Doppler change audible playback speed; they do not change the unit. `bufferBytes` counts that stream's four prepared PCM pages, `underruns` counts callback blocks requiring silence. `dopplerRatio`, `occlusionGain`, `cutoff` (Hz) and `distanceGain` distinguish independent spatial processing controls. Occlusion controls report the target; the DSP smooths toward it.

### Runtime settings

`loading`: `buffered` or `streamed`; `streamPageFrames`: integer 1024–16384 (default 4096). Four stereo float pages at 48000 Hz use `4 * frames * 2 * 4` bytes per voice, independent of track length. Decoder working memory and encoded OS caches are additional. Changing policy/page capacity replaces the voice. Buffered sounds decode the entire clip through ResourceManager; streamed sounds open/read/decode incrementally on the existing JobSystem. WAV, MP3 and FLAC use the pinned miniaudio decoders. Streamed filenames must have matching WAV/MP3/FLAC extensions. MP3 duration is null until a voice naturally decodes EOF (then shared metadata knows the length); prefill never invokes the pinned backend's potentially whole-file MP3 duration scan. Seek accuracy is decoder-dependent; sample-exact compressed seeking and universal gapless MP3 are not promised.

`loop`, `spatial`, `volume` (0–1), `pitch` (0.125–8), `referenceDistance` (>0 m), `maximumDistance` (>reference m), `rolloff` (non-negative), `attenuation` (`none`, `inverse`, `linear`) retain their authored meanings. `group` is an authored project group name; empty string or `master` routes directly to master.

`doppler` (0–4, default 0), `occlusion` (default false), `send` (0–1, default 0), `bypass` (default false), `occlusionLayers` (collision layer names, empty list blocks no layers), `occludedGain` (0–1, default .25) and `occludedCutoff` (40–24000 Hz, default 1200) are optional acoustics. Bypass removes obstruction gain/filter and environment send; ordinary distance attenuation still applies.

## Groups and fades

`audio.group(name)` returns `{gain, mute, paused}` or `null` for an unknown group. `audio.setGroup(name, {gain?, mute?, paused?}, fadeSeconds=0)` changes an existing group; returns true, null without an audio service, or throws for unknown group/invalid values. Groups are authored in Project settings, at most 16 plus `master`. Gain is 0–1; fades are 0–60 seconds and advance on the audio sample clock even when gameplay is paused.

```js
import {audio} from "judas";
audio.setGroup("effects", {paused: true});
audio.setGroup("music", {gain: 0.2}, 0.5);
// Project UI policy may leave music/menu sounds running.
```

Pause freezes that group's source cursors, distinct from muting (playback continues inaudibly). Master additionally affects every group. UI/pause gameplay policy belongs in JS, not in required engine category names. Group pause discards newly requested independent one-shots rather than accumulating events for resume; already playing persistent/held voices retain their cursor. Existing shared reverb tails can decay after an individual group stops sending; master mute/pause also silences those tails.

## Motion and coordinates

Spatial updates use the presented entity/camera pose once per frame, independently of the number of physics steps. Position, listener orientation and velocities use the same simulation world frame. Full listener orientation defines stereo axes; gravity is not audio up. M23 still uses a fixed double origin with local floats, without live rebasing.

The acoustic medium is stationary uniform air in that frame, sound speed 343.3 m/s. For the direction from source to listener, Judas uses `(c − listenerRadialVelocity)/(c − sourceRadialVelocity)`, with authored Doppler scale, subsonic terms bounded to ±.75c and ratio .5–2. Backend Doppler is disabled, so it is not doubled. Composed pitch is bounded to .125–8. This is a finite game approximation, not shock-wave acoustics.

Attached emitters/listeners use rigid-body point velocity including `angularVelocity × offset`; motors use actual velocity. Scripted motion/camera tracking uses frame finite differences unless explicitly supplied. Initial placement, first view selection/clear, frame gaps over .25 seconds and discontinuities greater than max(1 m, 60 m/s × frame duration) reset automatic estimates. Calling `setAudioVelocity(null)` restores that path. 2D voices bypass spatial Doppler and occlusion.

## Geometry obstruction

Resident PhysicsWorld geometry supplies at most eight rays every 50 ms, round-robin among audible playing spatial emitters opting in. Muted/paused/zero-gain routing and zero-volume emitters do not spend rays. The collision mask is independent of render layers. Source/listener bodies are ignored, sensors excluded, and a legitimate physics-only hit still obstructs. Skipped updates retain the previous observation. A moved wall/door changes the result through its collider.

A blocked ray targets authored gain and cutoff; a clear ray targets gain 1/full bandwidth. Gain and a real one-pole low-pass smooth over approximately 100 ms. Many sources increase observation latency. This approximates obstruction; it does not model diffraction, material transmission, thin/grazing geometry robustly or load missing world regions to trace a sound.

## Environmental reverb

An authored Audio environment component references `.judasreverb` settings: room size, damping, stereo width and wet level, each 0–1. Room size is the algorithm's feedback control, **not metres or a guaranteed decay time**. Oriented box/sphere zones have priority, interior blend distance, amount and enabled state. Highest priority wins; equal priority weights blend deterministically. Listener-weighted settings control one shared bounded Verblib processor; emitter `send` chooses participation. Outside zones is dry.

The dry signal is submitted once. Wet processing uses shared delay buffers and smoothed parameters; it is not rebuilt every frame. A source can stop while its tail decays. Removing a zone fades wet output; full world Stop/replacement clears DSP. One processor is allocated lazily when used and remains until full Stop. This is not portal-connected multi-room acoustics or convolution.

## Streaming clocks and lifetime

The callback consumes prepared PCM and atomics, never runs JS/physics/file IO/decoder work. One ordered job per stream fills bounded pages; stale seek generations are discarded. Temporary starvation emits zeros and **holds the media cursor**, increments underruns and remains distinct from EOF. Startup/seek readiness is not a guarantee against later starvation. Control notifications/snapshots are observed on the main thread, never dispatched to JS by the mixer.

Voice removal detaches from the mixer graph (the pinned backend can wait for its current graph read), then retires the decoder/pages asynchronously on Judas jobs. Region unload never waits for decoder cleanup. Process shutdown may drain jobs after voices detach. This is not a claim that the whole backend is lock-free.

Persistent-root voices retain one cursor across additive region trips. Ordinary suspended region emitters follow M59's restart-on-resume policy. Adopted emitters retain the existing voice; destroying their old region does not destroy them. World replacement/Play–Stop invalidates old entity handles and clears environment state. **Historical M60 persistence scope:** runtime decoder pointers, cursors and DSP tails were not save/fingerprint data. [M61 slots](saves.md) now preserve semantic persistent-voice cursors and control state using normal seek/prefill; decoder memory and DSP tails still clear. One-shot effects deliberately do not resume.

`audio.diagnostics` returns current voice/stream/PCM bytes, high-water PCM allocation, underruns, decoded frames, query count, pending retirements, processor count and maximum main-thread detach milliseconds (or null without audio service). These are workload diagnostics, not device latency or perceptual acceptance. [Executed example](examples/audio.js).

M65 creation failures distinguish invalid clip, invalid settings, missing project
audio group and exhausted buffered/streamed voice capacity. Capacity remains128
voices /32 active streams, with bounded retirements. One emitter/entity remains a
limit; these diagnostics do not add multi-emitter ownership.
