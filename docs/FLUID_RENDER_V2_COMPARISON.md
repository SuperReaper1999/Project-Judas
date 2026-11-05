# Advanced fluids with Renderer/Particles V2 — 5 November 2026

Follow-up: the [stream retirement correction](RENDERER_V2_UPLOAD_HITCH.md) resolves
the upload-path hitch below. This report and its raw measurements remain the
unchanged before-fix comparison.

## Verdict

Typical GPU rendering cost is essentially unchanged in the original flat and
radial fluid demos. There is no substantial fluid-rendering speedup to claim.
CPU draw submission is small; its observed differences are hundredths of a
millisecond and overlap run-to-run variation. Surface reconstruction/upload and
the unchanged CPU fluid step still dominate expensive frames.

**A real V2 upload-path tail regression was found.** The radial case repeatedly
produces occasional 1–2 ms main-camera GPU intervals with default persistent
uploads. The old renderer does not show those spikes in this experiment.
Two V2 controls disabling only persistent uploads remove them, while retaining
the modern renderer and identical physical state/geometry/pixels. This implicates
that path; it does not establish the exact driver/coherency/fence mechanism.
No production upload default was changed or fix claimed.

This is a measured comparison, not fluid-unlock, stability, release or human
acceptance evidence. Alpha's normal lock and all simulation settings are unchanged.

## What actually renders the water

- `FluidWorld` particles are authoritative CPU water samples, not visual emitters.
- `UpdateFluidSurface` in `src/WorldPresentation.cpp` reconstructs an isosurface
  on executed fluid revisions, then uploads it through `UpdateMeshVertices`.
- The retained mesh uses the **current Renderer V2** geometry submission, shader,
  view-uniform, lighting and shadow-receiving paths. Runs explicitly selected
  `JUDAS_GL_PROFILE=modern`; all V2 processes confirmed the modern tier.
- The original surface upload remains an owned, retained VBO updated with
  `glBufferData`. It is not a Particles V2 billboard or GPU simulation buffer.
  Retention is necessary between authored 20 Hz fluid updates; drawing a held
  mesh is not an accidental fallback to Renderer V1.
- These demos contain no visual-particle emitters. V2 reported zero GPU visual
  particle draws. Their Particles V2 simulation/collision infrastructure is
  therefore not exercised, and cannot be credited with accelerating SPH or
  meshing. Replacing water samples with GPU visual particles would change
  simulation/ownership semantics, not merely select a rendering backend.

GPU splash/foam/deposition effects alongside water are a separate integration
case; this experiment does not claim that coverage or introduce such effects.

## Matched experiment

Native Linux, Ryzen 5 5600, GTX 970 4 GiB, NVIDIA 580.178.04, 1280×720.
The restricted runner initially exposed Mesa software GL; those checks were
excluded from performance data. All recorded runs used the actual NVIDIA GPU.
No VM, software renderer, driver setting change, concurrent benchmark or GPU
clock lock was used. Clocks/temperature/utilisation were recorded per process.

The frozen pre-Renderer-V2 checkpoint is
`d7bee296e86dc3f01e401d69512e0c1522aeb525`. The current production parent is
`3ffe83ae9f5efe2669995e8a3ee36dbc7ef56bc7`, with only the diagnostic probe/CMake
addition in its measured dirty build. Each probe was compiled from the same
source against its revision's own headers and frozen/current engine library.
Exact binary build identities, hashes, link commands and probe-source hash are
retained in [the evidence](evidence/fluid-render-v2/PROVENANCE.json).
Fluid, physics, coupling, fixed-step and profiler sources were compared and
unchanged. The baseline archive was read only; missing source-header dependencies
were resolved in a separate disposable copy, without editing the archive.

Fixtures are isolated copies of `projects/fluid_demo`:

1. Original rectangular pool.
2. Original radial planet scene.
3. Pool with only `linear-rendering true` and exposure 1 added, exercising HDR.

Each retains 380 water samples and 16 physical bodies. Each process advances
360 ordinary 1/60-second frames; first 60 are warm-up, last 300 measured.
This matches physical work, **not real-time FPS/catch-up behaviour**. Each case
used before/V2/V2/before order; the radial tail received another such block.
Two additional radial V2 processes used `JUDAS_PERSISTENT_UPLOADS=0` as a
diagnostic control. No default was disabled in the principal comparisons.

There are 18 processes, 6,480 recorded physical frames and 5,400 post-warm-up
samples. Every process agreed with its case's reference at every recorded
frame: water positions, previous positions, velocities, accelerations/masses,
and all rigid-body poses and linear/angular velocities. Final GPU-read-back
surface position/normal streams and PNGs are bit-identical, including controls.
Every measured main-camera GPU query completed; no GPU samples were dropped.

## Results

