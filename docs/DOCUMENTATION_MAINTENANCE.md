# Maintaining current documentation

[Current status and finding register](PREALPHA.md) · [Architecture](ARCHITECTURE.md) ·
[JudasJS](JUDASJS.md) · [Build identity](BUILD_RELEASES.md)

Current guidance must describe actual source/runtime contracts. A milestone's
implementation date or a passing demonstration does not prove every API,
integration or platform. Historical reports and failed experiments retain their
exact checkpoint-time claims; put a subsequent correction in a current reference
and link the new evidence instead of rewriting those receipts.

## Change checklist

1. Trace the affected runtime registration, owner and implementation. Check
   arguments, results, errors, units, coordinate frames and lifecycle validity.
2. Update the subsystem reference and any current summaries in README,
   Architecture, onboarding and platform guides. Search for the superseded name
   or limitation in current documentation, including examples and editor labels.
3. For public JudasJS changes, update `docs/judas.d.ts`,
   [inventory and manifest](judasjs/API_INVENTORY.md), reference and relevant
   executable examples together. Use the documented [API checks](JUDASJS.md)
   and meaningful behavioural regressions; a symbol check cannot prove semantics.
4. Keep support claims scoped: Linux host acceptance, native Windows functional
   checks, software graphics, Android compile/emulator/device and human testing
   are separate evidence. Preserve unperformed checks and known failures.
5. Link exact reproducible evidence for corrected findings. Update the current
   disposition register; retain original fixtures, thresholds and failures. A
   report under `docs/evidence/` is not implicitly current guidance.
6. Check local links and copyable commands from their stated working directory.
   SDK/source archives deliberately exclude games, research and historical
   evidence; label links requiring the matching source checkout. Do not infer
   that an SDK includes every linked file or regression fixture.
7. Finish tracked source/docs edits before configuring final-provenance binaries
   and running guarded validation. Documentation changes contribute to source
   identity; stale-binary refusal is not a passing test. Record actual tested
   inputs and binary hashes without relabelling older artifacts.
8. Record human acceptance separately and only when the operator grants it for
   the exact candidate. Push development to Experimental; main promotion follows
   [AGENTS.md](../AGENTS.md), not documentation wording or automated passes.

## Focused current-reference check

From the repository root:

```sh
python3 -m unittest discover -s tests -p test_current_docs.py -v
python3 scripts/check_current_docs.py
```

The explicit [current-reference inventory](../tools/verification/current-docs.json)
selects maintained developer pages. The checker validates local file destinations
and Markdown heading links; it excludes historical archives/evidence from its
page selection. Update the inventory when adding a maintained reference. It does
not inspect external URLs or certify prose, API semantics or platform support.

CI in a complete Git checkout also uses
`python3 scripts/check_current_docs.py --require-tracked` to reject links relying
on untracked local files. Run that mode after intended files are tracked; an
engine-only source archive deliberately omits some linked games and evidence and
cannot supply a complete-checkout link qualification.

## Boundaries

Do not sanitize archived logs, regenerate expected hashes to hide drift, delete
known failing tests, or turn a local ignored continuity note into a tracked
artifact. API drift checks, link checks and compilation are useful focused
controls; none establishes complete prose correctness, physics correctness,
hardware qualification or Alpha readiness. These still require source review,
behavioural evidence and the recorded human review process.

## Release status and distribution examples

Do not copy a "current main" commit into evergreen onboarding prose: it becomes
stale at the next promotion. Exact identities belong in build manifests and
checkpoint-specific evidence; direct users to `--build-info-json` or Git for the
checkout they actually have. Separate commands for a full Git checkout, a verified
engine source archive and an unpacked SDK. SDK/archive profiles intentionally omit
repository games, and SDK executables live at its root rather than under `build/`.
Local-link checks do not prove these commands work; qualify the actual moved SDK
and create/export a new project before claiming that workflow passed.
