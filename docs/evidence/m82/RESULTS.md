# M82 — mechanical powertrain results

Starting checkpoint: `b74906350084ab20d01b09774dfc56711b4d97cb` (accepted M81).
Work is on **Experimental**. Human validation is pending; Stable/main are not
promoted by these results. Linux is the automated acceptance platform. The
Windows VM was shut off during this delivery; M82 Windows validation remains
unperformed and informational/nonblocking.

Linux candidate checks, editor smoke and moved-package checks have completed.
This is scoped automated evidence, not comprehensive vehicle certification or
operator acceptance.

## Scope and engineering decisions

Existing M81 wheels, ordinary rigid angular inertia and M45 hinges were reused.
The missing capability was a generic finite-inertia shaft network. M82 adds
named internal/wheel/body shafts, bounded combustion/electric/custom sources,
ideal gears, finite-capacity clutches, open differentials and mechanical brakes.
Actual endpoint inertia and bearing reactions enter the effective inverse-mass
matrix; wheel/body inertia is not duplicated. A bounded active-set constraint
solve runs before wheels and after their loads, with one source integration and
one shared clutch/brake capacity budget per fixed interval.

Driver inputs, starter/idle policy, shifting, camera and watch tour belong to
the ordinary project JavaScript. No vehicle type, wheel count, car controller
or scripted RPM display was added to the engine.

See [architecture and limitations](../../M82_POWERTRAINS.md),
[project controls](../../../projects/powertrain_lab/README.md),
[public API](../../judasjs/powertrains.md) and [reproduction](REPRODUCE.md).

## Preserved failures and corrected follow-ups

### Static machine HUD

The first flat application run completed 15 checks with two failures. Selecting
the shaft machine made the HUD request `velocity` from its **static stand**.
The existing public API correctly threw `TypeError: entity has no active dynamic
body`; subsequent mechanical-load/braking checks failed. This was project code,
not missing physical motion or a binding defect.

The controller now reads linear velocity only for wheeled dynamic assemblies.
The stationary machine displays its actual flywheel RPM from `powertrain.state`.
The corrected flat run passes all 15 checks, including real flywheel load and
braking. Runtime velocity semantics and numerical thresholds were unchanged.
The original log is [preserved](development/static-machine-hud-failure.txt).

### Streamed durable state versus derived observation

The first real region unload/revisit completed 11 checks with one failure in
exact retained powertrain bytes. Diagnostics independently compared each shaft's
absolute spin, relative angular velocity, angle and initialization flag, plus
every source/link/brake command. Commands, rotor momentum, angles and all other
shaft observations survived. Only the derived crank angular-velocity observation
changed: `65.930452166526749` before versus `113.7355322977528` after publication.

The fixture intentionally changed durable absolute spin to `113.75` just before
capture. Its preceding observation still described the earlier rotor state;
fresh publication correctly derived relative speed from the new spin and the
actual bearing. Persisting both independent momentum and a derived observation
created a misleading competing state representation. The generic correction
removes derived angular velocity from the new M82 durable record; it is rebuilt
from internal momentum, wheel spin and actual rigid-body/bearing motion. Angles,
initialization and held commands remain durable. The exact byte-equality check
and deliberately independent durable-state input remain unchanged.

The original [failure](development/stream-retention-failure.txt),
[diagnostic run](development/stream-cache-diagnostic.txt), and
[paired field/binary fixtures](development/stream-cache-diagnostic/retained-state.txt)
are retained. These `.bin` files are small numerical save fixtures, not programs
or generated executable artifacts. The corrected actual unload/revisit now
passes all 11 application checks without changing the exact equality test.

### Real cold-load optional participant preflight

The valid write application captured an ordinary powertrain save, but its first
separate-process load was rejected with the existing misleading diagnostic
`deformable participant does not match snapshot components`. SaveService's
optional-component preflight still counted the pre-M82 participant set: it did
not recognize `powertrains`, despite the writer and WorldPersistence supporting
the new component. The generic repair recognizes powertrain component presence,
matches it to participant presence and includes it in the bounded schema count.

The [actual loader failure](development/save-loader-participant-failure.txt)
and its exact [rejected snapshot](development/save-loader-rejected.save) are
preserved. Final application controls also submit missing and phantom
powertrain-participant snapshots through the **real asynchronous SaveService**,
requiring rejection before replacement of the healthy published world. The
positive fresh-process restore passes eight checks, including real resumed
flywheel acceleration. Missing and phantom cases each pass ten checks with the
exact diagnostic and original-world continuity.

### Qualified reference names

