# Pass 4 — native Windows qualification

## Result and identity

The full native Windows Release SDK, editor, ordinary exported game and focused
new-format cold-save contracts now execute successfully in the existing Windows
10 Home 22H2 VM. This is functional qualification with software graphics, **not**
physical Windows GPU/audio/controller acceptance, a performance gate, a clean OS
installation or a full-suite certification.

- Native C++ source: `9deb47d1667e5b6bd1abe708ed343c3fbf6fe847`.
- Separately staged Python-tool correction: `df8929f73505d407697b5d35b115fc71d9100471`.
- Native identity: `0.1.0-prealpha-Experimental-9deb47d-clean-86800e25cc69`.
- MSVC 19.44.35229, VS 2022, x64, Release; existing pinned dependencies/toolchains.
- SDK: **18 executables / 238 manifest files**. Existing compiler/upstream warnings
  are retained. Native binary build identities were never rewritten to the later
  Python-only revision.

[RESULTS.json](RESULTS.json), [native fingerprints](NATIVE_BINARIES.json),
[final SDK manifest](FINAL_SDK_MANIFEST.json) and
[final integrity](FINAL_INTEGRITY.json) record the exact scope and bytes.

## Native results

| Contract | Result |
|---|---|
| Full native SDK and seven focused test targets | Build passed |
| All 18 executable identities | Every field matches native SDK identity |
| Tools: real terrain/volume editing, deformation bake, model import, texture/collision/HDR cooking, script linking, save identity, diagnostic negatives | 41/42 initial cases passed; the inspector failure below was corrected separately |
| Windows readiness | 21 checks passed |
| Relocated Unicode/spaces SDK, ordinary export, moved runtime identity, missing ICU rejection, corrupted SDK rejection | 6/6 cases passed |
| Corrected inspector and strict inventory controls | 7/7 cases passed; no `-B` or Python environment mask |
| Impact robustness | 4,412 checks passed |
| Origin admission | 23 checks passed |
| CharacterMotor | 32 checks passed |
| World persistence | 27 checks passed |
| Actual M70 full character lab | 98 checks passed |
| M70 cold write/read; M77 in-progress corrective-step, standing and recovery write/read | Eight separate processes; 131 checks passed |
| Limited desktop editor: batch/undo, duplicate/undo, Play/Stop, save | Passed; original authored scene remained byte-identical |
| Runtime and moved standalone: actual motors/impulse, save/load, rotated transition, pause/resume and reload | Passed; incorrect-scene negative fails as intended |
| Original curved-mesh continuation contract | **372 checks, 4 failures — remains open** |

M77 readers reach **measured physical Standing**, including actual root transforms,
upright/height/speed and live support contacts. These are fresh process restores,
not a cached controller-status assertion. The original historical snapshot's
missing-continuation limitation is unchanged.

[Initial native tool outcomes](TOOLS_RESULTS.json),
[original desktop/physics results](GUI_RESULTS.json),
[cold-process outcomes](COLD_FOLLOWUP_RESULTS.json),
[complete-fixture M70 follow-up](M70_COMPLETE_FIXTURE_RESULTS.json),
[packaging](PACKAGING_RESULTS.json),
[inspector follow-up](INSPECTOR_FOLLOWUP_RESULTS.json) preserve both failures and
successful corrections. The initial cold-follow-up aggregate is false because it
also retains the ordinary M70 missing-cookbook setup failure; its eight cold
invocations all passed. The later ordinary M70 result is separate.

## Demonstrated defects and corrections

1. **MSVC C2026:** the accepted baseline's first embedded JudasJS literal had grown
   to 16,866 bytes. Original native compilation failed. One additional adjacent
   raw-literal boundary yields 13,783 / 3,083 / 13,439-byte chunks; the concatenated
   30,305-byte JS payload is unchanged. Both the standalone MSVC negative/positive
   and full corrected engine build are preserved. See the
   [byte-equivalence proof](../platform/windows-string-split/README.md).
