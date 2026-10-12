# M78 final combined native Windows functional validation

Final native archive: `71a722dded64cfcc634d679e55ddbd257058da888ada914f7f7150b8d88c2d62` (67,019,246 bytes, 138 members). Final WorldPersistence source: `39ae0eb2132f7b5f6f43710dd82accf38ef21b04dc1c57bd8af4279da84ad2bb`.

The final follow-up compiled only WorldPersistence and the expanded continuation fixture with the recorded MSVC Release flags. Eight earlier core executions retain their original `3554…` archive and binary identities because the final correction affects persistence only. Twenty affected executions were freshly linked and run against the final `71a…` archive.

## Actual focused outcomes

| Execution | Evidence stage | Actual result |
|---|---|---|
| `controller` | retained core | 119 checks / 0 failures |
| `motion` | retained core | 10,239 checks passed |
| `foundation` | retained core | 34 checks / 0 failures |
| `physical` | retained core | 64 checks / 0 failures |
| `joints` | retained core | 29 checks / 0 failures |
| `contact_lifecycle` | retained core | 1076 checks / 0 failures |
| `impact-timing` | retained core | 89 cases / 3,197 checks / 0 failures |
| `gravity` | retained core | 70 checks / 0 failures |
| `world_save` | fresh final | 27 checks / 0 failures |
| `streaming` | fresh final | 82 checks / 0 failures |
| `balance-step-trace` | fresh final | 15 checks / 0 failures |
| `balance-standing` | fresh final | 15 checks / 0 failures |
| `balance-supports` | fresh final | 14 checks / 0 failures |
| `balance-variants` | fresh final | 17 checks / 0 failures |
| `balance-recovery` | fresh final | 16 checks / 0 failures |
| `balance-radial` | fresh final | 18 checks / 0 failures |
| `balance-lifecycle` | fresh final | 26 checks / 0 failures |
| `balance-cold-stand-write` | fresh final | 23 checks / 0 failures |
| `balance-cold-stand-read` | fresh final | 13 checks / 0 failures |
| `balance-cold-recovery-write` | fresh final | 21 checks / 0 failures |
| `balance-cold-recovery-read` | fresh final | 13 checks / 0 failures |
| `balance-exact-cold-read` | fresh final | 13 checks / 1 failures |
| `balance-new-cold-write` | fresh final | 24 checks / 0 failures |
| `balance-new-cold-read` | fresh final | 13 checks / 0 failures |
| `character-baseline` | fresh final | 98 checks / 0 failures |
| `character-cold-write` | fresh final | 10 checks / 0 failures |
| `character-cold-read` | fresh final | 14 checks / 0 failures |
| `physics-continuation` | fresh final | 98 checks / 0 failures |

**28 focused executions: 27 passes and one preserved legacy failure.** A known failure is not counted as a pass because its exit code was expected.

## Actual current physical endpoint

A separate read-only observer preserves the original 24-check write and 13-check read acceptance. It replaces no engine objects and replays no commands. Both runs pass, and the source/restored **current** endpoint objects exactly match: actual bodies, current controller progress and freshly read completed contacts. Its derived verdict does not use the cached Standing observation.

Measured native root height is 1.36187017 m, upright alignment 0.998602152, and COM relative speed 7.13618363e-07 m/s. Both feet have current support, one landing is recorded, and current progress is Succeeded with physical standing true. The same saved bytes (`cc73b426266cfa94b946317567ccbc49015594e3dc974a24ca577aa9f9fa2c6d`) were used throughout.

See [actual endpoint parity](final-current-standing/WINDOWS_ACTUAL_ENDPOINT_PARITY.json).

## Editor and moved package

The final editor relinked 15 unchanged objects whose source hashes were checked against the current host files. Edit, duplicate, undo, Play, Stop and save exit successfully; authored scene bytes remain unchanged. Two rendered screenshots and the saved scene are preserved.

The ordinary BalanceLab export reports **52,362,663 bytes, 7 assets and 4 scenes**. Final runtime `740b77d3435cf2b3f3067dc7175df46cca5caf642e6d9d88e2487cf4ab413b23` was moved to `C:\JudasValidation\M78Final2RepairMovedPackage` and launched without arguments from `C:\Windows\Temp`, with a deliberately unavailable engine root. The original 1,720-frame harness exercises four scene transitions, two save/load service operations, pause/resume and reload. Export and moved-package process both exit 0. The tested package remains on the VM.

Two explicitly recorded, unchanged VM Mesa GL adapters use ordinary NTFS file hardlinks to bound guest disk use. No source directory is linked into the package. This proves software GL functionality; physical Windows GPU, controller, audio and human validation remain outstanding.

## Remaining failures and limits

- **Original legacy a106 cold-save: 13 checks / 1 failure**, unchanged. New captures pass, including independent current-state equality. The old input is preserved, not rewritten.
- **FLU-001 remains unresolved and failed overall.** Original Linux result is 731 checks / 1 failure at 0.558701396 m/s. Preserved native Windows result is 731 / 0 at 0.419123590 m/s. Both retain the original 4-second, 0.5 m/s criterion. This platform difference does not close FLU-001; fluid implementation, tests and research are untouched.
- VM performance is functional-only per the operator's clarification. The old 56.376 ms maximum and subsequent ordinary repeats remain preserved; no further VM timing work was performed. Linux host performance acceptance is reported separately.
- No full production rerun was performed for the narrow persistence correction. Unchanged prefab, navigation, scalability and other earlier receipts remain separately scoped; old first-candidate results are not relabeled final.

## Preserved failure and recovery history

The first proper repair's M70 cold-read **14 / 3 failure** remains intact. The final generic persistence correction captures actual Full SceneBody state, excludes CharacterMotor proxies and restores static-body velocities; its new native pair passes **10 / 0 and 14 / 0**.

Two export attempts stopped before export at the 75 MiB disk-headroom guard. After lossless host archives, fresh guest/host SHA and size verification, only named obsolete generated duplicate executables, objects and libraries were retired. Sources, raw receipts, snapshots, tested packages and the current archive were preserved. The actual export completed as a separate named recovery.

Native WP compilation reports one C4456 local `body` shadow warning and two pre-existing C4244 warnings in the unchanged legacy script-slot path. The read-only observer reports C4100 for an unused parameter. No zero-warning native full-clean-build claim is made.

## Identity and boundaries

[Final machine-readable matrix](WINDOWS_FINAL_V2_MATRIX.json) includes exact receipts, source and binary identities, package file hashes and remaining failures. Full generated bytes remain in the host cache; tracked evidence uses hash-only binary references. Prior immutable summaries and fingerprints are unchanged.

Everything remains unstaged and uncommitted. No push, tag, fluid change or later milestone work was performed.
