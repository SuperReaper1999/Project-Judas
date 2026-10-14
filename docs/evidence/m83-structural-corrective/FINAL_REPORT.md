# M83 practical crash-section corrective checkpoint

14 October 2026. Starting Experimental: `694ffa97c71717aaf2f62ba28eea4a2866deb844`.
Linux-validated development candidate; human acceptance and Windows validation pending.
This report supersedes the frozen research status documents for current runtime identity.

## Decision and implementation

The dense globally implicit node/beam crash model was unsuitable for this game workload.
It is archived, not compiled into the runtime. Its observed one-car p95 was 336.980 ms,
with a 556.020 ms maximum and a preserved failed 1754.785 ms step. These were different
research workloads; do not claim a matched numerical speedup or validation of that model.

Current cars use five actual physical bodies: a 336 kg hollow cabin/rear core and four
60 kg front sections, total 576 kg. Twenty dynamic section bodies, sixteen M81 wheels and
four M82 powertrains comprise the four-car workload, plus ordinary environment collision.
Every section remains physical before impact; an impact activation/mass-transfer shortcut
was unnecessary. Roof and pillars preserve a recognisable core. This is piecewise-rigid
local crush, not continuous sheet-metal folding or arbitrary mesh cutting.

M79 partition ownership and normal PhysicsWorld contacts/CCD are reused. Fixed connections
can opt into finite six-axis Kelvin–Voigt elasticity, plastic return mapping, hardening,
positive plastic work and finite strain capacity failure. Translation and rotation rest
history changes actual section geometry. Front wheel mounts follow those physical bodies;
physical graph connectivity determines whether the corresponding powertrain link transmits
power. JS driver commands cannot reconnect a physically broken link. No prescribed crush,
teleport reset, decorative debris, scene-name engine branch or gameplay controller was added.

The material schema is optional and defaults preserve legacy content. The lab uses two
native intervals with twelve rounds each at authoritative 60 Hz. Explicit local-neighbour
and intact-support filtering prevents overlapping rigid proxies fighting their connections;
remote self collision remains, and detached components regain collision. Configuration,
actual body poses/motion, plastic history and joint continuation are persisted. Streaming
remaps owned generated bodies under fresh identities and keeps staged bodies disabled until
publication. The final stream defect was private restore code wrongly calling the public
published-only body resolver; the public lifetime boundary remains unchanged.

## Meaningful observations

- Normal driving remains elastic, without permanent yield.
- Frontal and offset impacts create retained strain; offset crush is asymmetric.
- The same mass/shape under weak, nominal and strong material changes its outcome.
- Nominal 15 m/s impact retains approximately 0.199 m plastic travel without breakage;
  weak material approximately 0.235 m breaks; stronger material approximately 0.156 m remains intact.
- 25 m/s impact retains approximately 0.234 m and breaks two front connections per car.
- Actual high-impact application checks prove disconnected front driveline links and wheels
  still mounted on their corresponding real section, including exported four-car execution.
- Generic cantilever: 0.010174 m elastic peak, 0.001489 m recovered displacement, zero early
  plastic strain, then retained 0.600391 rad strain and one failed connection under overload.

## Final Linux checks and honest performance

`coarse-final/closure-linux/RESULTS.json`: normal, low, medium, high, offset and both four-car
cases passed. Comparison, oblique-gravity and corrected cantilever passed in their linked
final folders. 27 section-material checks passed. Final joint, fracture, M79 structural,
M80 bending, wheel, wheel lifecycle, powertrain and mechanical-connection checks passed.
Earlier contact/geometry focused regressions also passed; no unrelated production marathon.

Fresh-process save read compares the actual captured physical endpoint exactly, then runs
120 continuation steps. Prefab publication, missing-part negative control and reload pass.
Stream suspension retires the old family, restores exact geometry/plastic/drivetrain state
under fresh IDs, then continues normally. Final write/read/lifecycle/stream all passed.
Types, API manifest, three structured result-shape checks, four negative drift controls,
42 example syntax/type checks and actual VM enumeration passed (32 exports, 252 operations).
Editor duplicate/undo and Play/Stop preserve the authored scene exactly.

| Workload / retained run | Physics p95 impact | Physics maximum impact | Frame-work p95 impact |
|---|---:|---:|---:|
| One nominal car, initial final matrix | 1.133 ms | 1.332 ms | 2.445 ms |
| Four cars at 15 m/s, final matrix | 6.569 ms | 8.716 ms | 8.987 ms |
| Four cars at 25 m/s, final matrix | 5.792 ms | 8.691 ms | 7.956 ms |
| Four cars, final explicit mechanical proof | 7.293 ms | 10.634 ms | 9.461 ms |
| Exported four cars, final mechanical proof | 8.219 ms | 11.107 ms | 11.146 ms |

