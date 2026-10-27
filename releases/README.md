# Judas release metadata

No published channel is changed by this directory or its build scripts.
See [versioning](../docs/VERSIONING.md), [publication](../docs/RELEASING.md) and
[Alpha notes](../docs/ALPHA_V1_0_0_NOTES.md). A candidate is not a published release.

`release-manifest.json` schema 1 describes channel, SemVer, exact source commit,
compatibility protocol/major, notes and immutable artifact URLs, byte sizes and
SHA-256 values. The updater accepts only the engine repository's versioned GitHub
release URLs. `tests/fixtures/releases/update-cases.json` is deliberately synthetic
positive/negative test data and must never be uploaded as a real channel manifest.

The `update-manifests` GitHub release is the future authoritative metadata host.
It has one channel JSON asset per deliberately published channel. Stable remains
absent until a genuine GA release exists. Initial provisioning and all publishing
are withheld pending explicit final operator approval.
