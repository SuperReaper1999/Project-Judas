# 2D genre checks before v1.1.0

Experimental follow-up to 3e49c7ae. This is focused Linux qualification, not a
published release, a platform certification or human acceptance of new examples.

The actual native eight-box stack settles to ordered heights; raw coordinates
are in planar-gameplay.log. A separate upper platform supports its own falling
body. Sprite-render tests include framebuffer Y-sort crossing and order reversal.
The native runtime executes the shipped helper modules: thrust/front-target and
side-target exclusion, multi-target swipe identities, per-call deduplication,
wall occlusion and invalid-plane rejection. Presentation checks verify exact
parallax axis factors and repeatability. Existing Beacon Grove, sprite animation
and tilemap application tests exercise real game/animation/scene/save paths.

The melee helper is a bounded sampled fan of shape casts, NOT a continuous swept
rotating blade. Active-marker integration is illustrated in the guide; this pass
has not delivered or human-tested a complete sword-animation asset pack. Existing
animation integration independently tests marker-timed physical action. Parallax
is a single-camera script example with constant actor-follow offset, not native
per-view parallax, texture repetition, scrolling tile residency or live rebase
support. The fixture checks real runtime transforms, not framebuffer parallax.

Six source-matched suites pass in RESULTS.json. API/type checks pass with 58 typed
examples. CMake organization:18 passes; gate registration:6 passes,2 unavailable
configuration-generator skips. No test expectations or physics quality changed.
Initial helper draft had missing JSDoc/nullability annotations; api-initial.log
preserves the failed type check. Types and an explicit uninitialized-anchor error
were added; no type checking was disabled. Initial formatting warnings were
corrected without changing physical algorithms. No engine implementation changed.

Reproduction from repo root, using fresh output:

```sh
cmake -S . -B build-2d -DCMAKE_BUILD_TYPE=Release -DJUDAS_BUILD_CHANNEL=Experimental
cmake --build build-2d --target judas_2d_helpers_tests judas_planar_gameplay_tests judas_sprite_render_tests judas_beacon_grove_application_tests judas_sprite_animation_integration_tests judas_tilemap_application_tests --parallel 2
python3 scripts/prealpha_validate.py --build-dir build-2d --test 2d-helpers --test planar-gameplay --test sprite-render --test beacon-grove-application --test sprite-animation-integration --test tilemap-application --output /tmp/2d-genres-fresh --expected-channel Experimental
node scripts/check_judasjs_api.mjs
```

Remaining scope decisions are in docs/judasjs/2d-genres.md. In particular no
one-way platform, grid-RPG controller/framework, multi-level planar navigation,
exact multi-hit overlap query or Doom port is silently claimed. Windows/Android,
SDK/release packaging and human workflow acceptance are separate qualification.
No promotion, tag, version change or publication performed.
