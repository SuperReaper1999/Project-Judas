# Actual SetSolids near-plane wet-aperture investigation

## Reproduction and observation

This probe bakes a normal 2x2 surface from a 4 m3 box, creates LiquidSurface with
2 m3, and calls its public SetSolids API with a bounded six-plane physical solid.
The solid has a thin, oblique face ending just outside one real shared fluid face.
Every corner has strictly positive distance from that solid plane: about1e-16 at
one edge and2e-11 at the opposite edge. Thus actual solid intersection on the shared
face is empty. An independent long-double polygon clip computes open wet area.

The post-SetSolids physics aperture is observed through public KineticEnergy with
one nonzero public face discharge Q=1. Its actual equation gives
A = .5*face.distance*Q^2/KineticEnergy(cell). Other discharges are exactly zero.
This does not infer aperture from stored liquid quantity, bypass solver privacy,
or substitute a ledger test for a geometric test.

The physical surface and six solid planes are rigidly rotated together through
public data structures. Per-cell storage/loading geometry and original face
polygons/normals are transformed consistently; this is not a scene-name exception.

| Rotation about normalize(1,2,3) | Expected open area, m2 | Before actual, m2 | Before excess, m2 | After error, m2 |
|---|---:|---:|---:|---:|
| identity | .50505050504540294 | .50505303029792814 | 2.5252525251984181e-6 | 0 |
| .8 radians | .50505050504540294 | .50505338429475932 | 2.8792493563800647e-6 | 0 |
| 1.37 radians | .50505050504540283 | .5050535612982574 | 3.0562528545718948e-6 | 0 |

Raw before/after heads are identical in each matched pair. Body displacement
changes the local head legitimately; the area oracle uses that actual head.
Overflow is0 and the actual SetSolids diagnostic is empty for all cases.

## Mechanism and correction

The face-subtraction helper classified inside using signed distance<=1e-12, but
computed crossings at signed distance0. A positive near-plane endpoint was therefore
inside while another positive endpoint was outside. The resulting interpolation
parameter was negative: the algorithm extrapolated a clipped polygon beyond the
original physical face. Taking its outside portion made the aperture larger.

The production correction changes only the inside/complement tests to<=0/>0 in
subtractFace's clipping lambda. Classification and intersections now refer to the
same actual physical plane; no tolerance, solver budget, timestep or ledger repair
is introduced. Conservative broad-phase rejection and unrelated area/welding
thresholds are unchanged.

## Build / reproduction

From the Judas repository root, with the matching production source state:

    c++ -std=c++17 -O1 -I src -I third_party/glm \
      .cache/prealpha-liquid-rotation/face-investigation/probe.cpp \
      src/LiquidGeometry.cpp src/LiquidSurface.cpp src/PerformanceProfiler.cpp \
      -pthread -o .cache/prealpha-liquid-rotation/face-investigation/probe-after
    .cache/prealpha-liquid-rotation/face-investigation/probe-after

Original source fragment/full source is preserved as LiquidSurface.before.cpp;
original executable and new executable stay only in the ignored cache. BEFORE.json
and AFTER.json record the actual command, compiler, source and binary fingerprints.
Do not copy executable binaries into committed evidence.

The corresponding ordinary test extends LiquidRotationTests with public actual
SetSolids controls for nonintersecting and genuinely intersecting solids, original
and rotated surfaces, and removal/cache invalidation. Root runs the combined gate;
this probe is the matched numerical observation rather than a replacement for it.

## Scope and remaining accuracy bounds

This demonstrates the actual reachable near-plane aperture inflation mechanism and
its removal for these three rigid orientations. It does not certify every possible
ill-conditioned solid/face intersection or arbitrary scale. Double-precision plane
arithmetic still has finite error; nearly parallel *true* intersections can amplify
small signed-distance roundoff. No global exact-predicate guarantee is claimed.
The original failed trace, research and fluid ownership remain unchanged.

## Unchanged true-intersection control and extended evaluation follow-up

The added ordinary gate deliberately retained a genuine nearly parallel positive
intersection, not only the original nonintersection. The exact-plane-only variant
then failed the unchanged positive orientation1 check:

    expected=.25252534323728032 m2
    actual=.25252492647307512 m2
    difference=4.1676420520e-7 m2

Root preserves that run at
`.cache/prealpha-liquid-rotation/final-combined/rotation.log`. This is a second
arithmetic defect: evaluating a cancellation-prone signed plane distance in binary64
and then dividing by a nearly parallel edge's small signed-distance change amplified
roundoff. The geometry and test thresholds were not changed.

A cache-only variant evaluates plane signed distances and the intersection fraction
in long double, rounding once when producing the final double vertex. The unchanged
entire ordinary rotation test passed419 checks/37 orientations with that variant.
All six actual SetSolids aperture observations (nonintersection and true intersection,
identity/.8/1.37 rad rotations) agree with the independent extended-precision polygon
oracle to reported error0. Raw log: `rotation-extended.log`. This is measured finite
precision agreement, not a proof of exact real-arithmetic aperture for every geometry.

Root authorized the same small correction in the production subtractFace lambda.
Only the classification, signed-distance evaluation and intersection fraction change;
no test, solver settings, timestep, area threshold or liquid ledger changes occur.
`FINAL.json` records final production/test hashes and variant executable identities.
The measured cache variant and final production source differ by explanatory comments;
root performs the final shared Release build/regression on the actual final source.

### Bounded cost sanity

`bench.cpp` repeats actual surface construction, physical SetSolids and energy
observation with the same three rotated fixtures. O1 standalone compiler flags,
not production Release;300 operations/trial,7 sequential trials per variant,
first trial excluded as warmup. Both retain identical accumulated observations.

| Face-distance arithmetic | Median time/300 operations | Median time/operation |
|---|---:|---:|
| binary64 exact-plane-only |50.024100 ms|.166747 ms|
| long-double exact-plane |49.826172 ms|.166087 ms|

The difference is ordinary measurement noise; no speedup is claimed. This small
sanity check shows no demonstrated large added cost for the representative local
SetSolids work. It is not a multi-body scalability benchmark or frame-time gate.
Raw per-trial values/source/binary hashes are retained in FINAL.json and bench logs.
The full build-bench command compiles geometry and profiler once, each Surface
variant separately, then links the identical bench source with the appropriate
Surface object; all commands use the same `c++ -std=c++17 -O1 -I src -I third_party/glm`
flags and `-pthread`. Cache outputs alone contain object/executable files.

### Platform boundary

Linux x86_64 GCC provides extended long-double precision, as used in these measured
checks. Some compilers/platforms, notably MSVC Windows, define long double with the
same precision as double. They do not automatically obtain the same extra protection
from this implementation. No Windows result or cross-platform exact-predicate
qualification is claimed here. The same physical near-plane tests should be run on
those platforms; portable compensated/exact predicates are a separate possible
extension if demonstrated necessary. Double output positions and other geometry
stages also retain finite precision. This is a bounded, evidence-supported repair,
not universal robust-geometry certification.
