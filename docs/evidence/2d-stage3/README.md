# Native 2D Stage 3 evidence

Linux Release source from Experimental parent
`9e3d528632bfd692539f7e4d613b1bd55d3b7700`, 2026-10-30.
Automated qualification is not operator gameplay acceptance or a v1.1.0 release.
Main remains `e2008d1e6f4ac3229b292724e5718f37e8d2e1ed`.

`RESULTS.json` records the final selected gates, commands, exact executable hashes,
source identities and outcomes. `SOURCE_SHA256SUMS` pins intended implementation,
tests, docs and project content (excluding evidence outputs/self). Precheckpoint
builds honestly identify the parent plus dirty/untracked fingerprints. Source
bytes do not acquire a later commit identity retrospectively. The local HANDOFF
also indexes the clean-commit binaries/package when refreshed after checkpointing.

Native content/timing/draft, runtime/JS/prefab/stream/cold and actual pixel tests
are distinct. Supplementary existing skeletal animation/event/root-motion/cold
and application regressions protect the shared traversal wrappers. Native planar,
sprite and MK2 checks protect previous stages; no full-suite or cross-platform
certification is claimed. New-format cold continuation is run in another process,
not only in a second object in one process. Actual physical target displacement,
marker counts, exact phase and framebuffer pixels are checked independently.

## Preserved failures and corrections

- `decimal-before.log`: the first sprite implementation reused float skeletal
  marker storage with double clip durations. A marker authored at 0.3 rounded
  beyond the clip boundary, and the new decimal gate failed. Native sprite event
  times are now double; shared typed traversal retains historical skeletal float
  storage and existing save layouts. No epsilon or acceptance-threshold change.
- `application-before.log`: full application save succeeded but load was rejected
  by SaveService's strict participant inventory. Direct participant tests alone
  had missed this. Sprite content is now explicitly accounted for, and transient
  excluded sprites do not manufacture required participants. Original failure
  and corrected application/relocated tours are retained.
- `integration-crash.log`: a new test supplied no JobSystem to WorldStreaming,
  whose constructor requires one. Debugger identified JobSystem::Submit through
  that constructor. The fixture now supplies the normal job owner; no unrelated
  streaming implementation change was made or claimed.
- The first workshop launch had an invalid hand-authored save identity. It was
  corrected to the required 64-character hexadecimal identity. Compile/setup
  errors are retained locally and not labelled accepted-engine regressions.

The independent deliberately malformed/removed-frame, stale-disk, changed-content,
event-overflow and missing-package controls reject rather than yielding partial
publication or silently substituting art. Existing physical expectations, solver
quality and protected evidence have not been weakened.

A supplemental old animation application run initially failed to create its save
root because this execution sandbox cannot write the normal home user-data tree.
The unchanged test passed with `XDG_DATA_HOME` pointing to its isolated writable
cache directory. Both logs are retained; no frame limit or acceptance condition
was changed. One negative-package setup tried a cross-filesystem hard link and
was rerun using a copy within `/tmp`; that is not an engine failure.

The real JS negative fixture (`tests/fixtures/2d-stage3/api-negative.js`) replaces
only the copied workshop's `Assets/game.js`. Run 120 fixed frames with the normal
runtime harness: it must exit zero and log `STAGE3_API_NEGATIVE_PASS rejects=7`.
The fixture proves catchable type errors, atomic rejected patches, detached
observations and explicit seek/pause/reverse behaviour in the actual VM.

## Reproduce

Configure Linux Release with validation enabled, then build the targets listed in
`RESULTS.json` plus `judas_editor`, `judas`, `judas_export`,
`judas_sprite_animation_benchmark` and the supplemental animation targets.

```sh
python3 scripts/prealpha_validate.py --build-dir build --output /tmp/FRESH-stage3 \
  --test sprite-animation --test sprite-animation-integration --test sprite-animation-render
build/judas_sprite_animation_integration_tests "$PWD/projects/sprite_animation_workshop" \
  /tmp/FRESH-stage3/sprite-animation-project --restore
node scripts/check_judasjs_api.mjs
build/judas_sprite_animation_benchmark \
  projects/sprite_animation_workshop/Sprite_Animation_Workshop.judasproj
```

The benchmark is CPU playback publication only: 30 warmups, 240 measured updates,
128/1024 bodyless sprites, off/on. It does not measure whole-frame GPU/physics cost
or claim a speedup over an older sprite animation system. Raw distributions and
RSS are included. Mesa llvmpipe application/pixel evidence is functional rendering
validation, not a hardware GPU benchmark. Windows/Android remain unqualified here.

Full local captures and package location:
`.cache/2d-stage3-evidence/HANDOFF.json`.
See [implementation and human checklist](../../2D_STAGE3.md).
