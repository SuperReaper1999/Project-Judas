# Native Windows functional follow-up

The final production archive remains `791edc06d69e9a0c750c2bf23f91b20936ed5c0d530f20a936eed0c544c9ba48`. No production source or numerical setting was changed during these native runs. The VM is used for functionality, not performance certification or physical-GPU validation.

## Results

Twenty-four fresh focused executions completed: 23 passes and the unchanged in-progress corrective-step cold restore remains **13 checks / 1 failure** on exact snapshot `a106ce212416a510ec0470d2cf7abe0fc96fccdfec202c95e5209c9e56c4379b`. Thresholds, inputs and 720 resumed 60 Hz steps are unchanged. This is not labeled a pass.

Passing reruns include controller **119**, rigid motion **10,239**, contact lifecycle **1,076**, all **89 CCD cases / 3,197 checks**, gravity selection **70**, persistence **27**, physical animation **64**, joint **29** and balance foundation **34** checks. Actual Balance Lab corrective-step, standing, supports, variants, recovery, radial and lifecycle paths pass. Ordinary standing/recovery cold-save pairs pass. Character Lab baseline **98** and both cold-save processes pass.

The fresh native Editor target was built from its 15 unchanged source units using the actual retained MSVC recipes and linked against the final archive. Ordinary copied-project Edit/duplicate/undo/Play/Stop/save smoke passed. The authored scene remains byte-identical (`e7af42515a537284efd5d56d1b29073117f39090a6da368e88f24b7acd181fb2`); resources/jobs report no failures. A compiler-path tooling failure is preserved: mixing two copies of a header defeated MSVC `#pragma once`; a canonical copied header tree corrected only the test build arrangement, with six successful objects retained rather than rebuilt.

Fresh moved standalone smoke passed without arguments from `C:\Windows\Temp`, with deliberately unavailable engine root and the original 1,720-frame four-scene/save/load/pause/reload harness. Exact final runtime SHA is `ea39694571f4e8dbf0322cd6f5c216cd97437104c4a7432af4100b2c1ac326bf`; package file identities were verified before launch. Together these are **26 executions: 25 passes and the preserved cold-step failure**.

## Exact boundary investigation

The native passive row journal reproduced the exact same `a106...` save bytes for all three controlled captures. Uninterrupted source continuation reaches standing; ordinary cold restore fails; suppressing only the first initial joint warm seeds in the diagnostic copy also loses standing. Each has first-boundary row/state/command records and a full 720-step trace. No warm caches were restored, no acceptance values changed and no diagnostic overlay was retained in production. The independent solver analysis belongs to the accompanying cold-boundary report; this result alone does not establish a safe production correction.

A subsequent strict-guard native closure experiment preserved the exact a106 snapshot, 720 resumed steps at 60 Hz and original 13 checks. Replaying the captured first-boundary commands plus numerical seeds passes 13/0; the same experiment with a copied normal first controller Prepare also passes. Splitting those contributions with normal Prepare is decisive: **commands-only fails 13/1; seeds-only passes 13/0** and actual current bodies reach standing. Body/joint geometry equality guards pass before any diagnostic replay. All later algorithms are unchanged. This identifies sensitivity to disposable initial solver history; it does not justify serializing warm caches as physical state. These four copied-code runs are reported separately from production validation, and no overlay is retained in production.

## Evidence

- `WINDOWS_FUNCTIONAL_FINAL_MATRIX.json`: final aggregate with each production execution, separate diagnostic-only outcomes and explicit supplemental-build scope. Earlier matrices remain unchanged.
- `native-closure/` and `native-split/`: strict-guard first-boundary receipts, all first ten physical states, full traces, actual final physical state, and losslessly compressed linker maps with hashes. Explicit replacement objects are linked ahead of the unchanged final archive; `/VERBOSE:LIB` and maps prove the corresponding original archive members were not extracted.
- `native-functional/`: stdout/stderr, original thresholds, editor screenshots, compile/link/tooling failure and corrected smoke receipts.
- `m78-diagnostic-boundary-v1-pair.json` and the three passive `joint-boundary.jsonl` files: exact native a106 pair and isolated first-joint-seed observation.
- Original native FLU witness is unchanged in `../m78-continuation`: Windows 731/0, Linux 731/1; FLU-001 remains failed/unresolved overall. Fluid code was not changed or reinvestigated here.

All previous native/Linux failure records and final fingerprints are preserved. Current evidence and owned generated binaries were archived/hash-verified before disk recovery; source, results, final production archive and moved package remained intact.

## Supplemental native build scope

The preserved extra MSBuild navigation/spacecraft outputs passed their own tests, but read-only source identity shows their guest tree retained older first-candidate physics. They are **excluded from fresh final combined counts**. The earlier independently verified direct combined-v2 native spacecraft, prefab, streaming and navigation tests remain valid on the unchanged final archive and source; their exact receipts are mapped in `WINDOWS_SUPPLEMENTAL_SCOPE.json`. The extra PowerShell `if` tooling failure is preserved separately. No source or runtime correction was made to erase these records.
