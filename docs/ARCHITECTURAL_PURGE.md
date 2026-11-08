# Judas V2 architectural purge

The operator approved removal of superseded implementations, historical projects
and compatibility paths on 2026-11-08. This is development on Experimental, not a
release or human acceptance of the resulting engine.

## Preserved baseline

- Engine: `2ee4203a0053cc545eab4a184c627f4f5507c4bd`.
- Protected main: `1094d64515604fe9d03fdab479efc7f880b112a8`.
- Before: 74,367 tracked files, 4,130,630,845 apparent bytes; 120,051
  first-party source code lines under `src` (C/C++/Python/CMake/shell extensions).
- Existing Linux generated build: 540 Judas target registrations, including 451
  test executables, 17 SDK tools, 23 performance targets and 49 other targets.

The six consumer games were copied, including their local ignored content, to
`/home/conner/Projects/Judas-Extracted-Games/2026-11-08-2ee4203a/` before removal.
Each sibling `NAME.preservation.json` records the engine snapshot, last project
commit and SHA-256/size of every source file. Copy verification compared all
file contents and permission modes. These are historical editable projects,
not projects promised compatibility with the current engine.

The two locally modified historical water screenshots were copied and hash
verified in that folder's `preserved-local-water-screenshots` directory.
The RPG project, its SDK and existing HDD archives are outside this operation.

## Execution register

| Bundle | Status | Validation |
| --- | --- | --- |
| A: projects, examples, historical repository content | Consumer preservation complete; historical evidence/research removed; focused fixtures extracted. Project retirement remains in progress. | Current gravity/contact/animation/IK/import/material/streaming/text checks pass; generated water scenes remain byte-identical; scalability/export/history 275 checks pass |
| B: engine-owned gameplay and legacy delta saves | Pending | Pending |
| C: PBF and retired reservoir/container liquids | Pending | Pending |
| D: Terrain V2 consolidation | Pending | Pending |
| E: obsolete infrastructure | Font/empty source, fixed-panel fallback and old preference migrations removed; UI/joint implementations owned by their binding modules. Simple particle branch review remains. | Editor build; workspace 24/24, preferences 37/37, binding registry 20/20, module acceptance 8/8; menu cold write 53/53, read 38/38, application 10/10; editor architecture 5/5 and scripting tooling 8/8 |

No compatibility shim or conversion path is required for deliberately retired
systems. Current contracts and their unique regression coverage must survive.
The Advanced Fluids Alpha lock remains a required current contract.

## Qualification boundary

Builds and automated tests do not establish human acceptance. Final SDK,
integration, platform and CI results will be recorded after the corresponding
work has actually completed. Main remains unchanged throughout the purge.

## Build provenance correction found during the purge

The old configure step retained the complete binary Git diff in CMake strings.
With the historical evidence deletions, the CMake process reached 3,469,700 KiB
RSS and stalled waiting on memory-backed file reads. That build was interrupted;
it is not a passing qualification run.

`tools/source_fingerprint.py` now hashes the identical tracked input as a stream,
including CMake's CRLF normalization. Untracked identity remains independent,
and archive identity is unchanged. Four tests compare against the actual former
CMake calculation, including clean/untracked state, Unicode/binary changes,
staged deletion, carriage returns and failed Git. All pass. Git itself can still
buffer large changed blobs; streaming removes the additional whole-diff CMake
copies rather than claiming Git has constant memory consumption.

The first menu application qualification could not write the host's user-data
folder under restricted execution. The complete successful rerun set
`XDG_DATA_HOME` to an owned qualification directory inside the checkout, without
modifying any personal saves or game project.

## Retired test contracts versus retained coverage

- The old contact-cache test required a positive MK1 `ContactSolver` frame count.
  Current MK2 never fills that solver instance: only its stale diagnostic reads
  remained. That implementation-specific assertion is retired, not weakened to
  accept a smaller physical accuracy. The same suite retains independent contact
  inputs, exact bound comparisons, invalidation and previous/current sweep checks.
- Import cancellation no longer requires an ignored historical Skateboarder FBX
  from `.cache/m66`. It uses the committed 256-joint source fixture and verifies
  cancellation prevents publication, including work already staged. A timing race
  is not presented as proof that a particular parser instruction was interrupted.
- The optional original-character resource handoff mode is retired. The normal
  self-contained generated 109-joint/material workload remains unchanged.
- Model runtime rendering uses the existing synthetic multipart and 256-joint
  sources instead of an ignored 142 MB consumer character. It retains actual GPU
  high-index/eight-influence skinning, material/UV/normal mapping, primitive hiding,
  independent animation ownership, secondary-camera and teardown checks.
- Project tests create an ordinary minimal authored physics scene instead of
  asserting the layout, built-in player and doors of the retired tiny/technology
  games. Project validation, asset identities, resources, undo and Play isolation
  remain current contracts.

The first focused CI run after the streaming helper failed because its miniature
source fixture did not copy the new helper. The fixture now includes that real
configure dependency; all 27 provenance positive/negative controls pass locally.
The failed run is retained as evidence, not reported as a passing gate.

## Historical-content checkpoint

Removed 62,482 historical evidence files (3,002,691,028 apparent bytes) while
retaining current V2 qualification packages. The independent gravity manifest
and 136 scenes and the contact-pair input remain as current test fixtures.
Removed 581 abandoned research files from the two prototype trees; retained
and relocated the active 3D trajectory control used by planar-physics tests.

Focused current validation passed: planar 52, uniform gravity 12,095, contact
cache 13,326, skeletal animation 34, pose composition 16, multi-target IK 38,
fracture 33, structures 32, materials 40, streaming 85, Unicode text 49, native
text rendering 48, reimport 40, native model rendering 16, world authoring 494,
resource handoff 14, and scalability/export/history 275 checks. Project contracts
also pass. These results qualify this content/fixture checkpoint, not completion
of the remaining runtime-removal batches or platform qualification.

The four water-workshop generated scenes are byte-identical before/after removal
of the old-demo generator dependency. The four borrowed terrain source textures
and their licence notice are also byte-identical to the workshop-owned copies.
Consumer projects, legacy runtime implementations and their dedicated targets
are removed in subsequent dependency-aware batches.