The new mechanical qualified-reference parser initially split at the first
colon, although authored shaft names permit colons. It now uses the final colon
to separate the `target`/`bearing` field from the full shaft name. Independent
validator controls and an actual asynchronous two-region fixture exercise a
`crank:main` shaft bound to a moving foreign bearing. A nonexistent shaft fails
without publishing a graph or changing unrelated root physics. Ordinary local
references remain an independent positive control. The current garage retains
its working ordinary local-ID graph; this repair does not change game policy.

### Native fixture compile error

One new lifecycle fixture initially omitted the existing required sixth
argument to `RestoreRegionObject`. The compiler rejected that test source.
The fixture call was corrected; no runtime API was changed to accommodate it.
The actual [compiler failure](development/lifecycle-publication-fixture-build-failure.txt)
remains available.

### Reused registration fixture

One follow-up reused the previous script fixture directory, whose sidecar
metadata had already registered the probe during `AssetDatabase::Scan`. Its
explicit new-asset `Track` check therefore failed. A fresh directory passed all
17 checks. The [failure](development/reused-script-fixture-registration-failure.txt)
is retained; this changed no runtime import semantics. The follow-up helper now
chooses a fresh temporary script fixture for each reproduction.

## Completed Linux receipts

The Release configuration built the engine, runtime, editor, exporter and focused
targets. Narrow late corrections rebuilt their affected targets. New warning
sites were corrected; the initial log retains preexisting misleading-indentation
warnings in RuntimeWorld's material helpers. Final narrow build logs contain no
warnings. The complete production-suite collection was not rerun for this
optional feature; the relevant focused regressions below were run instead.

| Contract | Observed result | Receipt |
|---|---|---|
| Mechanical core, sources, gear/differential, clutch/brake capacity, drag, failure atomicity, cold continuation and derived-cache reconstruction | 56 checks, zero failures | [core](linux/powertrain.txt) |
| Actual wheel/body/bearing coupling, staged publication, authoring, prefab, saves, prescribed bearing motion and actual asynchronous colon-named cross-region references | 97 checks, zero failures | [lifecycle](linux/powertrain_lifecycle.txt) |
| Actual public QuickJS surface, documented cookbook, held commands, live enumeration | 17 checks, passed | [script](linux/powertrain_js.txt) |
| Flat application, real chassis drive, clutch take-up, gear relation, differential, electric rover, hinged machine | 15 checks, zero failures | [flat](linux/application-flat.txt) |
| Arbitrarily rotated uniform-gravity application | 16 checks, zero failures | [oblique](linux/application-oblique.txt) |
| Real spatial radial-gravity application and sphere collision | 16 checks, zero failures | [radial](linux/application-radial.txt) |
| Public API positive/negative controls, fixed-phase restriction, detached settings, disabled and destroyed handles | 18 checks, zero failures | [contracts](linux/application-contracts.txt) |
| Actual runtime prefab, registered region unload/revisit with remapped IDs and exact durable records, destroy/reload | 11 checks, zero failures after durable-cache correction | [region lifecycle](linux/application-lifecycle.txt) |
| Real SaveService write then separate-process restoration and resumed physical endpoint acceleration | write 16/read eight checks, zero failures | [write](linux/application-write.txt), [read](linux/application-read.txt) |
| Missing/phantom optional participant loads, exact failure and original-world continuity | ten checks each, zero failures | [missing](linux/application-missing.txt), [phantom](linux/application-phantom.txt) |
| TypeScript/source drift and live module enumeration | 32 exports, 364 public symbols, 252 native operations, 16 callbacks, 41 examples, three negative drift checks; passed | [source](linux/api-source-coverage.txt), [live](linux/api-live-coverage.txt) |

Relevant existing regressions passed: M81 wheel numerical/lifecycle/JS,
physics/collision/casts, joints/authoring, CharacterMotor, kinematic motion,
physics continuation, save storage/exclusion, M79 structural, M80 bending,
gravity selection and uniform-gravity authoring. The actual invocation/exit
inventory is [focused-cases.json](linux/focused-cases.json); individual logs
retain their own check counts. This was a focused collection, not a claim that
all production suites or historical research were rerun.

After the SaveService correction, existing save storage/exclusion, M81 wheel
application flat/oblique/radial/lifecycle, and streamed-range separate-process
save continuation were rerun successfully. Their final execution/exit inventory
is [save-service-regression-cases.json](linux/save-service-regression-cases.json).
Earlier genuine failures remain in development evidence; they are not hidden by
the later pass receipts.

## Editor and standalone