Times are milliseconds. The first table uses the original three ABBA blocks
(600 post-warm-up observations per renderer per case).

| Scene | CPU scene submission median, before → V2 | Main-camera GPU median, before → V2 | Whole frame CPU mean, before → V2 | Whole frame CPU p95, before → V2 |
|---|---:|---:|---:|---:|
| Pool | 0.162 → 0.127 | 0.689 → 0.688 | 5.830 → 5.406 | 17.783 → 15.283 |
| Radial | 0.174 → 0.172 | 0.378 → 0.376 | 6.538 → 6.531 | 19.808 → 20.059 |
| Pool HDR | 0.199 → 0.190 | 0.742 → 0.738 | 5.525 → 5.562 | 15.820 → 15.854 |

GPU medians differ by less than 1%. Do not attribute the larger apparent pool
whole-frame difference to rendering: the identical fluid step and meshing code
also varied in CPU timing. The radial CPU draw medians varied roughly
0.15–0.21 ms between individual processes, swamping its tiny paired difference.

V2 CPU surface reconstruction/upload costs about 5.58 ms per changed pool
surface, 6.03 ms radial and 5.68 ms HDR pool (medians). Those values include point
collection, CPU field/triangle construction and driver upload; they do not
separately measure pure meshing versus upload. Executed particle steps remain
about 9–10 ms. Do not add every GPU/CPU interval as though they cannot overlap.
Water updates at the original 20 Hz; a held surface can look less smooth than
the display frame rate even when GPU drawing is inexpensive.

### Radial tail: average performance hides a regression

This includes both radial ABBA blocks (1,200 observations per normal renderer)
and the two diagnostic processes (600 observations).

| Path | GPU mean | Median | p95 | p99 | Worst | Samples over 1 ms |
|---|---:|---:|---:|---:|---:|---:|
| Pre-V2 | 0.378 | 0.378 | 0.381 | 0.387 | 0.390 | 0/1200 |
| V2 default | 0.408 | 0.376 | 0.390 | 1.169 | 1.920 | 29/1200 |
| V2 persistent-off control | 0.375 | 0.375 | 0.378 | 0.384 | 0.388 | 0/600 |

Default V2 has 60/1200 samples over 0.5 ms; these concentrate in opaque-instance
GPU intervals in either the shadow or colour pass. They are not evidence of
extra fluid work. GPU timestamp intervals can include submission bubbles;
they are not independent fragment-shader cycle counts. The precise persistent
page-turnover/coherency/driver cause remains open. Both controls keep the modern
shader/view path, all original objects, the complete fluid state and all pixels.

The worst whole-frame CPU observations were 22.12 → 22.78 ms pool,
21.74 → 26.09 ms radial and 17.29 → 18.71 ms HDR in the original blocks.
CPU tails also vary on the shared desktop; these are observations, not claims
of a bounded worst-case guarantee. All mean/median/p95/worst distributions,
including surface updates, fluid steps and individual processes, remain in
[SUMMARY.json](evidence/fluid-render-v2/SUMMARY.json).

## Reproduction and retained evidence

- Current target: `cmake --build build -j2 --target judas_fluid_render_probe`.
- Run `JUDAS_PROFILE=1 JUDAS_GL_PROFILE=modern build/judas_fluid_render_probe PROJECT NEW_OUTPUT 360`.
  This host-specific probe explicitly requires the GTX 970 and never silently
  substitutes software rendering. It is an optional developer tool, not an SDK
  runtime requirement or a portable-platform acceptance gate.
- Baseline compile arguments and the frozen source/library paths are in
  `docs/evidence/fluid-render-v2/baseline-compile-qualified.json`.
- ABBA drivers and raw compressed frame/step/GPU rows are in
  `docs/evidence/fluid-render-v2/`; gzip contents are ordinary CSV.
- Complete native logs, profiler exports, initial failed harness/dependency
  attempts and disposable source/dependency environment remain in
  `.cache/fluid-v2-render-audit/` (about 121 MiB before final packaging).
  The initial pilot missed observers because Application reinstalled its own;
  all measured runs use the observer reattached at `beforeFrame`, and all 360
  steps/process were required. Pilot data are not performance acceptance data.
- Original projects, research, archived Renderer V1 evidence and the RPG SDK
  selection were not changed. Nothing is promoted or published by this audit.

## Next work

1. Investigate/fix persistent-upload tail behaviour for small retained scene
   batches, retaining proper fences, ownership and the large-workload benefits.
   Do not globally disable V2 or ship a scene-specific exception from the control.
2. Split and profile CPU surface construction versus dynamic VBO publication
   before choosing a GPU surface representation. This is the useful remaining
   presentation target; visual-emitter acceleration is not a water-mesher fix.
3. Continue the independently tracked fluid correctness/settling qualification
   before any lock removal. None of those limits is resolved by these timings.
