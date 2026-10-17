# Pre-Alpha pass 2: distribution qualification

Development base: `528cfb1b430d710ba5a4d2d13569aad106e3044e` on Experimental.
This work does not promote main, publish Alpha or establish human acceptance.

## Changes

- `tools/binary_dependencies.py` reads bounded ELF64 metadata and PE32/PE32+
  regular/delay imports without executing the inspected binary. ELF requirements
  come from `VERNEED`; exported definitions cannot inflate the compatibility floor.
- `tools/inspect_sdk.py` verifies the exact SDK inventory before reporting ABI,
  architecture and dependencies. Linux qualification can require an actual host
  identity and a glibc ceiling, with explicit trusted-SDK `ldd` checks. Windows
  checks every bundled DLL as well as each executable, resolving imports against
  bundled DLLs, the actual System32 and explicitly identified API-set contracts.
  It refuses to write its receipt into the SDK it just verified.
- CI now has a mechanical **Ubuntu 24.04 / glibc 2.39** SDK gate, including all
  17 public executables. Only a completed run establishes that a particular
  checkpoint passes. There is no universal Linux/AppImage claim and no promise
  that system-library closure proves GPU compatibility.
- The SDK includes the portable inspector, binary reader and independent APK
  verifier, usable without a source checkout or JDK. Drive-relative
  Windows manifest paths such as `C:outside.txt` are rejected on every host.
- Android preflight resolves one complete JDK 17+ and consistently uses its
  compiler, runtime and keytool. Desktop tool identities are checked before native
  compilation; each ABI is checked before **and after** its build, and desktop
  identities are checked again before export. A CMake auto-reconfiguration cannot
  silently produce newly compiled code with stale embedded packaging provenance.
- APKs now hash the stripped libraries actually shipped, including libc++, in
  `judas-native.json`. The independent verifier checks exact content/native sets,
  hashes, architecture, per-ABI identity, 16 KiB ELF load alignment and native
  import-name closure. Used Android system dependencies also require actual
  API-26 NDK stubs during building. This is not full symbol/API/device proof.
- APK, receipt and checksum are prepared together and rolled back on an ordinary
  publication error. Explicit cache-lock lifetime closes on every exit path.
  Publication is not a power-loss-atomic three-file transaction. Concurrent
  publishers/readers of the same destination remain unsupported.

No runtime, renderer, physics or gameplay semantics were changed by this work.

## Executed results

| Check | Observed result | Exact scope |
|---|---|---|
| Existing packaging controls, plus drive-relative-path negative | 24 tests pass | Actual fixture staging, inventory/identity/checksum failures, source archive controls; no compiler/device |
| New distribution controls on Linux | 16 tests pass | Independent ELF/PE fixtures, malformed metadata, excessive ABI floor, missing imports, APK corruption/closure, JDK handling, full mocked builder, build-time identity change, injected publication failure and canonical Android ABI identity |
| Actual pass-1 SDK inspection | 17 tools; 43 host dependencies resolve | Exact engine artifact `683cc3ddd81743febd15845e487de1aa7140556d`; not relabelled as pass-2 binaries |
| Same SDK under declared older baseline | Correctly fails | All 17 require newer glibc; host Ubuntu 26.04 is not Ubuntu 24.04 |
| Actual cached complete JDK | Pass: javac 17.0.20.1 | Coherent java/javac/keytool paths; no Java package installation |
| Actual system JRE | Correctly refused | No javac; failure occurs before native compilation |
| Native Windows Python follow-up | 15 tests pass in 0.933 seconds | Earlier exact source snapshot, before the later canonical-ABI test; actual Windows filesystem/Python semantics, with external compiler/signing operations simulated |
| Actual Windows PE file | Parses PE32+, x86-64 and imports | Exact System32 `cmd.exe` bytes/hash recorded; not current Judas execution |
| Native Windows C++ build | **Not completed** | Bounded configure attempt stopped under severe guest disk pressure; no native engine/SDK pass |
| Final native SDK follow-up | 17 tools / 212 files verify; loader check passes | Exact staged source fingerprint `7a0361d952377e46966122565e9c9c26d2305d1c041dc36a9a1a9d82dc0c5014`; Ubuntu 26.04 artifact, not Ubuntu 24 certification |
| Actual Android full rebuild | 260.07 seconds; signed bytes created, publication refused | Original verifier misclassified canonical ABI spelling; full failure preserved, not a successful transaction |
| Final incremental Android transaction | 17.77 seconds, successful | Real compile/relink/package/signature/alignment/content/native verification; no install/emulator/device execution |

