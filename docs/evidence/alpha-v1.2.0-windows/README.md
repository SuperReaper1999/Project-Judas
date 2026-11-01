# Windows AI release qualification correction

The first exact-source preparation of `37b425ec3730bf6b9a33e3df4f64dbfc8ca5815f`
passed Linux release gates, SDK tool/application workflows and both routine Linux
channel matrices. Windows built the SDK and passed the existing selected native
checks, plus Utility AI core (165) and scripting (21), then failed the AI editor's
Unicode asset creation at check 24. The original native Windows log is retained
byte-for-byte in `windows-first-failure.log.gz`; its uncompressed hash is recorded.
No tag, main promotion or publication occurred for that candidate.

## Mechanism and correction

Windows process manifests establish Judas's documented UTF-8/long-path contract.
The Windows runtime modules enumerated executables immediately when included.
Newer subsystem targets were declared afterward and silently missed that policy.
An unmanifested test could report successful project creation at a code-page alias
of a Unicode path, then fail an explicitly UTF-8 filesystem operation.

One common Windows finalizer now runs at the end of directory configuration.
Early and late MSVC executables receive the same process manifest and runtime
dependency; MinGW executables receive their equivalent generated resource.
The existing opt-in test/performance target classification is preserved. Ordinary
libraries do not acquire executable manifests or runtime dependencies.

AI definition creation also explicitly converts UTF-8 paths at its metadata
existence, staging copy and staging removal boundaries. Exclusive publication,
source ownership and conflict rejection are unchanged. New controls inspect the
actual Unicode directory and source/metadata files, duplicate and metadata-only
ownership, save/reopen, external conflicts and staging cleanup.

Local Linux AI editor checks pass **43/43**. The configure-only CMake suite passes
**22 tests**, including real early/late target graphs for both Windows toolchains
and negative controls that remove deferred finalization or the runtime dependency.
Those graph tests inspect actual configured properties without pretending to run
Windows executables on Linux. A fresh exact-source Windows release preparation
must still establish the native outcome.

The release workflow now collects all eight native AI test outcomes before
reporting failures and retains failure logs. No failing assertion was removed,
skipped or weakened. Advanced Fluids and browser opt-in policy are unchanged.
