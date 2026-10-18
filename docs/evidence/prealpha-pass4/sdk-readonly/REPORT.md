# Packaged Python tools must preserve their SDK

## Actual failure and cause

Native Windows qualification first verified the SDK successfully, then ran its
**packaged** `tools/inspect_sdk.py`. Before checking the SDK, Python imported the
sibling `binary_dependencies.py` and `package_sdk.py`, creating two `.pyc` files
under the package's `tools/__pycache__`. Strict verification then correctly
rejected the unexpected files, including the inspector's own invocation.

The original native receipt contains 42 cases, 41 passing. Its
`sdk-native-pe-imports` failure and the exact two generated paths/byte counts/hashes
are preserved unchanged in `WINDOWS_ORIGINAL_TOOLS_RESULTS.json` and
`WINDOWS_ORIGINAL_SDK_INSPECTOR_MUTATION.json`. Original cache binaries remain
preserved outside tracked evidence; they are not staged here.

`verify_android.py` imports the same sibling modules. Even when checking an
external APK successfully, it likewise mutated a writable SDK and caused a later
SDK verification failure. `package_sdk.py` and `binary_dependencies.py` import
only standard-library modules and do not cache themselves when executed directly.
The source-only Android builder is not part of the distributed SDK inventory.

## Small generic correction

Both distributed entrypoints set `sys.dont_write_bytecode = True` before sibling
imports. This prevents new cache writes; it does not ignore, delete, bless or
change the expected SDK inventory. No engine C++, API or package checksum policy
changes. Pre-existing unmanifested caches still produce the strict error and
need an explicitly owned cleanup or fresh SDK restage.

This is an entrypoint policy for the packaged command-line tools. It does not
promise that arbitrary callers can import any SDK module without the caller
first choosing its own no-bytecode policy.

## Actual 17-tool Linux SDK follow-up

The [separate native-package receipt](linux-native/REPORT.md) additionally runs
nine commands against the real 17-executable Linux SDK, without rebuilding C++.
An owned copy of the original package reproduces the same two-cache mutation and
self-verification failure. A freshly staged package with only the corrected
scripts passes its **packaged** inspector and host loader, the packaged Android
verifier entrypoint, and before/after strict verification with every SDK file
identical. An intentionally unexpected cache is still rejected. The original SDK
is untouched and all 17 native executable hashes match the qualified originals.

This closes the packaged-invocation coverage gap on the measured Linux host;
it does not imply a new Android device, hardware GPU or native Windows result.
The compiler-free valid-metadata APK case below separately exercises the Android
verifier's successful checking path rather than only its argument entrypoint.

## Why previous Linux qualification missed it

The prior Linux loader qualification invoked the **source-tree** inspector with
the SDK as its target. Its helper imports were therefore outside the SDK. Those
results remain valid ABI/dependency observations, but did not prove that running
the documented packaged inspector preserves its own hashed installation. The
new regression invokes the actual copied `SDK/tools/inspect_sdk.py` and
`SDK/tools/verify_android.py` in separate Python processes.

## Reproducible Linux mechanism checks

The compiler-free regression uses four fresh writable SDK directories, exact
script copies, a strict manifest and independently generated metadata-only ELF
and APK fixtures. These are format/ownership controls, not runnable Android or
engine qualification. All outputs are outside the package under test.

Each subprocess uses ordinary bytecode behavior: no `-B`; `-E` prevents inherited
`PYTHONDONTWRITEBYTECODE` or `PYTHONPYCACHEPREFIX` from masking the defect. Full
before/after file inventories and hashes are recorded. The two original-negative
script copies are byte-identical to the captured production sources, rather than
a fabricated alternate bug. All 16 commands have their expected exits:

| Entry/source | Initial SDK verify | Actual operation | SDK afterward |
|---|---:|---|---|
| Original inspector | Pass | Fails its own inventory check | Fails; two new caches |
| Original Android verifier | Pass | Valid metadata APK passes | Fails; two new caches |
| Corrected inspector | Pass | Inspection passes | Pass; every file identical |
| Corrected Android verifier | Pass | Valid metadata APK passes | Pass; every file identical |

For each corrected SDK, deliberately inserting an unexpected `.pyc` still causes
verification failure. Independently corrupting an existing payload also fails.
The fix therefore cannot pass by merely skipping caches or weakening hashes.
`LINUX_RESULTS.json`, `SOURCE_PROOF.json` and raw logs preserve the exact commands,
source hashes, outputs and mutation inventories.

Run from the repository root, choosing a fresh output directory:

```sh
python3 scripts/prealpha_sdk_readonly_checks.py --output /tmp/NEW-sdk-readonly-check
```

The exact runner is retained under `reproduction/`. Original Windows failure and
Linux controls are not relabeled as final native Windows success; corrected
native SDK restaging and functional qualification are recorded separately by the
platform workstream. No full build, engine fixture or performance benchmark was
run for this narrow script correction.