The existing SDK's actual maxima are **GLIBC 2.43**, **GLIBCXX 3.4.32** and
**CXXABI 1.3.15**. Its 43 resolved host libraries do not make it runnable on older
distributions. Both the positive host receipt and unchanged-artifact older-baseline
failure are retained. The forthcoming CI receipt, if successful, qualifies only
its own Ubuntu-24-built artifact/source identity.

The full mocked Android path exercises real ZIP generation, metadata, byte
verification and publication; CMake, javac, DEX, strip and signing commands are
simulated. It is deliberately not presented as successful native compilation,
Android-tool signature validation, emulator execution or hardware acceptance.
The independent verifier cannot retroactively certify older APKs that do not
contain the new native inventory. Their original evidence remains unchanged.

## Windows setup and preserved failures

The existing Windows 10 Home VM initially could not start because its readonly
installation CD referenced a missing ISO. With operator-authorized platform setup
and root approval, only that stale CD source was ejected. Before/after inactive
XML copies and their exact one-line diff remain in `.cache/prealpha-pass2/windows/`;
the receipt hashes both. Disk, boot order, CPU/RAM and graphics configuration were
unchanged. The existing guest then booted and its agent responded.

VS2022/MSVC 14.44, CMake/Ninja, Python, Git and vcpkg are installed. Initial free
space was approximately 512 MiB; it fell to approximately 80 MiB during the bounded
configure attempt. The cause of that OS-level consumption was not established.
Only this task's own process tree was stopped; no unrelated process or historical
file was deleted. Read-only inventory found only approximately 517 MB of old
generated binaries/objects outside protected source/tooling, insufficient safe
headroom for a reliable full current SDK build. Original source, saves, evidence
and established VM configuration were preserved. Full native engine, all-tool SDK,
relocated game, graphics and hardware checks remain outstanding.

The **first actual Windows Python run failed**: one canonical-path comparison
used case-sensitive strings, and three test cleanups exposed an unclosed builder
lock handle retained through exception tracebacks. The original source archive,
15-test receipt (one failure/three errors) and log remain intact. The lock now has
explicit context ownership and the comparison checks resolved filesystem paths.
A distinct hashed follow-up source directory then passes the same 15 tests.
No test was skipped or threshold weakened.

### Actual APK failure and corrected follow-up

The real full arm64 build reached development signing and passed both Android
signature and ZIP-alignment checks, then the new independent verifier refused
publication. Inspection established that CMake emits the canonical architecture
name **`arm64-v8a`**; the verifier accepted only equivalent CPU spellings
`aarch64`/`arm64`. Every actual native ELF independently reported machine **183**
(AArch64), with valid 16 KiB load alignment. The failure was metadata spelling,
not a native runtime, physics or gameplay defect.

The verifier now accepts the canonical ABI spelling, retaining the independent
ELF checks. A new positive canonical-ABI fixture and wrong-CPU negative raise the
Linux controls to **16/16**. The earlier Windows run remains **15/15**, not a claim
that the additional final test ran there. The original failed signed APK and
original verifier remain unchanged in `.cache/prealpha-pass2/android/failure-abi-name/`;
their hashes, exact compiler log, cause and corrected verification are retained
in this evidence folder without checking in the 53 MB executable package.

After the test-only assertion/verifier corrections, desktop identities were
refreshed and frozen before the **17.77-second** incremental APK transaction.
Its actual development APK is:

`/home/conner/Documents/GitHub/Project-Judas/.cache/prealpha-pass2/android/ConnectedWorkshop-PreAlphaPass2-arm64.apk`

- **53,484,213 bytes**, SHA-256
  `43b76986b69c66fbef3a37152eda9c82890a0ef2290a499af4c52163ed14569a`.
