# Scripting V2 qualification

Starting accepted source: `1094d64515604fe9d03fdab479efc7f880b112a8`.
Work belongs on Experimental; main promotion and human acceptance are separate.
No physics/fluid algorithms, games or independent research were modified.

## Architecture checkpoint

The original 3,270-line ScriptSystem combined the facade, dispatcher, metadata
and runtime. Subsystem native handlers and embedded facade fragments now own
those bindings; a validated catalogue supplies compilation and registration.
The coordinator and private runtime representation retain execution/lifecycle;
metadata has its own translation unit. Existing UI/joint/AI implementations are
reused behind adapters. Seven unused textual helper files were retired after
body/AST parity checks. There is one public module and one shared world session.

The clean archived baseline passed 291 checks across seven suites. The extracted
candidate passed 525 checks across 19 focused suites, including real loopback
networking. The initial restricted-environment networking failure is retained
locally; the same binary passes with local sockets available. No test was
weakened or skipped to turn that environment restriction into a pass.

The public wrapper comparison preserves 522 logical AST members/bodies, 45
exports/descriptors, 281 signatures, 357 native operations and 22 existing result
shape contracts. Prototype own-property order changes to subsystem grouping;
this is documented, not claimed identical. Metadata/coordinator token parity is
in `metadata-parity.json`. Source/API checks and eight configure-time catalogue
unittests include deliberately broken controls.

`architecture/RESULTS.json` records commands, binary hashes, emitted observations
and results. STATE/POSE observation digests match the accepted source; these
alone are not full trajectory evidence. Independent actual-body probes are
recorded separately below.

## Preserved negative controls

The baseline and extracted Audio tests both fail exactly one of eight checks:
`setAudio({volume:..., volumeDb:...})` silently ignores the foreign option and
publishes the volume change. Valid patches, invalid known fields, nested Camera
calls inside an Audio getter and teardown pass. The upcoming behavioral change
is intentionally separate from the architecture checkpoint.

Baseline provenance and reproduction are in `BASELINE.md`. Its source archive
and native build remain locally at `.cache/scripting-v2/baseline/`; SHA-256
`ceb60999dc78206525058778cc361fe7497a77ef20db1c76b7369b96cbd58793` identifies
the archive. Additional raw work, failed build attempts and reproduction helpers
are retained under `.cache/scripting-v2/` rather than replacing old evidence.

## Scope boundaries

These are native Linux automated results. They are not Windows/Android/browser
qualification, hardware graphics/audio certification or human editor acceptance.
The Alpha Advanced Fluid lock and main are unchanged. No release/version change
or converter is part of Scripting V2.

## Combined behavioral correction and final qualification

Architecture commit: `3522db113b8313a872de0768b4503179a0bad784`.
The following behavioral corrections are a separate checkpoint:

- Audio rejects unknown enumerable option names before publishing settings.
  `module-local-proof.json` proves that only the owning Audio native module
  changed during that repair: coordinator, private VM, metadata, registry and
  catalogue hashes stayed identical. The same public-path negative becomes
  **8/8 passing**, including actual emitter readback and a nested Camera call.
- Native handler exceptions cannot unwind through QuickJS's C boundary.
  Allocation failure becomes QuickJS OOM; other native failures become contextual
  InternalError. Existing JS exceptions retain their type/message. The original
  escaping-error probe and corrected output are preserved; five new registry
  checks cover catching the error and continuing in the same context.

The final combined candidate passed **538 checks across 20 native suites**;
`combined/RESULTS.json` records each executable hash and command. The live API
check also passes all 528 symbols / 357 registered operations / 281 signatures /
22 shapes / 59 examples, including eight drift negative controls. The catalogue
unittests pass 8/8, build-target tests 22/22 and validation-runner tests 15/15.
All three new registered CTest gates pass. No new compiler warnings were emitted
in the final combined build.

### Actual physical continuation

`trajectories/` retains native IEEE-754 pose/velocity words, AI state, script
snapshots and native Audio/Camera state for each fixed boundary. The same
observer source drives force/torque through real UtilityAgent → fixed scripts →
MK2 → save ordering. Contact and freeflight both match the accepted-source
counterpart **bit for bit** for uninterrupted, fresh-process restored and
impulse-negative trajectories (24 comparison groups). Both negative impulses
change native trajectories. These are actual body observations, not a cached
controller success flag. The freeflight cold continuation matches all 61
boundaries exactly.

