# Focused reproduction

From the Judas repository root, with an existing native Release build:

```sh
node projects/lunar_island/tests/dialogue-voice.test.js
node projects/lunar_island/tests/inventory.test.js
node projects/lunar_island/tests/encounters.test.js
build/judas_script_check projects/lunar_island/lunar_island.judasproj
python3 projects/lunar_island/tests/build_observer.py --output /tmp/lunar-audio-observer
SDL_AUDIODRIVER=dummy XDG_DATA_HOME=/tmp/lunar-audio-repro-data \
 /tmp/lunar-audio-observer projects/lunar_island/lunar_island.judasproj audio /tmp/lunar-audio-repro-proof
SDL_AUDIODRIVER=dummy XDG_DATA_HOME=/tmp/lunar-audio-repro-data \
 /tmp/lunar-audio-observer projects/lunar_island/lunar_island.judasproj audioread /tmp/lunar-audio-repro-proof
python3 projects/lunar_island/tools/package_game.py --output '/tmp/Lunar Island Audio Repro'
```

Use the ordinary desktop GPU for the observer; it opens a hidden rendering
window, supplies logical input through the existing native test seam, and mixes
PCM without a physical audio device. No gameplay inventory/quest grants or direct
native play commands are injected for its audio scenario. The original observer's
common setup still exercises actual movement, third-person toggle and pause.

For relocated testing, copy the generated package to a new directory and launch
its `judas game.judasproj` from /tmp with `JUDAS_TEST_SCRIPT` pointing to
`relocated.harness`. Use the same isolated XDG data root as its saved slot.
Never point this harness at operator save directories. Generation reproduction
and exact model revision are documented in `../../ArtSource/audio/README.md`.
