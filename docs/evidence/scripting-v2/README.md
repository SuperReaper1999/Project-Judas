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
