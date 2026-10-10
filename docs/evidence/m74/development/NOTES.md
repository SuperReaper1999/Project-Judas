# Genuine development observations

- Socket fixture omitted mandatory authored offset fields; corrected content.
- World manifest used a path where project settings require an asset ID; corrected content.
- M70 partial selection included its physical root; corrected content to leave it animation-owned.
- Empty/incomplete editor drafts passed through strict publication serialization and threw; fixed generic draft identity, kept strict publication validation.
- Export omitted the registered streaming scene; corrected the new project export scene list.
- Removing source provenance before export correctly fails freshness validation. The final target-only proof exports with provenance present, hides the authoring project during playback, and removes the cooked source rig as well.
- Source-free lab proof exposed a stale comparison-source handle; the lab now checks Entity.valid.
- Mask persistence test initially used source semantic names for deliberately opaque target Qxx names; corrected the fixture to full actual target keys. No engine change.
- A first autofocus patch declared its variable in the wrong helper and failed compilation; corrected scope and rebuilt. The early reused editor executable was not accepted as final evidence.
- Early performance capture exported before the readiness frame scopes closed; corrected capture boundary, separated loading/warmup from steady observations, and reran only the paired modes. Renderer counters are explicitly cumulative rather than per-frame.
- Original Blender fixture export completed successfully but its shutdown hung with a PulseAudio sandbox warning; only the owned process was interrupted. FBX and GLB then passed actual approved decoders and numerical checks. Blender does not perform retargeting.
- The planned Linux build found two pre-existing RuntimeWorld indentation warnings. RuntimeWorld was left unchanged. New Linux warnings in retarget/test code were corrected, with final affected build receipts clean. Native MSVC warnings are recorded separately below.
- Native Windows cooking initially failed in a deeply nested receipt directory. The inherited two-hash model cache plus staging suffix reached 261 characters; identical Win32 creation probes fail with error 3 there but succeed with an extended path or a short name in the same parent. The VM has LongPathsEnabled=0. The unchanged cook executable passes all 37 checks from a short test root. No path validation, solver or engine behavior was bypassed; the inherited long-path limitation remains documented.
- MSVC exposed float-literal and local-shadow warnings in the new tests. Test-only suffix/name corrections preserve their values; only those targets/checks were rebuilt. Inherited MSVC warnings and two new editor-autotest local-shadow warnings remain visible in the native build receipt; Windows is not represented as warning-free.

Other setup errors (existing-output authoring refusal, zero-length-helper fitter refusal, owned save-directory setup and an incorrect build-target name) were corrected without changing engine semantics. Protected historical failure records were not edited.
