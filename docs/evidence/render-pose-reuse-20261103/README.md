# Reuse presented poses across world render passes

Experimental checkpoint based on 58450228528231ce093d650a3a5e05fb613a01fe.
Static and dynamic draw loops reuse their transforms and skin palettes within
one presentation sample. The arrays follow the authored render list and native
body slots. They expire at the end of the frame. Each camera and shadow pass
still controls its own visibility, LOD and render state. Camera textures and
material bindings still resolve in the pass that consumes them, retaining
current resource demand and recency semantics.

Linux Release, Ryzen 5 5600, SDL offscreen, Mesa llvmpipe with two workers.
Hardware GPU access was unavailable. Matched Rendering Lab captures ran
before, after, after, before. Each drew 20 realtime frames. The table uses
the last 16 frames per run and sums the indicated CPU scope across passes.
Software rasterization can contribute to CPU scopes. No dropped or truncated
records were observed in the Rendering Lab captures.

| Gallery metric (ms) | Before, two runs | After, two runs |
|---|---:|---:|
| Opaque submission median | 16.76 / 16.45 | 15.71 / 15.51 |
| Opaque submission worst | 27.95 / 26.70 | 26.96 / 26.95 |
| World render submission median | 581.86 / 574.19 | 570.87 / 574.87 |
| World render submission worst | 744.85 / 740.75 | 736.56 / 775.63 |
| First world render submission | 1797.25 / 1803.38 | 1804.22 / 1796.01 |

The first-frame and tail results show no repeatable improvement. In a separate
24-frame Lunar Island opening capture, median world submission after four
frames was 125.74 to 119.30 ms and worst was 128.69 to 130.36 ms. Both Lunar
captures truncated 153 profiler records. The opener uses the harness's blocking
resource admission. These captures cannot establish the cost of async gameplay
uploads or hardware rendering.

A separate fixed-clock capture drew frame four and produced byte-identical
640×360 PNGs. The renderer controls passed 54/54, the rendering architecture
suite passed 44/44 and the animated model runtime suite passed 23/23.

## Reproduce

From the checkout root, use a Release standalone built from the baseline and
one built from the candidate. Use the same copy of the Rendering Lab project
and fresh output/data roots per run.

```bash
e=docs/evidence/render-pose-reuse-20261103
out=$(mktemp -d)
env -u DISPLAY -u WAYLAND_DISPLAY SDL_VIDEODRIVER=offscreen SDL_AUDIODRIVER=dummy \
  LIBGL_ALWAYS_SOFTWARE=1 LP_NUM_THREADS=2 JUDAS_ENGINE_ROOT="$PWD" \
  XDG_DATA_HOME="$out/data" XDG_CONFIG_HOME="$out/config" XDG_CACHE_HOME="$out/cache" \
  JUDAS_PROFILE=1 JUDAS_PROFILE_OUTPUT="$out/profile.json" JUDAS_WINDOW_SIZE=640x360 \
  JUDAS_TEST_SCRIPT="$PWD/$e/live.route.test" \
  /path/to/judas projects/rendering_lab/rendering_lab.judasproj
```

For pixel comparison use `FRAMES 24`, `FRAME_SECONDS 0.016666667`,
`LOG_EVERY 0`, `EXPECT_VIEWPORT 640 360 5` and a `SCREENSHOT 4` path in
each run's separate output directory. This fixed harness draws screenshot
frames only. The live route draws every frame.
