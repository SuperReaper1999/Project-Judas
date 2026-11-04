# Renderer V2 — Experimental

Development baseline: `d7bee296e86dc3f01e401d69512e0c1522aeb525`.
Main remains at `1094d64515604fe9d03fdab479efc7f880b112a8` pending human acceptance.
This work is the renderer portion of the proposed Alpha v2.0.0; it is not a
release or a Vulkan backend.

## Approved scope

1. Preserve matched baselines; introduce capability negotiation and coherent
   renderer ownership modules with portable correctness controls.
2. Improve submission, typed material batching, GPU uploads and resource lifetime.
3. Modernize shaders and view/pass efficiency without reducing lighting quality.
4. Establish bounded optional compute/storage/indirect infrastructure for future
   effects, while keeping gameplay physics and the Advanced Fluid lock unchanged.
5. Qualify the editable lab, consumer/editor journeys, exports and platform paths.

## Foundation checkpoint

Desktop requests OpenGL 4.6, then 4.5, then portable 3.3. `JUDAS_GL_PROFILE` accepts
exactly `auto`, `portable` or `modern`. Forced modern reports failure rather than
silently selecting portable. Android keeps GLES 3.0; browser keeps WebGL 2 and its
bounded outstanding-frame fence. Context capability reports distinguish hardware
support from the selected policy and validate entry points and limits.

`render/GraphicsCapabilities` owns discovery/policy/diagnostics, and
`render/ShaderLibrary` owns canonical shader sources, profile preparation,
compile/link lifetime and source-labelled errors. The eight extracted canonical
shader strings and existing ES adaptation were independently checked against the
baseline. Modern GLSL 450 initially changes the version header only.

Native foundation checks pass: 40 pure and 52 native capability checks, 47 shader
library checks, 16 new submission controls and 44 existing rendering architecture
checks. The initial architecture invocation reused its old default fixture,
failed script registration and then dereferenced the missing VM. A new disposable
output path passes; that first log remains preserved and is not an engine pass.

The same new benchmark compiled with the baseline's frozen committed headers and
engine/GLAD archives provides the comparison. Its first final-drain attempt
missed the last timestamp because the profiler intentionally excludes current-
frame queries. Advancing an unmeasured drain frame fixes the observer; the failed
89/90-sample run is preserved. Final short baseline rows have all GPU samples.

At 1080p on Ryzen 5 5600 / GTX 970, GCC 15.2 Release, 120 measured frames:

| Workload | Baseline CPU median / p95 ms | Baseline GPU median / p95 ms |
|---|---:|---:|
| Mixed PBR, 2048 objects, 1 view, 8 lights | 2.223 / 2.694 | 3.059 / 3.470 |
| Repeated, 4096 objects, 2 views | 3.677 / 3.779 | 3.762 / 3.927 |
| Ordered sprites, 4096, 1 view | 19.586 / 20.114 | 19.690 / 20.230 |
| General particles, 100000, 2 views | 28.037 / 34.712 | 28.301 / 34.824 |
| Overlapping lights, 256 objects, 128 lights | 0.775 / 1.032 | 21.035 / 21.197 |

GPU intervals include submission bubbles; sprite/particle GPU intervals are not
independent proof of shader cost. The light case has a materially different
CPU/GPU balance. These short runs select investigation targets, not final speedup
claims. Foundation mixed/light captures are pixel-identical to baseline, with
broadly unchanged costs. Raw commands, sources, hardware output, unsuccessful
observations and CSVs remain in `.cache/renderer-v2/`.

Performance changes and the final qualification results follow in later verified
Experimental checkpoints. No universal frame-rate or all-platform claim follows
from these measurements.
