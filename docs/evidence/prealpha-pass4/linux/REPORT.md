# Pass 4 native Linux qualification — code checkpoint 60a9217

## Scope and provenance

This receipt qualifies the committed implementation at
`60a92176adbdfb8ccf90cc1fe3a40505416b1559`. It supplements the separate refactor
and exact scheduling tests; it does not repeat their ten already-passing physics
fixtures, certify every engine capability, or represent human acceptance.
Any subsequent portability-only source change requires its own explicit receipt.

One coordinated Release `-j2` batch built the public Linux SDK and nine selected
application/integration targets in 69.036 s, with **zero compiler warnings**.
There were **34 executed commands**, all with their expected exits. This includes
one deliberately malformed script that correctly exited 1; it is a negative
control, not an unexpected engine failure.

`BUILD_INFO.json`, `SOURCE_FINGERPRINTS.json`, `BINARY_FINGERPRINTS.json` and
`FIXTURE_FINGERPRINTS.json` record the exact identity and bytes. The engine archive
SHA-256 is `6e77a9f799ade7f6ce3ad741687689b8cae6d5efa6fd82e472923749ee21b441`,
identical to the archive used in the final dense-contact comparison.
Provenance correctly reports a clean tracked tree **and 16 pre-existing untracked
source inputs**. This is a development SDK qualification, not the stricter clean
release preset. None of those inputs was silently removed or called clean.

## Functional results

The 15 additional fixture processes passed **383 assertions**:

| Actual existing fixture | Checks | Behaviour exercised |
|---|---:|---|
| Origin physics | 13 | Contact history, warm hinge, moving platform, invalid-origin rejection |
| Origin assembly | 46 | Driven wheels/powertrain, CharacterMotor, particles, rotated frames, save restoration, navigation and negative retired-chart queries |
| Origin streaming | 23 | Retained wheels, powertrain, kinematic trajectories, deformable nodes and physical fracture sections across unload, origin change and reconstruction |
| Composed lifecycle | 34 | Asynchronous terrain collision/navigation publication, pending-save rejection, last-good rollback, actual tunnel geometry, owner destruction |
| World persistence | 27 | Momentum, support references, pending forces, first resumed contacts, moving support, spring hinge, malformed/missing/newer participants |
| Particle application | 11 | Ordinary asynchronous runtime, fixed-step emitters, secondary camera, actual render submission and shutdown |
| M70 ordinary application | 98 | Real imported rig, shared IK, physical authority, owner collision delivery, lifecycle and rendering |
| M70 fresh-process writer / reader | 10 / 14 | Partial physical authority, durable script state, three real resumed steps and finite final pose |
| M77 in-progress step writer / reader | 24 / 13 | Genuine corrective step, preserved clocks, fresh native reconstruction and measured standing endpoint |
| M77 standing writer / reader | 23 / 13 | Standing continuation across two independent OS processes |
| M77 recovery writer / reader | 21 / 13 | In-progress recovery and the real restored endpoint after 1,080 unchanged steps |

M77 uses `judas_balance_lab_tests`, not the M71 Kinematic Lab. Original controllers,
fixtures, 60 Hz timestep and thresholds are unchanged. `Standing` verifies upright,
height, speed, both actual foot contacts and dynamic-body authority, rather than
accepting a cached progress enum. The fresh-process read logs measured:

| Save case | Actual upright | Height (m) | Speed (m/s) | Foot contacts | Landings |
|---|---:|---:|---:|---|---:|
| Corrective step | 0.998586 | 1.3616 | 0 | 4 / 4 | 1 |
| Standing | 0.999934 | 1.41828 | 0.00000195818 | 4 / 4 | 0 |
| Recovery | 0.99999 | 1.24952 | 0 | 4 / 4 | 0 |

The original legacy snapshot and its missing historical continuation limitation
are unaffected. This receipt establishes the existing **new-format** contracts.

