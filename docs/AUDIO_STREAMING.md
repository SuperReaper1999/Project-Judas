# M60 — Streaming audio and environmental acoustics

Starting checkpoint: `277e67d2b0811644b2122bae27a4301d6dd3c96b`.
Candidate, uncommitted; auditory/gameplay acceptance belongs to the operator.

Judas owns playback, spatial/DSP primitives and resource lifetime. Projects own
sounds, routing and settings. JavaScript chooses their meaning and pause policy.

## Architecture and boundedness

One EngineHost AudioSystem/miniaudio 0.11.23 engine, device and selected listener.
Worlds bind ordinary emitter/listener/environment components; additive regions do
not own mixers. Renderer never owns audio. Optional settings default to buffered,
Doppler off, obstruction off and send zero, preserving legacy sound behaviour.

Buffered effects share immutable whole-clip PCM. Streamed WAV/MP3/FLAC use custom
prepared miniaudio data sources: four fixed stereo-float pages at 48 kHz, 1024–16384
frames each (32–512 KiB per voice, default 128 KiB). Metadata is shared, cursors and
ordered jobs independent. Stream duration does not grow PCM allocation. Decoder
working storage and OS caches are additional, separately from known owned bytes.

The existing JobSystem opens/seeks/decodes and uninitializes decoders. At most two
pages are filled per job. Main-thread pumping schedules one ordered job per stream;
stale seek generations cannot publish playback readiness. Starvation emits zero,
holds the media cursor and counts underrun blocks; EOF is distinct. Invalid files
fail explicitly. 128 voices, 32 streams, 64 combined stream/retirement admission
slots, and 32 independent buffered one-shots per world are bounded policies.

The audio callback reads prepared pages/control atomics and runs bounded DSP.
There are no Judas filesystem, decoder, JS, physics, allocation or job-wait calls
in that callback. Pinned miniaudio graph detach/uninit may wait for the current
mixer read; this is **not** a strict lock-free backend claim. Normal region removal
first detaches then submits decoder/page cleanup asynchronously. It never waits on
a decoder job. Shutdown drains detached streams before destroying the JobSystem.

M59's composed identity worker formerly read entire registered files with a
128 MiB limit. It now uses 64 KiB incremental SHA-256, producing the same digest,
with cooperative cancellation. This shared correction prevents long music being
whole-file-loaded by unrelated identity work. Canonical fingerprint schema remains
5; optional authored fields use existing tagged conditional-extension conventions.
The broad gate also exposed the accepted icon PNG's missing asset metadata. Its
bytes remain unchanged; an ordinary texture sidecar now registers it, and the
technology-project asset integrity test reflects that tenth asset.

## Motion, geometry and environments

Presented source/listener poses are published once per outer frame. Real bodies
supply linear plus angular point velocity; motors supply actual motion. Scripted
sources and active views use coherent frame finite differences; explicit source
velocity is available. Initialization, view clear/first selection, >.25 s gaps and
large discontinuities reset estimates. See the [API reference](judasjs/audio.md)
for threshold and finite approximation limits. Uniform stationary air is assumed
in the same fixed simulation frame, c=343.3 m/s. Backend Doppler is disabled;
Judas's projected source/listener ratio composes with authored pitch. Gravity is
not a stereo up vector. No live origin rebasing was added.

At most eight audible eligible source rays every 50 ms use resident authoritative
PhysicsWorld geometry, existing collision masks, source/listener body exclusion
and sensor exclusion. Physics-only hits count. A real one-pole low-pass and gain
smooth over ~100 ms. A shared observation also applies to independent one-shots
at that emitter. This is single-ray obstruction, not diffraction/transmission;
render visibility never gates queries or playback. Muted/paused/zero-gain routing
and zero-volume emitters do not spend obstruction queries.

`.judasreverb` contains room-size feedback, damping, width and wet level (0–1).
Oriented box/sphere zones blend inside their edges. Highest priority wins, equal
priority settings blend in stable source identity order. Pending selected settings
hold the previous environment until the complete selection is ready, rather than
using job completion order. One lazily allocated Verblib 0.5 processor shares all
sends; dry is submitted once. Parameters smooth without rebuilding delays. Tails
may decay after stopping a source; zone removal fades wet output. Full Stop clears
DSP and all voices. This is listener-weighted shared reverb, not portal acoustics.

Verblib is pinned verbatim to the miniaudio 0.11.23 extra; Judas selects its MIT-0
license. [Provenance](../third_party/verblib/PROVENANCE.md), license and exported
runtime notices accompany it. No backend upgrade or second worker pool.

## Authoring, scripts and lifetime

