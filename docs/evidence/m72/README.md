# M72 pre-checkpoint candidate evidence

Base: `02b540f083bcf3d99b0dc6eb9aa70e63c91a31c3` (accepted M71). This record was captured before checkpointing, while M72 was uncommitted and awaiting operator review. The operator subsequently authorized commit/push **without human validation**. Current release status is in `docs/M72.md`; these original automated receipts do not establish human acceptance. No later milestone work.

## Executed scope

GNU 15.2.0, CMake Release `-O3 -DNDEBUG`, Linux x86-64. One fresh `.cache/m72-release` build plus necessary affected incremental corrections. Final builds have zero compiler warnings. Existing dependency configure messages include HarfBuzz's CMake-port notice and FreeType's CMake compatibility deprecation. No full production-suite repeat. SDL offscreen, Mesa llvmpipe LLVM 21.1.8 software GL, dummy audio. The benign EGL forced-software warning and unavailable offscreen relative-mouse warning are retained. These results do not validate desktop input feel, physical audio or native Windows.

| Check | Result | Receipt |
| --- | --- | --- |
| Actual colour/depth/shadow and independent main/aux rendering | 54/54 | `development/render-controls-02.log` |
| Public VM controls, atomicity, status/failure, hidden solid/script/socket, resource retirement, snapshot | 40/40 | `development/application-verified.log` |
| Fresh-process snapshot reconstruction + original-default reset | 13/13 | `development/fresh-process-verified.log` |
| Named/scene/prefab/undo and legacy/default compatibility | 28/28 | `development/authoring.log` |
| Real M59 retained unload/revisit and resource demand rebuild | 5/5 | `development/retained-region-01.log` |
| Every-frame ordinary application and actual logical controls | 4/4 | `development/live-app-verified.log` |
| Material/PBR/cold primitive/shadow sampler regression | 44/44 | `development/materials-final.log` |
| M57 integration including retained M55 optical/accounting behavior | 36/36 | `development/material-integration-final.log` |
| Secondary camera resource/render/lifecycle regression | 52/52 | `development/cameras-final.log` |
| Async integration | 12 cases, 246/246 | `development/async.log` |
| M66 original-model runtime / M46 skinning | 23/23; 34/34 | `development/model-runtime.log`; `development/skinning.log` |
| Historical schema-5 fingerprints | 140/140 | `development/fingerprint.log` |
| Editor widgets / geometry picking | 15/15; 36/36 | `development/editor-widgets.log`; `development/editor-picking.log` |
| Save storage / resource upload handoff / generic export | 28/28; 9/9; 18/18 | corresponding development logs |
| Live public export/member enumeration vs inventory | 321 symbols, 224 native operations; exact match | `development/api-live-comparison.json` |
| Current material cookbook | 9/9 | `development/cookbook-materials.log` |
| Ordinary editor Undo + Play/Stop | Authored scene IDENTICAL, save succeeds | `development/editor-smoke.log` |
| Source-hidden relocated standalone / real save-load | 260 scripted frames, controls/captures/save/load complete | `development/moved-package-final.log` |

The 144 focused M72 assertions include setup/lifetime checks. Other rows are affected existing coverage, not a claim that every production subsystem was retested. Editor runtime-preview Undo/Redo remains a human review item.

## Render and performance observations

Independent expectations and probe tolerances are embedded in `tests/RuntimeRenderTests.cpp`: RGB tolerance normally 4–5/255; no-blend-depth delta <=1e-6; cutout/hidden shadow clear depth >0.9999. Source/neighbour isolation is checked directly. Directional floor shadow probes reverse from 8/236 to 236/8. Six unedited PNGs and the bounded M56 capture are in `render-controls/`.

See `render-controls/README.md` for 128 shared instances: default CPU mean 3.5640 ms, changing mean 2.9561 ms with fewer visible objects (not a speedup claim), zero steady mesh/texture uploads, fixed cache bytes, and 16 created/16 destroyed replacement textures. Cold preparation was 1.5242 ms with uncontrolled OS caches.

