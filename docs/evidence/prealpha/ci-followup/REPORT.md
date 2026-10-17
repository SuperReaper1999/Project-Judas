# Clean-checkout resource validation follow-up

Engine/tooling checkpoint: `683cc3ddd81743febd15845e487de1aa7140556d`.
This follow-up changes only a test and its current documentation. Production
source, public APIs, project assets and physics are unchanged from that checkpoint.

## Original failure and cause

[The actual Ubuntu 24.04 CI run](https://github.com/SuperReaper1999/Project-Judas/actions/runs/39073963008)
built the engine, focused checks and complete SDK successfully. Tooling positive
and negative controls passed. Of 23 CTest entries, 22 passed; resource handoff
failed with `unknown asset id d15b628722311f90a9146c4d51e12ce2`.

The test loaded the ordinary import-lab scene, which references an approximately
137 MB cooked model excluded by that project's existing `.gitignore`. The
original FBX inputs are also excluded. These files existed on the development
machine, so the earlier local consumer test passed, but were absent in the real
clean checkout. Mesa/llvmpipe had created a working GL context: this was a hidden
fixture prerequisite, not an upload failure or missing graphics driver.

The original run status, all available per-gate results and exact failure log are
preserved under `original-ci/`. The preceding successful local original-consumer
test remains in the first checkpoint's evidence. Neither record is rewritten.

## Correction and stronger controls

The default test now creates a deterministic normalized `.judasmodel` inside its
fresh, test-owned output directory, using production `EncodeModelArchive`:

- 109 joints, eight influences per vertex and one animation clip.
- Eight indexed material parts, 3,488 vertices and 5,232 indices.
- Forty actual embedded 512 × 512 RGBA images, totaling 40 MiB of raw pixels.
- Archive SHA-256:
  `f6cf05cf94dcffa0b6cf19f3939c01b2c4b13194c5383e4144e0a28aa2a31737`.

The test uses ordinary asset metadata, decoding, real renderer allocations and
owner-thread GL uploads. It checks CPU-only private preparation, complete atomic
publication, bounded admission, worker/owner separation and actual retirement.
Cancellation requires both an allocated mesh and an allocated embedded map;
`CpuReady` alone is no longer accepted as evidence of GPU cancellation.

After CPU preparation, a test-only observer pauses once for at least 5 ms after
the first real map allocation. The next normal 2 ms upload-budget check therefore
yields on fast hardware too. The observer does not reenter or modify the engine.
The pause and its actual duration are recorded separately. These timings are
functional diagnostics, **not ordinary upload-performance measurements**.

`--original` retains the original full-quality consumer path. Its missing model
fails with an actionable diagnostic; it never silently substitutes the generated
fixture. No ignored model, FBX input or generated binary asset is committed.
Tracked-only source archives likewise do not promise to contain separately
supplied, ignored consumer assets.

## Verification and provenance

Final generated-fixture runs pass **14 checks** on both the GTX 970 desktop driver
and Mesa llvmpipe. Independently generated archives have identical hashes. An
unknown asset fails without a visible placeholder. An isolated copy of the
tracked original project without its ignored model must fail explicitly.

Build commands, identities, raw logs, fixture descriptions, counters and profiler
exports are preserved alongside this report. The initial sandbox-denied desktop
attempt is retained and classified as an environment failure. Review additionally
checked that cancellation is deterministic on faster GPUs and callback lifetimes
retain the owner context.

The SDK, signed Android APK and relocated standalone already produced for the
first checkpoint continue to identify **683cc3dd**, their actual build source.
A later test/documentation checkpoint does not relabel those binaries. Final
post-checkpoint receipts and any new CI run are reported separately in
`.cache/prealpha/ARTIFACTS.json`; pending CI is not a passing result.

The combined input inventory is in `FINAL_INPUTS.json`. The original checkpoint's
input and physics-evidence manifests remain immutable historical receipts. Main
remains at `9cf491a13dbb1cd82ab9ab9b916e255af2d579d2`; human acceptance is pending.

No M100/Alpha work, protected history, independent fluid research or game changes
are part of this correction. The unresolved CCD, dense-contact, platform and
composition limitations in [the engineering report](../REPORT.md) remain open.