2. **SDK inspector self-mutation:** importing sibling helpers created two `.pyc`
   files inside the checksum-verified SDK before inspection, so the documented
   command rejected its own new files. Both shipped inspector entrypoints now
   disable bytecode writes before their local imports. Normal inventory staging
   regenerated the SDK manifest; no exceptions were added to the inventory.
   Plain Windows inspection and both before/after verification pass; an arbitrary
   unexpected file still fails. Original `.pyc` bytes are preserved and SHA-verified
   outside Git; [mutation receipt](SDK_INSPECTOR_MUTATION.json) and
   [separate tooling provenance](TOOLING_RESTAGE.json) identify them.

## Setup failures retained honestly

The isolated verification initially lacked prerequisites normally supplied by a
repository checkout. These were repaired in the **task harness**, without editing
engine, fixtures, controller settings, timestep or acceptance thresholds:

- A Windows locale-dependent metadata read misdecoded a UTF-8 fixture filename;
  explicit UTF-8 restored exact source-path verification.
- SYSTEM-created private SDK staging did not allow the ordinary limited desktop
  account to read it. Only this task SDK received explicit read/execute permission.
- Isolated GL test executables lacked engine fonts. The unchanged SDK fonts were
  copied beside them.
- The broad M70 test also requires two relative cookbook files. Their absence
  produced an unhandled filesystem/test-runner failure (`0xc0000409`); supplying
  those exact original files restored all 98 checks. Its original failure remains.

Original setup/compiler failures are in [raw](raw/); successful prerequisite
hashes and follow-ups remain separate. No failed engine test was weakened.

## Storage and isolation

[CLEANUP.json](CLEANUP.json) records authorization, classifications, counts,
original raw-inventory hashes and backup locations. Rebuildable intermediates and
42 obsolete M78 package binaries were removed; all unique selected package bytes
were first copied/hash-verified on Linux. Four old generated Build trees (8,193
files) were fully archived and independently verified on Linux and P: before
relocation. Junctions preserve their old paths; historical readiness still passed.
Sources, saves, results, protected evidence and installed toolchains were retained.

The operator explicitly authorized expanding the existing disk to 90 GiB. Its
actual original virtual size was 40 GiB. The original system/C:/recovery partition
positions and sizes remain unchanged. Because recovery blocks extending C:, only
the new empty tail was formatted as a 50 GiB P: work volume. No recovery partition
was moved/deleted. C: has about 812 MB free and P: about 48.2 GB free at completion;
future C: servicing headroom remains modest.

Persistent VM XML is byte-identical before/after:
`2e2384c9412d9e6ffef2cabcea79dc6751ad728d8026a1dfbdd88e151d4bfa0e`.
The existing four-vCPU/6-GiB/QXL isolation remains. The completed task's transient
scheduled task was removed; original editor preferences were restored; no Judas
process remains. The VM is running normally.

## Graphics, limitations and artifacts

Stock QXL cannot provide the required OpenGL 3.3 Core entrypoints; the unmodified
runtime reports that explicitly. Existing Mesa 26.2.4 llvmpipe supplied software
OpenGL **only in disposable test copies**. Stock SDK and pristine exported game
contain no Mesa DLLs. Tests ran as the ordinary limited `Conner` desktop user,
session 1, with MSVC/vcpkg/Git removed from runtime PATH.

- VM SDK: `P:\JudasValidation\PreAlphaPass4\Build\windows-sdk`.
- Pristine moved game: `P:\JudasValidation\PreAlphaPass4\Moved Game é α`.
- Software test game: `P:\JudasValidation\PreAlphaPass4\GuiWork\MovedSoftwareGame é α`.
- Reproduction runners: [reproduction](reproduction/); adjust the task/toolchain
  roots on another machine and supply the same unchanged project prerequisites.
- Raw large backups/builds/binaries remain in ignored `.cache/prealpha-pass4/windows`
  and the VM; they are not repository payloads.

Final hashes match **1,435 archived source files, 26 native binaries and 41 pristine
export files**. Screenshots show the actual [editor](screenshots/editor.play.png)
and [moved runtime](screenshots/runtime.png). VM timings are diagnostic only; Linux
remains the measured performance reference. FLU-001, legacy-save restrictions,
curved-mesh continuation and physical Windows hardware qualification remain open.
