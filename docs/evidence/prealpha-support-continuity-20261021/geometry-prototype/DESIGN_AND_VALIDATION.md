# Private geometric feature-episode prototype

## Status and scope

These are private implementation candidates, not production changes or a claim
of measured speedup. The child inspected source, created a modified private copy
of `Contacts.cpp`, and reviewed the diff. It did **not** compile or execute it.
The root owns integration, tests, measurements and the final architecture.

Files:

- `PrimitiveContactEpisode.h`: caller-owned small feature state/options/stats.
- `Contacts.cpp`: current original source plus the two feature paths and private
  API. All existing public functions remain present and use native behavior
  unless called within the explicit private episode wrapper.
- `GeometryEpisodeQualification.cpp`: source-only deterministic qualification
  fixture, with independent native/shadow comparison and sensitivity controls.
- `SOURCE_MANIFEST.json`: original/current private source hashes.

This is a replacement candidate for **native `PrimitiveContacts`**, not the
general `ComputeContacts` cooked-mesh dispatcher. The accelerated domain is
zero-margin native Box/Box. Sphere pairs retain their unchanged analytic path.
Do not replace cooked/terrain/compound dispatch with this wrapper.

## Intended ownership and integration

The world's persistent pair graph owns one `Episode` for each directed primitive
pair with generation-validated body identities. Geometric state contains no body
pointers, solver pointers, old impulses, old world-space positions or old
manifolds. Root should preserve existing collision/mask/sensor/suspension checks
before calling the same narrowphase dispatch point.

```cpp
using namespace persistent_contact_geometry;
auto result = Query(shapeA, poseA, shapeB, poseB, pair.episode,
                    {.separatingWitness = true, .uncutFaceReplay = true,
                     .shadowNative = false},
                    0, preparedA, preparedB);
```

For pre-C++20 builds, construct `Options` and assign fields explicitly rather than
using the illustrative designated initializer. The source itself uses ordinary
C++17-compatible aggregate initialization.

The wrapper checks a primitive dimension/type signature and resets on changes.
Root still owns generation/filter/topology/lifetime invalidation and memory
budget. Relative positions/orientations can change arbitrarily; neither fast
path assumes they are unchanged. Root may retain these hints across steps if
all world ownership rules are enforced; the hints are always revalidated at the
current pose and never used as a historical Boolean answer.

Replace the original `Contacts.cpp` object in the private archive, compile this
copy with the sibling private header and original `src` include directory, and
include the private header at the graph integration point. Do not link both
original and prototype `Contacts.cpp` definitions. The wrapper uses scoped
thread-local capture so unrelated native calls remain independent.

## Path 1: revalidate the retained separating feature

On a native zero-margin Box/Box separation, retain only the rejecting SAT axis
index (0–14). On a contact, the selected response axis can also be tried as a
future separating hint. At the next actual pose:

1. Construct the same current native `Context` and `ContactSatFilter`.
2. Recompute that axis from the **current** represented quaternion pair.
3. Use the existing filter to certify it is nonzero.
4. Evaluate the same homogeneous `Sat` polynomial and require a certified
   **strictly positive** sign.
5. Only then return an empty manifold. Otherwise run the complete native SAT,
   including its interval/exact fallbacks, and learn its current feature.

This is a new proof on each query. The old rejection is not reused. Changed
orientation, a different separating axis, contact/recontact, an arithmetic-range
failure or an ambiguous sign cannot force a stale empty answer. No epsilon,
penetration allowance, response normal or solver input is changed.

Why this is sound geometrically: a nonzero current axis whose projected support
intervals are strictly separated proves the full two boxes are disjoint. The
existing `ContactSatFilter` already certifies this polynomial's sign against its
native interval/exact contract. This is not a broadphase approximation.

**Compatibility qualification:** a proof of disjoint geometry is not by itself
proof that the complete native traversal would never report uncertainty on some
earlier, unrelated axis due to an arithmetic-range failure. The filter is guarded
to its bounded arithmetic domain, but the prototype must still shadow-check
extreme inputs and preserve a native-fallback boundary if any native uncertainty
disagrees. Do not declare complete native parity based only on the geometric
argument or ordinary pile cases.

The expected opportunity is the large number of *different-pose* empty domain
queries; whether the previous witness survives and is cheaper must be measured.
Fresh and failed hints add work. This is not the rejected exact-input map.

## Path 2: exact four-vertex face continuation

Retain reference face, incident face, their signs, and whether the previous
patch was an unclipped four-vertex incident face. On a new positive query:

1. Run the complete native `BoxSat`, including the native response-axis selection,
   penetration face preference and all sign/numeric-gap checks.
2. Compute the same current incident face and its four vertices.
3. Require the retained reference/incident identities and signs to match.
4. Require all four current vertices strictly inside both finite reference-face
   tangential bounds. Edge/vertex boundaries fall back.
5. Require all four current normal gaps finite and nonpositive.
6. Reconstruct the same four native contacts in their original order, including
   fresh witnesses, precise normal, local anchors and signed separation.

Proof of exact native output for this subset: each of the native clipper's four
passes would retain all four identical input vertices, in order, and insert no
intersections. All four candidates pass the zero-margin gap condition, so no
closest-feature fallback or >4-point reduction is invoked. The prototype uses
the same scalar expressions and `MakeContact` sequence. No old contact anchor or
normal is transported as though it remained valid.

