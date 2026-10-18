# Pass 5 native Windows qualification

## Candidate and environment

Native Windows functional results for tracked Experimental checkpoint `139d0de20e2e443bd186a6ed8f786eaa223cbba6`. Its runtime correction is the impact broadphase candidate cache from `be621491a6a1a578ac87e8c82e3b297ed8b18614`. The curved-contact research prototypes are excluded.

Windows 10 Home 22H2 ran in the established `win10` KVM guest, using Visual Studio 2022, MSVC 19.44.35229.0, x64 Release and the existing static-md dependency installation. Compilation reused the original CMake source/build paths. No cache path rewriting or SDK aggregate rebuild was performed.

- Full source archive: **1,438 selected tracked files**, clean archive identity; untracked prototypes excluded explicitly.
- Archive SHA256: `5d945ae3bc4955dc17aab6d6d8a90c86a92213534fd23b15cf7dd59495c12cfb`.
- Native build ID: `0.1.0-prealpha-Experimental-139d0de-clean-b271ec3d7937`.
- Before-source verification: all 1,435 files from preserved `9deb47d1`; original archive SHA `cfedc6eee2ea3e8d4a9d406c307ec6822be9ed0e0dfe18fde0307002827cebb5` matched.
- The exact delta changed 16 source/metadata paths. Complete before/after inventories are in [SOURCE_DELTA.json](SOURCE_DELTA.json).
- Configure and all **16 selected targets** passed. Native build: 138.17 seconds, **zero compiler warnings/errors**. Configure retained upstream HarfBuzz/FreeType notices and an unused cached toolchain-variable warning.

[BUILD_PROVENANCE.json](raw/BUILD_PROVENANCE.json), [BUILD_RESULT.json](raw/BUILD_RESULT.json), [SOURCE_VERIFIED.json](raw/SOURCE_VERIFIED.json) and [build.log](raw/build.log) preserve the actual inputs, commands and outputs.

## Functional results

| Contract | Observed result |
|---|---|
| Candidate cache | 17 checks passed; deliberately omitted revision produced the intended nonzero failure. |
| Impact timing / contact lifecycle / continuation | 3,197 / 1,076 / 112 checks passed. |
| CharacterMotor / rigid motion / impact robustness | 32 / 10,239 / 4,412 checks passed. |
| World persistence / origin admission | 27 / 23 checks passed. |
| Prepared-contact cache | Complete follow-up passed 13,327 checks, reading all 823 original fixture rows and 887 primitive pairs. Initial missing-data failure retained below. |
| Original curved-mesh continuation | **372 checks, 4 failures**, unchanged original arguments `60 "" 1.2`: coarse 11, medium 20 and dense 15 crossings, plus the rotated assertion. |
| Curved-mesh first arrival control | **372 checks, zero failures**, original arguments `60 "" 1.2 entry`. |
| M70 actual physics and cold save | 98 checks; separate writer/reader 10 / 14 checks passed, including actual partial authority and resumed finite poses. |
| M77 new-format cold continuation | Step 24 / 13, standing 23 / 13 and recovery 21 / 13 checks passed in separate writer/reader processes. Every reader passed the unchanged measured physical standing assertion. |
| Editor | Authoring batch and undo, duplicate/undo and Play/Stop passed; authored scene bytes remained identical and original editor preferences were restored. |
| Ordinary runtime | Save/load, scene transition, pause/resume and reload passed; deliberately absent expected scene returned nonzero. |
| Export | Ordinary consumer export produced six assets/four scenes, 58,803,881 bytes. Relocated Unicode/spaces package identity and full standalone harness passed from an unrelated working directory. |

The fresh M77 readers observed physical endpoints, including both supporting feet:

| Reader | Upright | Speed (m/s) | Foot contacts | Landings |
|---|---:|---:|---:|---:|
| Corrective step | 0.998602 | 0 | 4 / 4 | 1 |
| Standing | 0.999929 | 0.00000310053 | 4 / 4 | 0 |
| Recovery | 0.999991 | 0 | 4 / 4 | 0 |