Project settings expose up to 16 authored groups plus master, gain/mute/pause and
sample-clock fades. Pause freezes existing source cursors and discards new independent
one-shot requests instead of queuing them for resume; mute leaves them advancing.
Master affects dry and existing tails once; group mute/pause prevents new sends
but existing shared tails may decay. Projects choose menu/music/effects policy.

Emitter inspector exposes loading/page capacity, group, send/bypass, Doppler and
obstruction controls. Add **Audio environment**, select a reverb asset, and author
shape/priority/blend. Asset Browser creates/edits reusable settings. Ordinary
scene properties preserve prefab overrides and stable IDs. Cursors/decoder state
are runtime only. Project group serialization uses invariant one-line data.

[Current public audio reference](judasjs/audio.md), [declarations](judas.d.ts),
[API inventory](judasjs/API_INVENTORY.md) and [executed example](judasjs/examples/audio.js)
cover `audio`, emitter snapshots/settings, seeks, explicit velocity and independent
one-shots. Stale entity handles use existing ReferenceError behaviour.

Persistent root music keeps one voice/cursor across region requests. Local streams,
queries and zones disappear on removal and retire safely. Adopted emitters keep
voice identity; cancellation cannot resurrect removed sources. M59 suspension
still restarts region sounds on reconstruction; decoder/DSP snapshot suspension
was not invented. Script destroy callbacks run while world audio is valid.

## Demonstrations and human listening checklist

Open `projects/audio_lab/audio_lab.judasproj`. WASD/mouse move/look, Esc controls,
E toggles the physical hinged door, P Doppler, B actual effect bypass, T tone on/off,
Space independent reverb impulse, R clean reload. The menu pauses/seeks music, toggles looping,
changes group gain and pauses effects independently. Initial levels are modest.
Pause music and stop the tone to isolate wall/reverb comparisons. Walk toward the
left doorway to compare closed/open geometry; blue floor is hall, orange damped
booth, outside dry.

`projects/streamed_range/streamed_range.judasproj` retains existing game/locale/
return-journey controls. Root music streams continuously, gallery ambience uses
resident wall geometry, contrasting gallery zones select environments, and the
ordinary carryable prop emits quiet sound. Shots/hits use independent buffered
voices. JS menu policy pauses effects while leaving music running.

1. Start music; pause/resume, seek, loop and adjust group gain.
2. Hear the moving tone approach/pass/recede; compare P Doppler off/on.
3. Walk behind the wall/door; open E and compare smooth obstruction.
4. Enter both coloured spaces, Space impulse, compare B dry/wet and tails.
5. Play across several galleries **and back**; music continues, local sounds retire,
   and carry Q a sound-emitting prop beyond its original region.
6. Pause, switch locale, resume, reload and Stop; no stuck controls/ghost sound.
7. Inspect diagnostics and repeat in moved standalone.

## Evidence and limits

Focused numerical, real PCM, lifecycle, project and package evidence is under
[docs/evidence/m60 (historical)](https://github.com/SuperReaper1999/Project-Judas/tree/2ee4203a0053cc545eab4a184c627f4f5507c4bd/docs/evidence/m60). Performance numbers there distinguish control,
offline DSP, rendered frame work and actual device operation, not output latency.
Headless waveform measurements and a brief device smoke establish mechanics;
they do not constitute human listening acceptance.

No universal gapless MP3 or sample-exact compressed seeking promise; decoder seeks have codec-dependent cost. Streamed MP3 deliberately avoids
whole-file length scans; duration is unknown until natural decoded EOF, while
WAV/FLAC use header lengths. No HRTF, multiple active listeners,
material acoustics, portal graph, microphone, interactive music framework or audio
VM callbacks. Abrupt control changes are smoothed where described, not sample-exact
AV/network synchronization. Thin/grazing geometry and discontinuities below the
motion threshold remain approximate. One shared environment tail cannot be muted
independently per originating group. Current platform remains Linux desktop.

## Focused checks from a fresh checkout

Build the normal Release targets. `tools/CreateAudioFixtures.py` uses local FFmpeg
only for generated test recordings under the ignored cache (not a game/runtime
requirement), then run:

```sh
python3 tools/CreateAudioFixtures.py .cache/m60-audio
./build/judas_audio_acoustics_tests .cache/m60-audio
./build/judas_audio_world_tests
DISPLAY=:0 ./build/judas_audio_application_tests projects/audio_lab/audio_lab.judasproj .cache/m60-app
```

The API checker accepts a local TypeScript installation through `--typescript`;
there is no permanent runtime/build dependency on Node/TypeScript. The once-only
candidate runner preserves its output and refuses to overwrite the broad gate.
See [final result adjudication (historical)](https://github.com/SuperReaper1999/Project-Judas/tree/2ee4203a0053cc545eab4a184c627f4f5507c4bd/docs/evidence/m60/RESULTS.md); original failures remain.