The ordinary editor loaded a temporary copy of the project, entered/stopped Play,
saved normally, and reported authored state **IDENTICAL** after Play/Stop.
[Editor output](linux/editor.txt) and edit/play PNGs are retained. Its offscreen
SDL backend lacks relative mouse mode; resulting mouse-capture warnings are
recorded, and actual desktop pointer behaviour remains a human check.

Normal export produced **ten assets, five scenes and 54,701,126 bytes** at:

```text
/tmp/judas-m82-powertrain-lab-20261013
```

The complete package was copied to `/tmp/Judas M82 Moved Package`, then launched
from the unrelated `/tmp` working directory. Its packaged executable completed
the 4,370-frame logical-input watch tour, pause/resume, oblique/radial scene
transitions and radial reload. Screenshots show solved RPM/torque/slip for all
three assemblies. The actual rover encounters an authored obstacle and exhibits
load/stall behaviour; the focused input test separately proves actual driven
distance. This is not a navigation/autonomous-driving claim.

The moved project also passed write 16/read eight checks, including exact durable
mechanical records and new-input physical response after cold restoration.
See [package invocation/exit receipts](linux/package-cases.json),
[standalone log](linux/moved-standalone.txt),
[moved write](linux/moved-application-write.txt) and
[moved read](linux/moved-application-read.txt). No source-tree working directory
or hidden project-specific native path was needed.

## Measured performance

Core pre/post projection benchmark, seven scalar degrees of freedom per graph:

| Simultaneous assemblies | Median ms | p95 ms | Maximum ms |
|---|---:|---:|---:|
| 1 | 0.007216 | 0.015074 | 0.024780 |
| 10 | 0.072501 | 0.158832 | 0.192701 |
| 100 | 0.739280 | 1.248367 | 3.188245 |

The ordinary lab contains three mechanical assemblies, 13 shafts, eight sources,
four links, nine mechanical brakes, ten M81 wheels, three dynamic bodies, ten
static bodies and one ordinary hinge before optional spawn/payload/streaming.
Each application drive run samples 640 frames with a fixed `1/60 s` frame input.

| Scene | Fixed median ms | Fixed p95 ms | Fixed max ms | Frame work median ms | Frame work p95 ms |
|---|---:|---:|---:|---:|---:|
| Flat | 1.024629 | 1.653391 | 3.047294 | 20.574905 | 26.728088 |
| Oblique | 1.024874 | 1.663866 | 2.070343 | 20.647416 | 26.654956 |
| Radial | 0.985022 | 1.645791 | 5.501210 | 14.499077 | 20.147648 |

These are normal hidden application executions using SDL offscreen GL. Frame
work includes application/render work and is **not** desktop refresh-rate or
physical GPU acceptance. Core timings omit ordinary tyre/contact/render cost.
Measured input, hardware/software environment and full timing receipts are
retained with the logs; no Windows VM timing is an acceptance gate.

## Delivery and remaining limits

Exact delivery scope and integrity records are
[CHANGED_FILES.txt](CHANGED_FILES.txt), [SOURCE_SHA256.txt](SOURCE_SHA256.txt),
[BINARY_SHA256.txt](BINARY_SHA256.txt) and
[VERIFY_FINGERPRINTS.txt](VERIFY_FINGERPRINTS.txt). Source hashes identify the
final candidate, while local binaries/packages stay outside the committed
source inventory. Verification status is the actual receipt, not inferred from
a successful compilation.

The raw initial untracked listing includes one newly created M82 header;
[CONTINUITY_NOTE.md](CONTINUITY_NOTE.md) records that classification correction
and confirms preservation of all actual unrelated entries.

Authoring review also found a **preexisting, source-traced streaming limitation**:
an explicit hard qualified reference from an installing region back into that
same region uses the public active-region resolver before publication. Normal
same-region graph references remapped by ordinary local entity IDs are supported
and are what the garage uses. The wider explicit self-reference resolver path
is deferred; it is not certified by the M82 passing local-reference tests.

The graph is scalar rotational mechanics, not full gyroscopic rotor transport,
thermodynamics, electrical/battery simulation, flexible shafts, backlash or a
limited-slip differential. Abrupt ideal ratio changes can dissipate energy via
their physical impulse projection; projects should disengage/re-engage a clutch
when that is their desired policy. Existing M81 tyre/contact/CCD limits remain.

The watch tour uses real continuing motion on a finite deck; it is not an endless
automated driving course. For prolonged manual driving steer/brake within that
deck. No scripted chassis repositioning is used to hide this boundary.

Human acceptance is [pending](HUMAN_CHECKLIST.md). Protected history, independent
fluid research, existing games and unrelated deferred defects remain outside
this change scope. No later feature work or Stable/main promotion is part of M82 delivery.