## Original four-ragdoll performance gate

The byte-identical frozen M78 observer was compiled against the current archive.
All four original 900-step cases ran once at 60 Hz. Windows was suspended and
other coordinated heavy work held during this measurement, then immediately
released. No mesh, rig, contact, CCD, cadence, solver budget or acceptance threshold
was changed. All bodies remained finite, above the original floor envelope and
produced actual contacts.

| Workload | Median (ms) | p95 (ms) | Worst (ms) |
|---|---:|---:|---:|
| One 13-body rig, box floor | 0.172517 | 0.282483 | 2.147408 |
| Four rigs, box floor | 0.714616 | 1.308574 | 9.217564 |
| One rig, 8,192-triangle floor | 0.247539 | 1.480369 | 10.899206 |
| Four rigs / 52 bodies, same mesh | 0.652534 | **4.814678** | **15.280967** |

The existing four-rig p95 **16.67 ms** gate passes; 0 / 900 steps exceeded it.
Two original bounded CCD-search-limit occurrences remain recorded in the four-rig
mesh case. Passing this workload does not resolve the separate 1,024-piece ~1.9 s
tail or curved-mesh crossing defects. No historical before/after speedup is claimed
from this single current-source qualification.

## SDK, editor and relocated package

The inventory contains **17 Linux executables**; Windows adds its eighteenth
readiness executable. All 17 agree on build identity and pass checksum, ELF
architecture and native dependency-loader checks. Actual required ABI maxima are
glibc **2.43**, GLIBCXX **3.4.32**, CXXABI **1.3.15**. This proves this host's
compatibility, not older Linux distribution support.

The staged SDK tools executed real script compile/link, a syntax-error negative
control, six-part character inspection, collision cooking, mesh/LOD cooking,
save-content identity, and volumetric create/remove/validate/cook operations.
The edited volume cooked 214,191 bytes through ordinary project authoring.

The **staged SDK editor**, on a private copy of the authored Physics Stress Lab,
performed batch edit/undo, duplicate/undo, Play/Stop and scene save. All five explicit
workflow assertions pass and authored state after Play/Stop is identical.
The **staged SDK exporter** built the normal project with its four declared scenes.
Its package was copied to `/tmp/Judas-PreAlphaPass4-Physics-60a9217`, launched from
unrelated working directory `/tmp`, executed the project watch action for 90
harness frames, captured a screenshot and exited normally. Export provenance,
asset closure, runtime hash and screenshots are retained.

### Graphics and interaction boundary

Actual SDL/OpenGL application execution reports Mesa 26.0.8 **llvmpipe** software
GL 4.5. The editor logs that relative mouse mode is unavailable in this automation
environment. Automation validates application/render/lifecycle behaviour, **not**
physical GPU performance, desktop pointer capture or human usability. The harness
run intentionally hides its window; a human launch must omit `JUDAS_TEST_SCRIPT`.

## Reproduction and preservation

`RESULTS.json` contains each exact argv, working directory, selected environment,
UTC timestamp, exit, duration and log/executable hashes. `reproduction/` preserves
the runner as executed from `.cache/prealpha-pass4/linux/run_final.py`; copy it back
to that path in a checkout of 60a9217 before running its `build`, `performance`,
`functional` and `packaging` stages in that order. Use fresh owned output folders;
the application fixtures deliberately refuse to reuse a writer's existing project.
The four-rig source is the unchanged
`docs/evidence/m78-followup/probes/concave_final.cpp` with pinned SHA-256
`f0ce020eb47613ae82bb2619cdc03e6b477782b744576be2b9a36a52f5079136`.

No engine, game, original test, acceptance threshold, protected history or fluid
research file was edited by this qualification. All 101 relevant authored fixture
files were fingerprinted. Builds, SDK binaries, private projects and save binaries
remain outside the tracked evidence. Raw profiles, per-step CSVs, endpoint records,
logs and manifests are preserved here.