[FUNCTIONAL_RESULTS.json](raw/FUNCTIONAL_RESULTS.json) retains every initial child exit, stdout and stderr. [CONTACT_FOLLOWUP_RESULTS.json](raw/CONTACT_FOLLOWUP_RESULTS.json) and [FINAL_INTEGRITY.json](raw/FINAL_INTEGRITY.json) retain the corrected prerequisite result and final classification. The initial aggregate remains false because its failed prerequisite attempt is preserved. Final integrity found **zero unexpected functional outcomes** after that explicit follow-up; the four known curved-contact failures remain open.

## Preserved setup failure and reproduction boundary

The engine-profile source archive intentionally excludes protected historical evidence. The unchanged contact-cache test reads `docs/evidence/ftft4/geometry/fixtures.txt`; the first native attempt read zero rows and failed its two prerequisite assertions. Other portions of that run completed, but it was not counted as a passing fixture.

Only the exact tracked fixture was copied into the disposable test working directory, with SHA256 `dcbcb9bf038f11662a2e6c388cc21cc4bdcbfe6807c54a282bb6dea87d7b57ce`. The narrow rerun passed all 13,327 checks. The original source tree, protected fixture, test and thresholds were unchanged. This records a **source-archive test-data coverage gap**; it does not establish that the engine-profile archive independently contains every historical regression prerequisite.

One progress request timed out while the guest was actively compiling, and a live log read encountered its writer's sharing lock. Completed build receipts/logs were subsequently retrieved. These transport observations are recorded in [TRANSPORT_AND_SCOPE.json](TRANSPORT_AND_SCOPE.json).

The scheduled-task entry shell's final exit does not propagate its called script's result through transcript cleanup. Individual native exits/output, the preserved aggregate and explicit final-integrity classification are used as evidence. The task shell's exit was not used as a test pass.

## Graphics and platform limits

Stock QXL startup correctly rejected the unavailable OpenGL 3.3 context. Graphical application checks used Mesa 26.2.4 llvmpipe DLLs **only in disposable task copies**. The pristine exported package contains its ordinary native runtime payload, and the software-rendered copy is separate. These results do not qualify physical Windows GPU, controller or audio hardware. Windows VM timings are diagnostic; Linux supplies the performance acceptance measurements.

The existing full Windows SDK was preserved. This pass rebuilt and exercised current runtime/editor/export plus selected fixtures; its coverage is not a fresh all-tool SDK qualification or a production-suite run.

## Integrity and retained artifacts

Final verification matched all 1,438 source files and metadata, all executed native AMD64 PE hashes, three application identities, 26 accepted native backup hashes, the 41-file accepted pristine export and the accepted original source archive. The native engine archive SHA256 is `c685583363ee7b2cfd5506a9dc7e650cdd19a46f85023b665ba78d8a6ca8e662`.

The owned desktop task was removed. Original editor preferences were restored. The VM returned to its initial suspended state; persistent domain XML SHA remained `2e2384c9412d9e6ffef2cabcea79dc6751ad728d8026a1dfbdd88e151d4bfa0e`.

- Guest task: `P:\JudasValidation\PreAlphaPass5`.
- Pristine current game: `P:\JudasValidation\PreAlphaPass5\GuiWork\Moved Game é α`.
- Functional software copy: adjacent `Moved Software Game é α`.
- Accepted Pass 4 source/archive/export and copied PE backups remain preserved.
- Raw guest collection: 151 files, transport ZIP SHA `d217df44da346375d18909efa37604697be710620dadf98d532fe9d5e5c6a800`.
- Four editor preference contents are omitted from published evidence as unrelated user state. Restoration hashes remain, and exact originals remain in the owned guest/local cache.

[Reproduction helpers](reproduction/) preserve the as-run scripts. `prepare_source_delta.py` consumes verified full source archives; `apply_source_delta.py` refuses mismatched source/artifacts. Run selected native targets with `configure-build.ps1`, the ordinary limited desktop checks with `launch-desktop.ps1`, and the exact-fixture follow-up with `launch-contact-followup.ps1`. Existing task outputs are intentionally refused to preserve earlier attempts. Guest paths and native toolchain prerequisites are explicit in the scripts.

Run `python3 docs/evidence/prealpha-pass5/windows/verify_delivery.py` from the repository root to verify the delivered file hashes. This verifies evidence integrity, not human acceptance.