The declared four-car p95 physics <=10 ms and frame-work <=16.67 ms goals pass. These are
host workload measurements, not universally guaranteed 60 FPS. The later single-car
mechanical-proof run passed behaviour but failed its tighter <=3 ms physics goal:
approach/impact/damaged p95 3.401/3.609/3.700 ms, while frame-work p95 remained <=7.841 ms.
The final repeat also missed that target (impact 3.615 ms, damaged 3.860 ms), while its
frame-work p95 stayed <=8.111 ms. Both failures remain; no budget was relaxed or favourable
samples selected to erase them. Save-mode captures also have additional host/resource work and are not
reported as identical performance workloads.

All maxima remain in raw receipts. A comparison run had a 21.407 ms approach physics spike;
exported four-car approach maximum was 17.956 ms. Engine frame maxima can exceed one frame.
Synchronous PNG capture in the harness contributes ~48–58 ms to a captured frame, separately
from fixed physics. Thus this is practical measured headroom with occasional tails, not a
claim of uninterrupted 60 Hz presentation on all machines. The rejected seven-body severe
four-car case had impact p95 24.114 ms/max 43.296 ms; bounded profiler captures attributed
most of its slow steps to native continuous-impact contact work. Reducing the actual
representation complexity, not lowering collision fidelity or cadence, addressed that cost.

## Export, reproduction and retained failures

Current project: `projects/coarse_crash_lab/coarse_crash_lab.judasproj`.
Current package: `/tmp/judas-m83-coarse-final-package`; moved read-only standalone:
`/tmp/Judas_M60_M83CoarseFinalAuth20261014`. Export includes eleven assets and eleven scenes.
The unmodified moved binary launches from unrelated `/tmp` and closes normally. Editor,
API, package and physical four-car results are under `coarse-final/closure-linux`.
One helper failed with a stale desktop authorization file; its failure and corrected smoke
are retained. The exporter names its project `game.judasproj`; use that name when reproducing.

Build normal Release targets `judas`, `judas_editor`, `judas_export`,
`judas_section_material_tests`, `judas_coarse_crash_application_tests`.
Run:

```sh
build/judas_section_material_tests
build/judas_coarse_crash_application_tests projects/coarse_crash_lab/coarse_crash_lab.judasproj group_high watch /tmp/m83-four-check
build/judas_export projects/coarse_crash_lab/coarse_crash_lab.judasproj /tmp/m83-package build/judas
node scripts/check_judasjs_api.mjs --typescript /path/to/existing/typescript --runtime docs/evidence/m83-structural-corrective/coarse-final/closure-linux/cookbook/surface.json
```

`tools/CreateCoarseCrashLab.py` reproduces the authored assets with the normal baker.
The numerical law, units, authoring grammar and controls are in `docs/M83_COARSE_CRASH.md`.
`preserved-investigation.zip` preserves 4807 original paths using deduplicated content blobs.
Its embedded MANIFEST maps each exact path to SHA256/blob; extraction reconstructs each path
by writing that blob. Executables, libraries, build outputs and caches are excluded.
Frozen implicit source and native prototype receipts are also directly available in
`architecture-reassessment`. Original failures include node stalls/continuation, early
contact filtering, wrong input injection, invalid startup content, specimen overlap/load,
streamed restore, and the seven-body cost gate. They have not been rewritten into successes.

## Limitations and acceptance

Componentwise small-rotation plasticity; coarse authored rigid pieces, not industrial FEM.
No internal deformation of individual sections, arbitrary cutting, fatigue or thermal model.
Capacity may be exceeded within one physical interval; positive plastic work precedes failure
in that same constitutive update, with no artificial frame delay. Collision/mass distribution
and chosen section topology matter. Existing nodal/lumped section inertia remains approximate.
No sleep/impact-only activation optimization or universal performance certification.
Windows not revalidated. No human acceptance inferred. Main remains unchanged.
Protected history, accepted fluid and independent fluid research remain untouched.

## One human checklist

1. Open the lab; V runs one case, T tours; W/S, A/D, Space drive/steer/brake.
2. Confirm normal driving stays intact; low/medium/high impacts change the actual bodywork.
3. Try offset and weak/nominal/strong cases; compare local deformation and failure.
4. After severe damage, inspect wheel alignment, retained crush and lost drive.
5. Run the elastic/plastic specimen and rotated-gravity case.
6. Open group/group_high and watch four simultaneous real cars; check responsiveness.
7. Spawn a prefab (P), save/load (F5/F8), stream annex (F3), reload (R).
8. Repeat the key cases in the standalone; review visual recognisability and limitations.

Checkpoint readiness: practical Linux functional and four-car acceptance goals pass;
tighter single-car budget variability is explicitly open. This is a development checkpoint,
not human-accepted release promotion. No main change, tag, unrelated deferred-fluid work or M84.