`application/performance.json` derives only completed ordinary every-frame application samples, not the screenshot-only test harness. Software GL at 1024×768 main and 384×216 every-frame camera: outer median 31.736 ms, p95 35.828 ms, max 62.486 ms; fixed median 0.0597 ms; combined JS phase median 0.3528 ms. Resolved GPU main/shadow/aux medians 23.914/1.023/5.071 ms. 180 live frames, latest 119 standalone samples retained; eight asynchronous GPU records remain pending. Profiler diagnostics are zero. One rigid body; after hiding three objects, 58 draw calls / 20,172 triangles. Startup was 320.937 ms. Software timings do not guarantee hardware FPS or eliminate M68 first-use upload hitches.

The screenshot-only `application/lab-profile.json` includes save/load resource reconstruction stalls (maximum 438 ms). Its mostly unrendered simulation frames are not an FPS benchmark. Grouped public lighting/material/visibility writes took 7.216 ms for 10,000 groups in the final integration run; this measures publication only, not total rendering.

## Package

Current source: `projects/render_control_lab/render_control_lab.judasproj`.
Export: `/tmp/judas-m72-moved-render-lab-20261009/judas` (33 registered assets, one scene, 53,147,870 bytes). Export used a copied current source at `.cache/m72/export-source`, subsequently renamed `export-source-hidden`, and the package was moved. Final launch CWD was `/tmp/judas-m72-unrelated`. The original project and user libraries stayed intact. Package startup resolved its own assets; logical controls, hidden collider/socket/script, screenshots and save/load ran through ordinary APIs. Runtime switches use declared texture asset roots. Corrupt image fixtures stay in owned test cache: normal export correctly rejects corrupt assets.

## Preserved development failures

- Initial build warnings and a requested nonexistent resource-test target: `development/build-01.log`; indentation corrections and corrected target list are in follow-up builds.
- Unknown appearance-baseline field accepted, then rejected by the strict versioned decoder; a repeat-run test metadata-registration error was corrected: `authoring-01.log`, `authoring-02.log`, final `authoring.log`.
- Incorrect fixture scene syntax and incomplete required legacy fields: `application-01.log`, `application-02.log`; corrected ordinary scene parser path passes.
- Render test assumed a different existing saturated-colour display resolve; resource wait preceded the request on repeated retirement: `render-controls-01.log`; independent channel/neighbour expectations and request/wait order fixed in final run.
- Fresh snapshot was inspected before script reconstruction at its normal resume boundary; a whole-target clear was incorrectly expected to remove a distinct part patch: `fresh-process-01.log`; corrected expectations pass.
- Material test invocation used nonexistent `material_demo` environment path, then indexed the unsuccessful fixture load and segfaulted. `materials-01-wrong-fixture-path.log` preserves this runner failure. Correct `material_lab` invocation passes all 44.
- Invalid harness service names `all`/`resources` and first moved-package run's repository CWD: startup01/02 and moved-package01 logs. Final scene/save service names and unrelated CWD are recorded.
- First live test injected input before normal BeginFrame reset. `live-app-01.log` preserved; ordinary queued synthetic input in test mode fixes the test without runtime changes. Final every-frame run passes.
- Newly appended retained-region target needed a CMake reconfigure: retained-region-build01; final build02 passes.

There is no fabricated physics failure or protected evidence rewrite. Existing FTFT/P1/M33–M71 evidence, prototypes, simulation/fluid implementation and user asset libraries remain unchanged.

## Remaining verification boundary

The runtime/source inventory and real VM enumeration match. Standard TypeScript AST/declaration/example checking was not executed: no installed checker was found, and automatic approval rejected a temporary download under the prompt's explicit-dependency-agreement rule. The permission question remains unanswered. No dependency was added. Native Windows M72 and desktop visual/input/editor preview acceptance remain outstanding.

See `commands.md`, `changed-files.txt` and `final-source-sha256.txt` for reproduction and final scope. Read `docs/M72.md` and the project README for exact public contracts, reset/fallback semantics, limitations and human controls.

## Checkpoint status-only follow-up

Before staging, all 123 candidate source/asset fingerprints and 95 receipt fingerprints matched. Operator authorization then changed only current documentation status to checkpointed without human validation; implementation, tests and assets did not change. The manifests were refreshed for those explicit documentation edits. Original execution logs, failures and captures remain unchanged.
