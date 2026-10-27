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
