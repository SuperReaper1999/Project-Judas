# Root-coordinated isolated native build

These scripts are prepared tooling. They have not yet built or run a candidate. Windows source staging, existing archive member enumeration and native PowerShell syntax checks are recorded separately.

## Prepare exact final inputs on the host

From the repository root, only after final physics sources are frozen:

```sh
python3 docs/evidence/m78-continuation/probes/prepare_windows_direct_manifest.py final-v1 \
  --source-root . \
  --replace src/RigidBody.cpp --replace src/PhysicsWorld.cpp --replace src/CollisionGeometry.cpp \
  --header src/RigidBody.h --header src/PhysicsWorld.h --header src/RigidMotion.h \
  --case event=docs/evidence/m78-continuation/probes/event_angular_transport.cpp \
  --case rigid_motion=tests/RigidMotionTests.cpp \
  --case balance=tests/PhysicalBalanceControllerTests.cpp \
  --case physical_animation=tests/PhysicalAnimationTests.cpp \
  --case gravity=tests/GravitySelectionTests.cpp --extra gravity=src/editor/EditorHistory.cpp \
  --case lifecycle=tests/LifecycleIntegrationTests.cpp \
  --case balance_lab=tests/BalanceLabTests.cpp
```

`--source-root` may also be an isolated diagnostic tree containing the explicit files either under `src/` or at its root. Missing explicit source is an error; there is no silent fallback to another implementation. Only include files actually changed are needed in the overlay. Header ABI/layout/inline changes require their affected consumers; do not bypass that requirement.

The generator writes unique JSON/ZIP files under `.cache/windows-vm-transfer/`, and refuses to overwrite a prior label. The source ZIP only contains selected source/header/probe inputs. Unchanged tests compile from the existing guest source tree after exact host-vs-guest SHA verification, preserving relative third-party includes. A test SHA mismatch must be resolved by staging the exact current fixture, not by regenerating an expected hash from the guest.

Before invocation, set each manifest case's exact `arguments` and owned output paths where required. Balance Lab modes use separate fresh output directories, except matching cold-write/read share their corresponding directory. Native cases compile additional explicitly listed translation units via `--extra`; GravitySelection requires `src/editor/EditorHistory.cpp`. An actual Application target additionally needs its normal Application/RuntimeOptions/RuntimeDiagnostics/TestHarness translation units, rather than inventing a partial API harness.

## Native invocation after root authorization

Native scripts are staged under `C:\JudasValidation\M78ContinuationSource\Scripts`. The final multi-TU script is `windows_direct_case_v3.ps1`; previous script versions and their parser receipts remain preserved. Invoke normally from an encoded guest-agent wrapper:

```powershell
& 'C:\JudasValidation\M78ContinuationSource\Scripts\windows_direct_library.ps1' `
  -ManifestName 'm78-continuation-direct-final-v1.json'

& 'C:\JudasValidation\M78ContinuationSource\Scripts\windows_direct_case_v3.ps1' `
  -ManifestName 'm78-continuation-direct-final-v1.json' -CaseId 'balance'
```

The library step compiles only selected source translation units with the exact captured MSVC recipes and creates a separate engine archive. It leaves the baseline archive intact, checks the member count and rejects duplicate replacement member names. Cases recompile their actual unchanged test sources and link against that same isolated archive. The recorded response text, source/object/library/executable hashes, arguments, outputs and exit status establish which candidate was exercised.

Run one case at a time while the host and guest are not performing a competing heavy build/benchmark. The script preserves outputs and executables; only after independent host archival verification may root authorize reclaiming those owned outputs. Never delete shared DLLs/libraries, preserved source, old result directories, working review packages or installed tools. The fixed reserve checks supplement a fresh peak-footprint estimate; they do not justify proceeding if the real operation cannot fit.

## Prior preparation smoke

`m78-continuation-direct-recipe-preparation-only.json/.zip` is a **diagnostic event-right variant preparation**, not the final candidate and not a native result. The host generator confirmed the original object token and source CPP token were removed from case recipes, selected the exact archive member and handled a two-TU gravity case. Do not substitute it for final combined-source validation.
