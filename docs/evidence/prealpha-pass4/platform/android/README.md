# Android runtime compatibility — compile/link only

The existing NDK **28.2.13676358**, **ARM64/API26 Release** configuration compiled
and linked the extracted physics modules and final scheduling implementation.
The later byte-identical JudasJS literal correction was then compiled and the
runtime relinked successfully, after pausing the Windows VM to avoid competing
intensive builds.

- `physics-checkpoint/` preserves the complete earlier incremental build and
  actual ELF/source/runtime hashes.
- `final-portability/` records the final `ScriptSystem.cpp` compilation, shared
  runtime link, ELF inspection and hashes. Selected engine/build inputs match
  code checkpoint `9deb47d1667e5b6bd1abe708ed343c3fbf6fe847` exactly.
- The final build observes only `docs/PREALPHA.md` as tracked dirtiness. It is
  recorded rather than falsely replacing the compiled provenance with a clean
  later report checkpoint.

Final local runtime: `.cache/m88/android-arm64-v8a/libmain.so`, SHA256
`01c19d8a14776d9bdf675ab56952be482b8bb19758df039f8f992bf70a110c26`.
Existing Clang warnings remain in both build logs; the final narrow log reports
four. No toolchain dependency was added to desktop builds.

**No APK, emulator launch, physical phone, controller, mobile performance or
hardware acceptance is claimed.** This receipt establishes native Android ABI
compilation/linking for the changed engine code. It does not replace the earlier
milestones' distinct device-validation scope.

Reproduce with an already configured matching Android cache/toolchain:

```sh
cmake --build .cache/m88/android-arm64-v8a --target main -j2
```

For a new SDK/NDK installation use the normal [Android build instructions](../../../../ANDROID.md)
and preserve its actual new platform identity. The cache path above is the
recorded local configuration, not a new permanent prerequisite.
