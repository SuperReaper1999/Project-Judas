# Streaming audio and environmental acoustics

The engine owns playback, spatial/DSP primitives and resource lifetime.
Projects own sounds/routing/settings; scripts choose their meaning and pause policy.
Physical output and backend qualification are separate from dummy-audio tests.

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

region's composed identity worker formerly read entire registered files with a
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
voice identity; cancellation cannot resurrect removed sources. region suspension
still restarts region sounds on reconstruction; decoder/DSP snapshot suspension
was not invented. Script destroy callbacks run while world audio is valid.
