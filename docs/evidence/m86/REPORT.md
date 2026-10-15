# M86 — Judas Learns to Weather

## Status

Linux-validated Experimental candidate based on
`7d1110ee323e3f7d6a7b5d84be338edf144546dc`. Human visual/interactive acceptance
is **pending**. Windows M86 validation was not performed and remains informational.
`main` was not changed. This is an approximate weather capability, not a climate
or atmospheric-fluid solver. No protected research or historical evidence changed.

The M86 focused checks pass. One existing atmospheric-flight assertion remains
failed; its source-isolation evidence and exact limitation are recorded below.
This report does not claim every historical suite passes.

## Deliverable and ownership

- Project: [`projects/weather_lab/weather_lab.judasproj`](../../../projects/weather_lab/weather_lab.judasproj).
- Controls/project layout: [Weather Lab README](../../../projects/weather_lab/README.md).
- Architecture/model/limits: [`docs/M86_WEATHER.md`](../../M86_WEATHER.md).
- Public API: [JudasJS weather](../../judasjs/weather.md), [`judas.d.ts`](../../judas.d.ts).
- Linux standalone: `/tmp/judas-m86-weather-lab/judas` (55,298,820-byte package,
  8 closure assets and 3 scenes). Run directly; it contains its runtime and project.
  This temporary build can be regenerated using the export command below.

`Weather.h/.cpp` owns pure binary64 anchored profiles, smooth transitions, bounded
gust/vortex samples and seeded strike history. `WorldWeather.cpp` resolves published
regions, applies opt-in drag through PhysicsWorld and schedules original procedural
thunder through AudioSystem. Fixed weather advancement follows project fixedUpdate
and precedes ordinary physics. No renderer owns the weather clock or physical intent.

Renderer owns all GL3.3 resources: mip-filtered 3D density, flat/radial layer marching,
sunlight/self-shadow approximation, inexpensive ground shadows, fog, precipitation,
wet/snow tint, lightning and moving funnel/debris. Default quarter-resolution clouds
use 28 steps; higher quality uses 44 at half resolution. Secondary views use 16 steps
at eighth resolution, without local precipitation or temporal history. Two scratch
sizes are cached with bounded LRU replacement; quality does not affect CPU forces.

Editor presets copy editable values rather than installing native weather modes.
Scene/prefab component descriptors, explicit full-save continuation and retained
streaming archive v8 use the normal runtime/resource/export paths. Weather owners
are generation-safe entities. World reset, destruction and region reconstruction
retire old fields, handles, pending audio and shelter state.

JavaScript chooses weather through `Entity.weather.transition/strike/enabled`,
reads state and spatial `weather.sample`, and selects visual `weather.quality`.
The lab's watch tour, movement, flight, UI, save requests and scene switches are
ordinary project JS. Flat weather up, gravity and support remain separate. Radial
weather is anchored to an explicit absolute sphere and survives M85 origin changes.

## Linux validation

Release CMake build and final relink passed. The combined build emitted two
pre-existing misleading-indentation warnings in RuntimeWorld material methods;
no new weather warning was observed. No full production-suite run was needed.

| Focused contract | Final result |
|---|---:|
| Core fields, interpolation, events, invariance and strict serialization | 67 / 67 |
| Scene/editor/prefab authoring and invalid descriptors | 30 / 30 |
| World forces, priority/fade, shelter, origin and lifecycle/reset | 50 / 50 |
| Retained streaming, unpublished state, rollback and cold continuation | 31 / 31 |
| Actual QuickJS bindings, negative phases/values, exact cookbook and cold resume | 30 / 30 |
| Actual GL output, oblique opaque depth, arbitrary-up/origin, cache and GPU queries | 39 / 39 |
| Shipping application controls, shelter, thunder, saves, planet travel and reload | 67 / 67 |
| CPU workload and exact visual-quality physical invariance | 28 / 28 |
| Save container, including 19/20/21 participants and rejected 22 | 36 / 36 |
| Existing render-camera / portal-camera regressions | 52 / 52; 21 / 21 |
| Actual VM enumeration plus TypeScript/drift verification | 385 symbols, 34 exports; pass |

The documentation checker also checks 268 native operations, 44 examples,
7 structured result shapes and 4 deliberate drift controls. This is API coverage,
not proof of all behavioural semantics. The weather cookbook executes in QuickJS.