A pre-existing limitation remains: in the accepted-source contact fixture,
restoration matches the saved boundary exactly, but the next contact step differs
slightly. First position.z difference is 4.47e-8 m; maximum subsequent position
separation is 1.49e-6 m, linear velocity 8.63e-5 m/s, angular velocity 1.86e-4 rad/s.
The refactor preserves each original trajectory exactly; it does not fix or
silently certify bitwise cold-contact equivalence. No engine physics was retuned.

### Existing editor and packaged SDK

The real editor opened a copied Developer Lab, exercised Play/Stop, undo and
save; authored state was identical afterward. Its ordinary export was copied
byte for byte and ran from an unrelated directory with pause, resume, reset and
scene reload. See `editor-export.json` and local screenshots/logs at
`.cache/scripting-v2/editor-export/`.

A fresh **17-tool Linux SDK** passed inventory checks, identity probes, project
creation, editor Play/Stop, script checks with a deliberately broken source,
import/cook/save/server operations, and both editor/CLI relocated exports.
SDK-only operations left the packaged inventory unchanged. See `sdk-*.json`.
This is host Linux packaging qualification, not a new Ubuntu compatibility or
clean-machine certification. Automated graphics used offscreen Mesa in restricted
runs and NVIDIA in native-access runs; none implies human visual/audio acceptance.

### Measured scripting cost

Five alternating baseline/candidate pairs, same host/compiler/Release settings,
no simultaneous builds. Median of the existing fixture's frame+fixed batch
measurements (microseconds):

| Scripts | Accepted source | V2 candidate |
|---|---:|---:|
| 0 | 0.007 | 0.007 |
| 10 | 16.009 | 12.431 |
| 100 | 179.388 | 146.262 |

`timings.json` retains every sampled measurement. This is a bounded scripting
microbenchmark, not an engine-wide FPS claim or worst-case guarantee.

### Reproduction and fingerprints

Build with native Release/Experimental configuration, then the suite targets in
`combined/RESULTS.json`; run their listed arguments. Set `SDL_AUDIODRIVER=dummy`
and `SDL_VIDEODRIVER=offscreen` for automated UI checks, and allow loopback sockets
for the real networking suite. Configure before building newly added targets;
a first local attempt accidentally ran an old Audio test binary after target
re-registration. Its timestamp/log is retained locally; explicitly relinking
produced the expected 8/8 pass. Acceptance uses the recorded final binary hashes.

`SOURCE_SHA256.json` fingerprints the implementation and validation wiring.
`combined/BUILD_INFO.json` retains actual tested provenance: architecture commit
plus the explicit behavioral working diff, with unrelated existing untracked
work reported honestly. This is not a clean release build. Compare hashes before
reusing binaries. The final checkpoint adds evidence/docs without changing that
validated source. The SDK can be regenerated with `judas_linux_sdk`.

For the portable observer, copy `reproduction/` into an owned temporary directory
then run `run_endpoint_probe.py --build <existing-Makefiles-build> --label <name>`
(with `--freeflight` for that control). It compiles the preserved observer against
that build's libraries, records every command, and starts distinct write/read
processes. `compare_endpoint_probes.py <baseline-result> <candidate-result>`
requires identical observer source and compares each trajectory separately.
The Node parity helpers additionally expect the original source restored at
`.cache/scripting-v2/baseline/source/` and their usual working location
`.cache/scripting-v2/evidence/`; API checking itself needs neither archive nor
those research helpers.

### Human review checklist

1. Open Developer Lab in `build/judas_editor`; create/attach a script and edit its
   declared properties. Confirm ordinary selection/undo/save behavior.
2. Play, use a breakpoint, step and inspect locals; fix a deliberate error and
   deliberately reload. Confirm source locations and retained JSON state.
3. Stop and confirm authored state is restored; reopen the saved project.
4. Run an exported existing game and confirm its controls, HUD and saving feel
   unchanged. SDK candidate: `build/linux-sdk/`.

Windows, Android and browser builds have not been requalified in this pass.
Human acceptance remains pending; main and the Advanced Fluid lock are unchanged.
