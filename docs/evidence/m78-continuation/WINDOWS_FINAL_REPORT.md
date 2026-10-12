# Native Windows final combined M78 validation

Immutable receipt summary for archive `791edc06d69e9a0c750c2bf23f91b20936ed5c0d530f20a936eed0c544c9ba48`. HEAD remains `9e77b2226edcf217ab789fe4f0fea42a3415a72d`. All production-source inputs in the 218-file native manifest still match. Nothing committed, pushed or staged by this validation work.

## Results

- Focused matrix: **49 executions, 48 program passes, one real cold-save failure**. Timing gates are reported separately; program exit 0 is not an all-gates pass.
- Original M77 controller 119/0 and actual public-JS corrective step 15/0 pass. Rigid motion 10,239, contact lifecycle 1,076, geometry 345, M70 physical animation 64, joints 29, ragdoll 36, contacts/cache, CCD 89 cases / 3,197 checks, gravity, world saves, CharacterMotor/application/lifecycle, retained prefab/streaming/scalability/navigation checks pass.
- In-progress corrective-step cold write 24/0; original 720-step cold read **13/1**. Standing and recovery cold-save pairs pass. Fresh and exact-original restored snapshots repeat the failure; uninterrupted exact source state reaches standing.
- Read-only causal probes preserve identical save bytes/state/commands. Actual-command-only overlay still fails. Suppressing only next-solve warm seeds in an uninterrupted source also fails; first solve has zero contact rows. Smaller projected impulse residual does not establish successful recovery. **No speculative save/controller correction was retained.**
- Unchanged FLU rotated-half native witness 731/0 at 0.419123590 m/s. Linux remains 731/1 at 0.558701396 m/s versus original 0.5 m/s at four seconds. **FLU-001 remains unresolved/failed overall; no fluid source, input, threshold or research changed.**

## Original ragdoll workload

| Native workload | Median ms | p95 ms | Max ms | >16.67 ms | Asleep |
| --- | ---: | ---: | ---: | ---: | ---: |
| box-1 | 0.3377 | 0.4907 | 7.5172 | 0/900 | 13/13 |
| box-4 | 1.354 | 2.2685 | 30.7227 | 2/900 | 52/52 |
| mesh-1 | 0.5336 | 0.9817 | 20.8192 | 1/900 | 0/13 |
| mesh-4 | 1.8522 | 10.839 | 56.376 | 37/900 | 26/52 |

Mesh-4 retains 52 original bodies, 8,192 triangles and 900 steps at 60 Hz, normal contact/CCD fidelity. Median <8 ms and p95 <16.67 ms pass; **maximum <50 ms fails at 56.376 ms**. Seven bounded CCD limits/fallbacks remain. Worst step 85 has 285 queries / 830 iterations; solver 54.2711 ms of 56.376 ms. P95-tail solver share 91.593%. Native VM timings are distinct from Linux; no invented cross-platform speedup. Raw CSV and profiler traces are preserved.

## Application/package and environment

- Fresh runtime `ea39694571f4e8dbf0322cd6f5c216cd97437104c4a7432af4100b2c1ac326bf` exported normal registered Balance Lab, moved to `C:\JudasValidation\M78ContinuationMoved-combined-v2`, launched with no arguments from `C:\Windows\Temp` and unavailable engine root. Four scenes/save/load/pause/resume/reload smoke passed, exit 0. Package file/DLL hashes are in the exact receipt.
- Initial default-driver exit 255 / WER and missing-guest-fixture failure remain preserved. Same binaries passed after explicit established `GALLIUM_DRIVER=llvmpipe` and staging the byte-identical protected fixture. These were environment/input repairs; no runtime change.
- Native /W4 C4456/C4244 compilation warnings, initial duplicate-header/empty-library-log tooling failures and host driver syntax error are retained. Do not claim zero native warnings or a new full clean whole-engine build.
- VM remains 4 vCPUs / 6 GiB / 40 GiB; only independently archived owned generated files/temp transfer parts were removed. Libraries, source, review packages, original evidence and protected fixtures remain. **Physical Windows GPU/controller/audio and human validation remain outstanding.**

## Audit paths

- [Complete per-case/submode outcomes, source/binary hashes and raw evidence inventory](WINDOWS_FINAL_MATRIX.json).
- [Native final library identity and independently verified host copy](windows-final-native-library.json).
- [Exact final native executable identities](windows-final-native-binaries.json), plus [controller/motion executable identities](windows-final-native-extra-binaries.json).
- Raw receipts use `m78-combined-v2-*`; read-only mechanism probes use `m78-diagnostic-warm-v1-*`. They are distinct from the final production archive.