Affected existing atmosphere, CharacterMotor, gravity-selection, M85 world/streaming,
origin-physics, rigid-contact, streaming, save/exclusion, audio, prefab and script
checks passed **except** the atmospheric-flight assertion below. Exact raw results
are in `final/headless-integration/`; final focused receipts follow the late narrow
render/save corrections in `final/headless-final/` and the dedicated final logs.

Editor copied-project smoke passed duplicate/undo, authored preview, Play/Stop,
exact authored-state restoration and scene save. The normal exporter initially
rejected the authored scene inclusion list because it omitted startup flat.judas;
the content/generator list now includes all three scenes. No exporter policy changed.
The relocated standalone ran from unrelated `/tmp`, using ordinary logical input:
all eight selections, pause/resume, durable save/load, flat/planet transitions,
natural F1 ascent to 4,000,000 m and F2 landing back on real collision. Exit was 0.

### Preserved unrelated failure

`judas_atmospheric_flight_tests` fails one assertion:
`manual torque builds a tumble that persists with SAS off in gas`.
All **79/79** repository dependencies across 26 compiled units match the starting
checkpoint byte-for-byte; target definition, physics source list, configuration,
generated icon and linked profiler/icon paths are unchanged. The target directly
compiles old physics and does not link Weather, WorldWeather, Renderer or judas_engine.

This is classified **inherited/source-isolated**, not passed or repaired by M86.
It is not a separately executed baseline comparison or a completed diagnosis.
The unchanged assertion tests angular-speed magnitude during anisotropic torque-free
rotation; its original physical contract requires separate review. See
`final/inherited-flight-failure/` and the original failing stdout. No threshold or
physics implementation was altered to obtain the weather results.

## Performance — actual development machine

Linux i7-4790, NVIDIA GTX 970 4 GB, driver 580.178.04. Native hardware identity is
recorded in `final/host.json`. Measurements were sequential; no build or VM workload
was run beside the accepted performance captures. These are measured workloads,
not a universal performance certification.

### Matched render workload

1280 × 720, same camera/lighting and 61 drawn boxes (not simulated bodies), 12 warmup
and 80 measured frames. CPU is renderer submission work; GPU uses completed M56
hardware timestamps. Swap wait is outside those intervals. One test-only finish
drains outstanding GPU queries rather than forcing synchronization every frame.

| Weather | CPU median / p95 ms | Whole-scene GPU median / p95 ms |
|---|---:|---:|
| Off | 0.505 / 0.581 | 0.547 / 0.641 |
| Sunny | 0.558 / 0.753 | 0.823 / 0.835 |
| Cloudy | 0.550 / 1.034 | 0.862 / 1.079 |
| Thunderstorm | 0.569 / 0.776 | 0.792 / 0.810 |
| Tornado | 0.566 / 0.673 | 0.808 / 0.888 |
| Blizzard | 0.562 / 0.640 | 0.739 / 0.756 |

Measured GPU increment is approximately 0.19–0.32 ms in this fixture. Cloud pass
medians are 0.156–0.234 ms and fog/composite 0.161–0.194 ms. Particle-pass times
are small in this view and are not a worst-case overdraw bound. `-1` in raw records
means that pass was absent, not negative elapsed time.

Off from a fresh renderer allocates zero weather textures. Enabled default primary
plus cached 64 × 64 secondary accounts for **4,464,713 bytes** of texture payload;
one primary is approximately 4.24 MiB. This excludes existing HDR buffers and driver
overhead. Process RSS observations were 87,838,720 bytes at off and 139,415,552 bytes
at the later blizzard capture. That whole-process increase also includes profiling,
driver and retained scene/cache allocations; it is **not** measured weather VRAM.
The weather payload estimate is the separate attributable resource accounting.

### Whole lab responsiveness

3,105 normally rendered application frames: actual frame-period median **17.109 ms**
(about **58.5 FPS**), p95 **17.956 ms**, maximum **293.093 ms**. There were 14 periods
over 50 ms during the sequence, which deliberately includes screenshots, saves,
reloads and cold planetary preparation. CPU frame median/p95 were 2.331/12.185 ms;
fixed-step p95/max were 2.131/26.966 ms, render CPU p95 9.401 ms. These mixed
lifecycle tails are reported rather than hidden behind the median; this does not
promise hitch-free asynchronous terrain preparation or disk capture.