- Application `org.projectjudas.connectedworkshop.prealpha`, version code **2**,
  version name **prealpha-pass2**, minimum API **26**, target API **36**.
- Build identity **`0.1.0-prealpha-Experimental-528cfb1-dirty-7a0361d95237-u84f47c24fcd0`**.
  It identifies the exact staged candidate before the final evidence commit,
  not a falsely clean or retrospectively relabelled checkpoint.
- Actual apksigner **v2/v3** verification and **16 KiB zipalign** pass. Independent
  checks verify all **21** content files / **2,261,535 bytes**, the three exact
  stripped libraries, their import-name closure, API-26 stubs, per-ABI identity,
  AArch64 machine and every ELF load alignment. Both APK/receipt hashes match.
- The full native rebuild emitted **19 Clang warnings**: ten indentation,
  seven C++20 structured-binding-capture extensions and two class/struct tag
  mismatches. All 19 diagnostic source lines exist in accepted `9cf491a...`;
  this source comparison is not a new matched historical compiler run. Four
  upstream CMake deprecation warnings and a Java deprecation note are separate.
  The final incremental run did not recompile those unchanged translation units.

Raw commands/results, signature, alignment, compiled-manifest and independent
verification receipts are under `android/`. The package was **not installed or
executed** on an emulator or physical device. This is native compilation,
signing and artifact correctness, not mobile performance or human acceptance.

After preserving results, guest-agent shutdown timed out and an ordinary ACPI
shutdown request left the VM running. Root then authorized reversible suspension
of this task's guest for Linux timing; `domstate` confirmed **paused**. No forced
power-off was used. The final state receipt is retained for the operator.

Code review also caught the JDK dictionary being shadowed by the generated Java
directory and stale pre-build identities surviving a CMake reconfiguration.
Both were corrected before a native APK attempt, with a complete mocked success
path and reconfiguration negative control. The small lock-ownership extraction
initially omitted the ABI local variable; the focused test immediately caught it
and the variable was moved into the extracted transaction before the passing run.

## Reproduction

```sh
python3 scripts/prealpha_packaging_checks.py --output /outside/packaging.json
python3 scripts/prealpha_distribution_checks.py --output /outside/distribution.json
python3 tools/inspect_sdk.py /trusted/sdk --host-check --output /outside/actual-host.json
python3 tools/inspect_sdk.py /trusted/sdk --host-check --max-glibc 2.39 --require-os ubuntu:24.04 --output /outside/baseline.json
python3 tools/verify_android.py /outside/game.apk --receipt /outside/game.build.json --output /outside/apk-verification.json
```

For a fresh Windows functional fixture, run `scripts/prepare_windows_prealpha.py`
and the enclosed `Run-PreAlphaChecks.ps1` in its own explicitly supplied work root.
The archived original/follow-up ZIPs here preserve the exact inputs used above;
their inventories record individual source hashes and scope. The retained task
scripts show the actual VM reproduction paths. They do not boot/configure a VM
or grant extra disk capacity.

## Remaining boundaries

- No current full Windows SDK/native engine certification, no Windows GPU claim.
- Ubuntu 24.04 binary gate needs its own final-checkpoint CI result; locally built
  Ubuntu 26.04 executables remain newer-ABI artifacts.
- Actual new arm64 build, signatures, ZIP alignment and artifact checks pass for
  the precisely identified staged candidate; device/emulator/hardware acceptance
  remains separate and outstanding for this Pre-Alpha change set.
- ELF reader currently supports little-endian ELF64, matching supported native
  targets; unsupported formats fail. PE legacy VA-based delay imports fail rather
  than being incorrectly declared resolved.
- API-set import recognition does not establish a Windows minimum-OS guarantee.
  Runtime plugin/dlopen dependencies are not implied covered by static imports.
- SHA-256 inventories detect accidental drift, not publisher authenticity. No
  production signing credentials, secrets or hardware installations are included.

See [distribution documentation](../../../DISTRIBUTION.md),
[Android](../../../ANDROID.md) and [Windows](../../../WINDOWS.md).
