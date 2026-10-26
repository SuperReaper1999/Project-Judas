# Lunar Island generated audio

The procedural effects and dialogue are generated locally with the operator's
`SFX_TTS_Tool`, inspected at commit `077c37fa2a5fb5c3929a0d29e266b659588cdb65`.
Source: `/home/conner/Documents/GitHub/SFX_TTS_Tool/`.
The tool remains separate and unchanged; no generator, model or Python dependency
is needed in the exported game.

- Eight SFX recipes (`*.sfx.json`) are editable in the tool's existing designer.
  Existing asset IDs and gameplay calls are retained. The vehicle-engine loop is
  unchanged. These are synthetic, designed effects, not field recordings.
- `dialogue_manifest.json` preserves the exact text, subtitles, speaker, voice,
  pace, duration, output hash, model revision/weight hash and voice-file hashes.
- All 24 existing friendly-survivor topic lines are voiced. Quest acceptance,
  rewards, dynamically computed progress and trade descriptions remain text.
- Two installed Kokoro voices are deliberately reused: British `bf_emma` and
  `bm_george`. They are stock synthetic voices, not performances by the named NPCs
  or imitations of real people. Mara and Ivo's name prefixes are subtitle labels
  and are not spoken.

## Regenerate

Install the SFX tool's documented Linux dependencies outside Judas first. For the
existing local installation, run from the Judas root:

```sh
PYTHONDONTWRITEBYTECODE=1 \
PYTHONPATH=/home/conner/Documents/GitHub/SFX_TTS_Tool/.local-tts \
/home/conner/Documents/GitHub/SFX_TTS_Tool/.python312/bin/python3 \
 projects/lunar_island/tools/generate_audio.py \
 --tool /home/conner/Documents/GitHub/SFX_TTS_Tool \
 --revision f3ff3571791e39611d31c381e3a41a3af07b4987
```

The batch uses the tool's SFX renderer and installed Kokoro pipeline. It loads the
cached speech model once on CPU with two threads. It does not install/download
packages or run a remote service. `--sfx-only` needs NumPy/Tk but not Kokoro.
Run audio authoring **after** any earlier stage scene-authoring scripts; those
scripts intentionally preserve their original scene generations and would remove
new voice emitters if rerun. The audio authoring step adds them back. Review its
scene diff before committing. Output hashes describe these exact generated files;
bit-identical inference across different PyTorch/CPU versions is not promised.

`Assets/scripts/voice-data.js` maps stable NPC keys and topic indices to ordinary
root-scene audio emitters. `dialogue-voice.js` is project-only presentation policy:
one voice at a time, native resource-ready checks, bounded diagnostics, no automatic
replay every render, and immediate stop on leaving/changing conversations. Speech
is non-spatial so the paused dialogue remains legible. It follows the existing
master volume. There is no lip sync or separate voice-volume preference yet.
All voice clips total only a few MB; they are buffered with the root region.

## Provenance and licences

SFX recipes are original adaptations of the operator-owned MIT-licensed SFX tool's
built-in presets. No external recordings were downloaded. See `sfx_manifest.json`
for tool version, preset and audio hashes. The game source and authored dialogue
remain under the project's MIT terms and Conner / SuperReaper1999 attribution.

Kokoro-82M's official model card identifies its weights as Apache-2.0:
https://huggingface.co/hexgrad/Kokoro-82M (checked 2026-10-26).
Model hash: `496dba118d1a58f5f3db2efc88dbdc216e0483fc89fe6e47ee1f2c53f18ad1e4`.
The installed tool's `THIRD_PARTY_NOTICES.md` also records Kokoro and its dependency
licences. Only generated PCM is shipped here, not weights, voice packs, Python,
PyTorch, NumPy or their code. Do not copy model/dependency directories into the game.

## Saves and acceptance

This changes authored content and therefore its native compatibility fingerprint.
Prior-build saves are preserved and may be rejected as incompatible by the existing
save policy. No migration, deletion or invented compatibility is supplied by this
audio pass. Validation uses isolated test save roots, not the operator's saves.
Human listening, pronunciation and mix approval remain outstanding.