The separate 64-dynamic-body wind benchmark uses 100 warmup + 700 measured steps
at unchanged 60 Hz. Final raw timings are in `final/weather-cpu-performance.log`.
With visuals off, the separate measured work was:

| Regions / 64 response bodies | Weather CPU median / p95 / max ms | Physics CPU p95 / max ms |
|---|---:|---:|
| 1, disabled | 0.011 / 0.013 / 0.025 | 0.034 / 0.073 |
| 1, enabled | 0.070 / 0.143 / 0.320 | 1.186 / 3.612 |
| 4, disabled | 0.017 / 0.028 / 0.043 | 0.044 / 0.068 |
| 4, enabled | 0.133 / 0.170 / 0.265 | 0.720 / 1.110 |

The same bodies physically move/wake under wind, so the extra ordinary physics
work is included separately rather than attributed to field sampling alone.
Graphics off/high give **exactly identical position/velocity endpoints**, and
the 64-body workload remains bounded to one force shelter cast per body per step.
The fields-only and full-force workloads are independent, not an exact subtraction
profiler. No rigid precipitation bodies, climate solve or quality-dependent forces.

## Remaining approximation limits

- Fixed repeating procedural cloud noise; no authored cloud-map resource/import
  format. Seed changes CPU gust/strike history, not GPU cloud noise. Orbital cover
  is deliberately coarser than ground density and can show coarse cloud gaps.
- One composed cloud frame/event identity per view; overlapping incompatible flat
  and radial regions should be authored coherently. No temporal accumulation.
- Shelter is a staggered 16 × 16 atlas over 32 m, taking 16 rendered frames to
  refresh. Small/fast overhangs can leak. Wind shelter is one short ray with 10%
  residual exposure; it is not indoor airflow. Material tint is not roof-masked.
- Prescribed vortex/centred drag are bounded approximations, without distributed
  lift, aerodynamic torque or finite-atmosphere momentum conservation. Visual
  debris is decorative; forces on opted-in ordinary bodies are real.
- Lightning uses reference ground, not terrain-aware electrical propagation.
  Thunder queues/voices are bounded; active sound waveforms are not saved, pending
  future events are. Follow-view listeners support origins; positional emitters
  and audio zones keep their existing restrictions.
- No flooding, snowpack, climate, storm-scale fluid solve or altered fluid physics.
- Linux rendering/behaviour is checked; Windows compilation/visual/audio behaviour
  and human appearance/usability acceptance remain outstanding.

## Reproduction

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel 4 --target judas judas_editor judas_export \
  judas_weather_tests judas_weather_authoring_tests judas_weather_world_tests \
  judas_weather_streaming_tests judas_weather_script_tests \
  judas_weather_render_tests judas_weather_application_tests \
  judas_weather_performance_tests
python3 scripts/m86_validation.py --output /tmp/new-m86-receipts
# In a desktop GL session, separately from builds/CPU benchmarks:
python3 scripts/m86_validation.py --output /tmp/new-m86-graphics --graphics
build/judas_export projects/weather_lab/weather_lab.judasproj /tmp/new-weather-package build/judas
```

`--regressions`, `--performance`, `--typescript PATH`, `--fingerprints-only` and
`--verify-fingerprints MANIFEST` are explicit runner options. Existing receipt
files are preserved with new run suffixes. Verification never replaces expected
hashes. The final source/binary manifest and changed-file inventory accompany
this report; evidence has a separate SHA-256 inventory to avoid self-hashing.

## One human checklist

1. Launch the project or standalone. Select 1–8; inspect clouds from below/inside,
   sunny lighting, rain, storm/lightning, tornado, snow, blizzard and fog.
2. F5 watch tour; check transitions and then interrupt using a direct selection.
3. Walk under/out of the pavilion; inspect shelter, rain/snow and approximate tint.
4. Listen for thunder after lightning. Inspect the moving funnel and light/heavy props.
5. Toggle Q/0 while props move: quality/visual changes must leave wind physics intact.
6. F9 planet, F1 ascend, F2 land, F3 face boundary. Look for seams/origin jumps,
   thin-layer depth bands, local-up errors and stable return to real ground collision.
7. F6 save mid-transition/storm, change weather and F7 load; test pause and R reload.
8. Edit a profile/region/response in the inspector, Play/Stop and try a fresh export.
9. Repeat the controls in the moved standalone with sound enabled; report appearance,
   audio, responsiveness and any stalls. Human acceptance remains authoritative.