This accepts orientation/position changes within a retained finite feature
episode. A changed SAT winner, changed incident face/sign, tangential exit,
partial normal separation, edge-axis response, ambiguity or uncertainty falls
back to the complete native clipping/manifold path.

**This path only saves clipping and bookkeeping, not SAT.** It deliberately
does not invent a proof that the old response axis stays optimal after changing
poses. Counters distinguish its opportunity from actual avoided SAT in Path 1.
If it does not pay for its guards in matched measurements, disable it; its value
is a small independently provable prototype boundary, not an architectural win
claimed in advance.

## Counters and controls

`GetStats`/`ResetStats` report hint attempts, certified separations, native SAT
loops, learned separators, retained-face candidates/replays, feature exits,
uncertainty and shadow mismatches. Root should attach step/run/origin attribution.

- Both options false: immediate unchanged native query, no episode mutation.
- Separating witness only: tests avoided SAT independently, without four-vertex
  checks; still records native positive response axes as potential future hints.
- Face replay only: tests clipping savings independently with complete SAT.
- Both true: combined prototype.
- `shadowNative=true`: every proposed shortcut is independently evaluated with
  the complete native path; compare **all valid Contact fields bit for bit**,
  not just count/hash. Return the native output in shadow mode, and reset the
  episode on disagreement. Shadow time is not a performance comparison.

Native diagnostic predicate counts decrease when valid proof paths bypass work;
they are not expected to remain identical workload counters. Physical output
and uncertainty must be checked separately. There is no automatic production
enablement, file I/O, gameplay hook or hidden demo exception.

## Required positive and negative tests before integration

### Separation proof

1. Learn a rejecting face at one pose; translate/rotate both boxes to different
   represented poses that remain separated along that feature. Prove the hint
   actually runs and avoids a full SAT traversal; shadow must match exactly.
2. Repeat with a valid cross-edge axis, then make its edges nearly parallel.
   An uncertified/zero axis must fall back; never turn ambiguity into separation.
3. Move from separation into face, edge and vertex contact and then penetration.
   A stale hint must fail and the complete native nonempty output must appear.
4. Change the actual separating feature; native fallback must discover it.
5. Exercise signed zeros, quaternion sign flips, off-unit quaternions, tiny and
   large bounded values, large positions/small gaps, subnormal/out-of-range
   values, invalid orientations and uncertainty. If native uncertainty differs
   from a hint empty, narrow the proof path or reject it before adoption.
6. Dimension change, body generation replacement, reversed order and primitive
   remapping must preserve native results and required ownership invalidation.

### Exact positive patch replay

7. A useful positive seed: A box `(4,.3,4)` at the origin, B box `(.5,.5,.5)`
   near `(0,.75,0)`, with small B translation/yaw/tilt. After the first native
   query, the retained large reference face should permit four-vertex replay.
   Check every output field against independent native queries at each new pose.
8. Move B toward the finite reference edge until one incident vertex exits.
   `faceOutside` must increment and clipping/native counts must take over.
9. Tilt/raise B so some incident corners have positive gap. `faceGapRejected`
   must increment; preserve native fewer-point and contact-loss behavior.
10. Change reference winner, incident face/sign, or enter edge-edge contact.
    No stale-face replay. Include exactly-on-boundary and one-ULP-on-either-side.
11. Exercise >4-point clipped polygons and native point reduction. They must
    remain native, with identical row order and anchors.
12. Origin translation and arbitrary common rotation must retain appropriate
    geometry/normal behavior; compare to native in the new frame rather than
    claiming transformed float arithmetic is bit-identical to the old frame.

### Integrated physics and usefulness

13. Run shadow mode over the original short/long/live 1,024-piece scenes and
    actual consumer arrangements, preserving uncertainty and all physical output.
14. Native versus prototype trajectory digests, contact/CCD regressions, curved
    finite-mesh cases (untouched native path), angular/energy/momentum invariants,
    saves and lifecycle tests appropriate to root's graph integration.
15. Measure hint success/failure rate, face replay rate, geometry costs, memory,
    median/p95/worst step and full-frame outcomes with identical workloads.
    Reject negative gains and preserve the failed control. Do not extrapolate
    savings from a high empty-query count or synthetic positive patch alone.

No item above is marked passed by this design document.

## Qualification fixture and adoption gate

`GeometryEpisodeQualification.cpp` is a standalone test entry point. Link it
against the private prototype `Contacts.cpp` object and the normal dependencies
needed by that object. Do not also link the original Contacts.cpp definitions.
No build or execution has yet been performed by the child agent.

The fixture checks both the returned native output **and** shadow mismatch
counters, because shadow substitution alone would conceal a bad proposal. It
requires both acceleration paths to be exercised, so a permanently disabled
implementation cannot pass simply by always calling native. Directed episode
changes, clipping exits, contact entry, exact one-ULP boundaries, nonunit/invalid
orientations and an exact-zero stale cross-axis hint test the fallback mechanism.
Forged stale-empty and corrupted witness/gap results verify comparator sensitivity.

This prototype is a candidate component, not a requirement to add complexity to
the final engine. Root's separate retained ordinary-force control has shown a
much larger possible benefit and must be evaluated independently. Qualify any
geometry path only if it improves the **combined** candidate with sound physical
output and lifecycle tests. If its additional guards/state do not pay for
themselves, retain this source/evidence privately and omit it from production.
