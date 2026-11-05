# Renderer V2 qualification

Implementation checkpoint: **fb4ef80f79aee1d8d3aad9137ee1c44508124ec4**,
Experimental, 5 November 2026. Main remains
`1094d64515604fe9d03fdab479efc7f880b112a8`.
This is automated development qualification, not human acceptance, publication,
Ubuntu 24.04 distribution requalification or a version/channel change.

The following qualification checkpoint carries this report. Its engine
implementation is identical to the tested implementation checkpoint. The
workshop uses T for watch/free camera so F5 remains the editor's Play/Stop
shortcut, with an additional free-camera smoke route. The final handoff records
refreshed tool provenance and fingerprints outside the tracked report.

## Results

- **32/32 focused native gates passed** against rebuilt, matching `fb4ef80f`
  provenance. `CHECKPOINT_GATES.json` preserves every invocation, binary hash,
  embedded identity, elapsed time and status; `native-logs/` preserves raw output.
- The original 31/32 and 28/32 attempts are preserved in `combined-qualified/`
  and `combined-gates/`, with their actual failing logs and identities. They are
  not counted as passes. Missing historical asset-cache fixtures and a relative
  source-path assumption were repaired without changing acceptance thresholds.
- The M55 material/optics fixture failed its two nonempty-path checks because
  it had never applied its project's explicitly enabled fluid policy. Applying
  that policy passes **36/36** assertions unchanged on Experimental. Alpha's
  runtime lock remains authoritative; the combined legacy-optics fixture is
  classified unavailable there. Separate ordinary-material/rendering/basic-water
  gates remain active. Python negative controls guard that distinction.
- The shadow/LOD negative reproduction fails **2/34** checks before correction;
  the corrected native predicate passes **34/34**. A last-drawn object's disabled
  LOD cannot suppress another camera's dependency on other LOD casters.
- `HDR_BASELINE_NEGATIVE.log` retains **12/29** failed checks against the frozen
  old engine. The scoped HDR continuation correction passes **29/29** independent
  framebuffer/colour controls, including same-size scratch reuse and eviction.
- Editor V2 viewport/detachment **87/87**, Scripting V2 module/registry **8/8 +
  20/20**, UI menu preview/transfer **10/10**, physics continuation **148/148**,
  origin assemblies **47/47**, networking **105/105**, fluid policy **26/26**
  and project settings **42/42** pass alongside the rendering owners. These
  scoped suites do not imply comprehensive engine certification.
- Python source/build registration, packaging, CI, scripting, editor architecture
  and current/release documentation checks pass. Two optional Ninja registration
  controls report skipped because Ninja is unavailable, not passed.
- All **17 packaged tools** agree on identity; SDK integrity and host inspection
  pass. Packaged CLI tools create/validate a fresh project and export it.
  A malformed script is rejected with its source location and the repaired
  script passes. Packaged Editor V2 creates a visible starter, exercises Play,
  resize/picking, unsaved-document handling and explicit export.
- The final checkpoint SDK exports a fresh MK2 scene/controller and the workshop.
  Their relocated runtimes have the exact SDK runtime bytes. A new process
  restores **gold 37 over constructor default 0**, with 13 save participants;
  packaged save inspection/check pass. Missing-save negative evidence remains
  in the earlier strong SDK qualification.
- The checkpoint packaged workshop completes **430 harness frames**, all six scenes,
  pause, live monitors, split views and save/load, with ten actual captures.
  This harness renders requested capture frames; it is a behavioural test,
  not a sustained game performance measurement. The subsequent T-key free-camera
  route also completes all 430 frames. Its normal live runtime renders
  every presentation frame. Editor Play/Stop leaves authored scene bytes
  **IDENTICAL**, and project source hashes were unchanged.
- A separately traced relocated run of the same rendering implementation reads
  **zero source/build/SDK paths** in 26,420 file-trace rows. Its earlier fixture
  had a weaker state oracle; the later strong zero-to-37 cold test is separate.
  We did not hide the source tree: unprivileged namespace creation was unavailable.
- Portable compiler and WebGL results are in the adjacent portability report:
  **12/12** changed translation units per Windows/Android/browser compiler,
  plus **33/33** actual WebGL2 owner checks. No native Windows or Android runtime
  qualification, full updated browser game or physical-device acceptance is claimed.

`ENGINE_SOURCES.json` and `BINARY_SHA256.json` identify the actual code and
binaries. `SDK_BUILD_INFO.json`, `SDK_MANIFEST.json`, `SDK_CHECKPOINT.json` and
`SDK_VERIFY.log` identify the exercised SDK and exports. Its source is tracked
clean; 18 pre-existing untracked build-input candidates are explicitly recorded
and preserved. It is a development package, not a clean release preset.

## Performance and ownership

See `../final-performance/README.md` for 20 matched ABBA runs, 50,000 samples,
28 pixel-identical captures, raw-file hashes and median/p95/worst costs. CPU
reductions are 19.7% mixed, 43.1% repeated, 32.8% sprites and 60.0% particles;
GPU-bound lighting improves 29.8%. These are targets achieved in representative
bottlenecked cases, not universal speedups. Memory and cold-start regressions are
reported. Matched timing runs have shadows disabled; shadow correctness is
qualified separately.

Renderer.cpp falls from 1,804 pre-renderer lines to 1,240, with actual shader,
submission, upload, particle, view, target, shadow-cache and compute ownership
separated into focused modules. That line count is a consequence, not an
acceptance oracle. The rejected multi-program specialization and its unexplained
intermediate failed observation remain archived; the production fix is a simpler
uniform guard around a redundant Legacy-light loop in the shared shader.

## Reproduction

Use a configured native Release build with the repository's qualified dependencies.
`SELECTION.json` lists the exact public/test build targets and 32 manifest names.
Build those targets with `cmake --build build -j2 --target ...`, then run:

```sh
python3 scripts/prealpha_validate.py --build-dir build --output /tmp/NEW-RENDERER-GATES \
  --test graphics-capabilities --test renderer-shader-library \
  --test renderer-v2-submission --test renderer-view-uniforms \
  --test renderer-submission-keys --test renderer-stream-arena \
  --test renderer-particle-pass --test renderer-shadow-reuse \
  --test renderer-compute --test renderer-compute-portable \
  --test renderer-pass-state --test rendering-architecture \
  --test render-cameras --test visual-particles --test asset-pipeline \
  --test asset-residency --test material-application --test shadow-sampling \
  --test editor-v2-viewports --test scripting-module-acceptance \
  --test scripting-registry --test sprite-render --test tilemap-render \
  --test sprite-animation-render --test weather-lighting --test ui-menu-workflow \
  --test project-fluid-policy --test project-settings-editor \
  --test physics-continuation --test origin-assembly --test network-session \
  --test beacon-grove-application
```

Output must be new. `qualify-final-sdk-original.py` is the exact historical
checkpoint runner, with its original fixed cache destinations; **do not rerun it
into retained evidence**. Use new output/SDK/temporary directories and the same
commands. Actual process arguments and relocation paths are in `SDK_CHECKPOINT.json`.
All raw timing CSVs, original failures, prototype sources/binaries and full local
build artifacts remain under `.cache/renderer-v2/`; this report does not duplicate
large binaries into Git.

The human checklist and remaining architectural/platform boundaries are in
`docs/RENDERER_V2.md`. No Vulkan backend, GPU particle/fluid simulation, Alpha
fluid unlock, release, main promotion or human acceptance is implied.
