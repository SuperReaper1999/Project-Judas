# Lunar Island — local SFX/TTS pass

Base: Experimental `bd075ec137c5f5b2a5e9f1d05020e0d8838ab229`.
Date: 2026-10-26. Linux. Human listening/mix/voice acceptance pending.
No engine, external tool, research, historical evidence or main changes.

## Implemented

- Inspected the actual Linux tool, installed Python/Kokoro environment, cached
  voices/model and procedural renderer at SFX_TTS_Tool commit
  `077c37fa2a5fb5c3929a0d29e266b659588cdb65`. The tool checkout has local
  README/installer edits; these are preserved, and actual synthesis source
  hashes/status are recorded in TOOL_PROVENANCE.json.
- Eight replacement effects: gathering, chopping, crafting, consumption,
  footsteps, gunshot, reload and hurt. Existing gameplay hooks/asset IDs remain.
  The vehicle engine loop remains unchanged.
- All 24 existing topic lines for the eight friendly survivors now have local
  synthetic speech. Two installed British stock voices are reused, with Mara /
  Ivo name prefixes omitted from speech but retained in subtitles.
- Ordinary public audio emitters and project JavaScript handle playback. Topic
  selection plays once; changing topic, entering trade, closing or destroying
  the HUD stops the line. Loading/failed audio retains text, with bounded errors.
- Saving during a conversation silences transient speech without replay on the
  next HUD refresh. Fresh-process loading closes transient UI and preserves
  gameplay progress without restarting old conversation audio.
- Editable recipes, full text/voice/model/output fingerprints and an offline
  regeneration tool are preserved in ArtSource/audio and tools/generate_audio.py.
  Runtime/export requires only PCM, not Python, model weights or the authoring tool.

## Validation

- Native Release consumer observer: audio **30 checks / 0 failures**. Actual
  ordinary interaction/UI starts Mara and Ivo, changes topic, enters trade,
  closes, saves during conversation, and inspects native playback state. The
  device-free native mixer produces nonzero PCM while gameplay is paused.
- Fresh-process observer: **12 checks / 0 failures**. Exact inventory restored,
  all 24 native conversation voices stopped, and a new conversation plays again.
- Pure dialogue presentation policy: **11 / 0**, including resource-not-ready,
  failed/invalid handles, backend exception, topic interruption, no render-loop
  restart and save-time silence.
- Existing inventory **83 checks / 0** and encounter rules **36 / 0**. The
  encounter module has one further independent regional-quest assertion after
  its printed 36-check summary; it also passes.
- Public script preflight: **32 modules / 0 compile/link failures**.
- All **33 WAVs** (including unchanged vehicle loop): mono 16-bit PCM, nonzero,
  finite, no full-scale samples. Speech is 24 kHz; SFX is 44.1 kHz. Runtime audio
  files total **6,724,188 bytes**; speech totals **135.5 seconds**. These are file
  sizes/durations, not a claim of measured runtime RAM or listening quality.
- Eight saved SFX recipes replay byte-identically without altering assets.
- 24/24 dialogue text, output hash, stable emitter and metadata checks pass.
- Final native export: **202 assets, 26 scenes, 248,281,048 bytes, 7.503 s**.
  Copied to `/tmp/Lunar Island Audio Relocated`, launched with **cwd /tmp**,
  cold-loaded its isolated slot and completed 240 harness frames with exit 0.
  No source-tree assets or authoring tool are supplied to the package.
- GPU application checks: GTX 970 / NVIDIA 580.178.04. No performance benchmark
  or general platform requalification is claimed. Device-free mixing validates
  signal/playback, not speakers, pronunciation, perceptual loudness or voice taste.

## Preserved development failures

- `authoring-required-fields-failure.log`: initial new emitters omitted required
  attenuation fields. Fixed the authored data/generator; strict parser unchanged.
- `software-renderer-attempt.log`: restricted launch selected llvmpipe; aborted
  only our test process and repeated with the normal desktop GPU. Not a pass.
- `early-phoneme-observation-failure.log`: the first signal assertion ran before
  speech began (first >100-PCM-unit sample is at 0.2714 s). The native cursor
  advanced correctly. Final observation waits past the first phoneme, still
  requiring actual nonzero mixed audio; the original failure remains recorded.
  That diagnostic run also lacked its new mode's explicit completion; the final
  observer now quits at successful test completion.

## Boundaries and handoff

This is an audio/content pass, not an engine feature or later game stage. No lip
sync, bespoke voice actor performances, voice-volume slider, dynamic quest-result
narration or field-recorded ambience is claimed. Text remains the gameplay source
of truth. Clips are non-spatial conversation presentation, buffered in the root
region (24 extra emitter voices; the native total remains under 40 in this test).
No generated voice/model package is shipped, just generated speech WAVs.

Authored content changes update native compatibility fingerprints. Existing
operator saves have not been read, overwritten or deleted. Earlier-build saves
may be rejected by existing compatibility policy; this pass does not add an
unreviewed migration. All test saves use `/tmp/lunar-audio-final-data`.

Editable project: `projects/lunar_island/lunar_island.judasproj`.
Final standalone: `.cache/lunar-island-audio-final/Lunar Island/`.
Regeneration/attribution: `ArtSource/audio/README.md` and manifests.

### Human checklist

1. Launch the final package and listen to movement/gathering/tool feedback.
2. Talk to Mara and Ivo with E; select each topic and judge voice/pronunciation.
3. Switch topics rapidly, open trade, then close; former speech must stop.
4. Save during a spoken topic, quit, load; progress stays, old speech stays silent.
5. Visit other friendly survivors and compare their speech with their subtitles.
6. Judge gunshot/reload/hurt balance through the existing master-volume controls.

Main promotion and human audio acceptance remain the operator's decision.
