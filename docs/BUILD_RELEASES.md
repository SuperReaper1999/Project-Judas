# Build identity and release infrastructure

[Versioning/channels](VERSIONING.md) · [Prepare/publish](RELEASING.md) ·
[Distribution verification](DISTRIBUTION.md) · [Support matrix](ALPHA_SUPPORT_MATRIX.md)

`engine-version.json` is authoritative. CMake emits one build identity header and
JSON record. Editor, runtime, server and public SDK tools expose `--version` and
`--build-info-json` before initializing windows/resources. SDK `BUILD_INFO.json`,
export `BUILD_PROVENANCE.json` and JudasJS `engine.buildInfo` preserve that actual
identity rather than relabelling a neighboring binary.

Source commit/tree, tracked diff fingerprint, dirty status, observed untracked
build-input count/hash, configuration, architecture and compiler are distinct.
Unknown provenance is allowed for local development and rejected by release
packaging. Clean tracked files do not excuse untracked compilation inputs.

`tools/package_source.py` creates deterministic tracked engine-source archives,
excluding games, protected evidence and unrelated untracked work. Source archives
carry `JUDAS_SOURCE_INFO.json` plus a hashed per-file inventory; no Git history is
required to build them. Changed/missing/added inputs are detected. A different clean source archive must use a fresh build directory:
deterministic old timestamps can otherwise retain stale compiled objects. CMake
rejects that cache reuse; unchanged-source repeats and ordinary Git iteration
remain supported. Use
`--allow-untracked` only when explicitly excluding unrelated local experiments;
those inputs are counted in the source-packaging receipt, not silently included.

The Ubuntu 24.04 [baseline builder](LINUX_BASELINE_BUILD.md) supplies an older
complete compiler/userland; copying files or editing glibc symbol labels is not a
portability strategy. Native MSVC and optional MinGW remain separate workflows.
Windows SDKs retain the complete public-tool inventory and required DLL licences.

SDK inventory is declared once in `packaging/sdk-inventory.json`, consumed by
CMake and Python staging/verification. Every SDK records its tool identities,
files, dependencies and SHA256SUMS. Release staging requires a clean known source,
Release configuration, no observed untracked build inputs and a supported channel.
A channel's maturity must agree; Alpha is not GA Stable. No staging command tags,
publishes, promotes a branch or grants human acceptance.

Build overrides are for deliberate development/tooling: clear an old
`JUDAS_ENGINE_VERSION` override to follow the authoritative file. Official release
preparation rejects SDK identities that differ from their bundled version file.
Publication uses separately approved immutable artifacts, not an untested rebuild.

## Update notification

The editor checks published version manifests, not Git ancestry. See
[update preferences, trust and integrity](VERSIONING.md#updates). Manual and
startup checks never install anything or overwrite a project.

## Channel-aware CI qualification

The hosted focused gate builds **both Experimental and Alpha** on either branch.
This catches channel-dependent behaviour before promotion rather than discovering
it only when main selects Alpha. Both configurations retain SDK qualification.

CTest and release qualification share the policy in
[scripts/prealpha_validate.py](../scripts/prealpha_validate.py). Original native
fixtures and assertions are unchanged. Experimental executes all scheduled fluid
tests. Alpha retains direct liquid geometry/storage tests, Basic Water/swimming
and explicit refusal/ownership/export controls. The two native targets requiring
RuntimeWorld advanced fluids are unavailable in Alpha for the published contact
stability restriction.

Those cases remain visible in CTest. The runner checks the binary's source and
configured channel before returning **77 / not-exercised**; CTest reports them as
skipped, never passed. Their individual JSON receipts retain `passed: false`. A
wrong/stale binary, a native failure (including native exit 77), or an unrelated
failing test remains a failure. Group runs containing unavailable cases return
77 rather than claiming complete coverage. `--channel` is accepted only for a
configure-time registration plan; execution uses the embedded binary identity.

`judas-validation-channel-plan.json`, per-case results and CTest logs are retained
in each channel's separate CI artifact. Known historical manual failures retain
their existing admission rules. The original six Alpha CI failures are preserved
in [the investigation record](evidence/alpha-ci-channel-policy/ORIGINAL_FAILURE.json);
this repairs test selection, not the experimental fluid solver. No published tag
or package is replaced by this correction.
