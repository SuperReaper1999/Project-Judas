# Build identity and reproducible SDKs

`engine-version.json` owns SemVer, maturity and the display name. CMake's
`JudasBuildIdentity.cmake` records the actual channel, revision/tree, source
fingerprint, dirty/untracked observations, platform, architecture, compiler and
configuration. A matching version string alone does not establish matching tools.
Git source and verified source archives retain different honest provenance.

## Configure and inspect

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
  -DJUDAS_BUILD_CHANNEL=Experimental -DJUDAS_ENABLE_VALIDATION=ON
python3 tools/workspace_storage.py run -- cmake --build build \
  --target judas_prealpha_tests judas_linux_sdk -j2
build/judas --build-info-json
python3 build/linux-sdk/tools/package_sdk.py verify build/linux-sdk
```

Configure from the source root and reuse one valid toolchain/configuration cache.
Ordinary development permits honest dirty provenance. `JUDAS_SDK_RELEASE=ON`
requires clean Git/archive source, Release and an admitted distribution channel.
Do not relabel old executables or bypass source-fingerprint checks. Reconfigure/
rebuild after source identity changes; a neighbouring executable cannot prove that
an old native test binary is current.

`JUDAS_BUILD_IDENTITY_ONLY=ON` allows the isolated identity infrastructure to be
configured without a graphics SDK. That is not an engine/editor/SDK build.
`JUDAS_ENABLE_VALIDATION` registers bounded gates; `JUDAS_INCLUDE_KNOWN_FAILURES`
controls explicit manual finding admission, not permission to count failures as passes.
The [manifest](../tools/validation/suites.json) owns actual registrations.

## Platform and packaging boundaries

Build `judas_linux_sdk` using the [shared inventory](../packaging/sdk-inventory.json).
Official SDK distribution is Linux-only. Windows build targets remain available
for independent use under the [Windows policy](WINDOWS.md#platform-support-and-executable-trust).
Staging/checksum verification is not relocation or application proof.
Use [SDK qualification](DISTRIBUTION.md) for create/author/save/Play/Stop/reopen and
moved exports with source dependencies hidden.

Linux binaries inherit their build host ABI. The [Ubuntu 24.04 baseline builder](LINUX_BASELINE_BUILD.md)
uses a complete older userland; it does not rewrite symbol requirements in newer
binaries. [Windows](WINDOWS.md), [Android](ANDROID.md) and
[browser](BROWSER_SUPPORT.md) have separate prerequisites and scopes.

## CI and promotion

[Routine CI](CI.md) builds one Alpha-labelled V2 configuration, selected registered
checks and the public SDK. Advanced Fluids remains project-opt-in in every V2
channel; retired channel skip plans are not the current validation policy.
Documentation/tooling-only changes use their corresponding lightweight checks.
The existing path-routing controls verify those exclusions.

Main promotion requires operator acceptance of the exact candidate. The machine
proof in `scripts/check_ci_promotion.py` currently accepts a completed successful
**Linux release-candidate workflow with its successful `linux` job**, from
this repository's Experimental branch at that exact SHA. Routine focused gates
alone are not that promotion proof. Incomplete/skipped/foreign-source or tooling-
only success rejects. This check still does not establish human acceptance.

A source archive records exact committed blobs and excludes generated caches.
Unexpected authored inputs/dirty source must be addressed explicitly; testing does
not turn output files into release source. [Release preparation](RELEASING.md)
never publishes or promotes automatically. Successful routine diagnostics retain
three days; failures/candidate artifacts fourteen days in the current workflows.
See [storage policy](WORKSPACE_STORAGE.md) for local budgets and output ownership.

Editor update checks compare trusted published version manifests, not Git ancestry,
and never install or overwrite projects. See [versioning](VERSIONING.md#updates).
