# Pass 5: deferred mixed sphere/box manifold reuse

## Disposition

**Not adopted into production in this pass.** Source inspection and detecting
native tests establish a safe, narrowly guarded reuse mechanism. It removes
27,510 duplicate geometry calls in the worst dense step while preserving exact
physical results. However, the single valid matched comparison has a worse
median, essentially unchanged total wall time and higher observed RSS. Small
p95/maximum differences do not justify another production path on this evidence.
No general speedup, worst-case guarantee, memory benefit or human acceptance is
claimed. The accepted first fat-candidate-cache correction is retained separately.

All proposed production files remain **drafts inside this evidence**. No
`ImpactManifoldReuse.h`, reciprocal runtime path or permanent reciprocal test was
added to engine source. No more timing runs were performed after this disposition.

## Demonstrated mechanism and proof boundary

The prior contact-subcost observer establishes that a reversed discovery hit is
followed by a second, canonical-order response query. In `Contacts.cpp`, native
mixed sphere/box orders call **the identical sphere-first `SphereBoxPair` context**.
The box-first dispatcher then negates normal/preciseNormal and swaps local
anchors/witnesses. That operation is an involution. `Narrowphase.cpp`'s independent
per-child frame transformations commute with the field swap. Thus the existing
native mixed manifold can be converted to the exact canonical response fields,
while retaining the original discovery query, its truth and its uncertainty.

The proposed path:

1. Runs the original directed discovery query without alteration.
2. Reuses a hit only for a native mixed sphere/box pair.
3. Applies exactly the native dispatcher's field involution when discovery body
   order is reversed.
4. Records swapped **canonical primitive indices** alongside that geometry.
5. Retains original traversal, response-row order, filters, CCD and solver calls.

Box/box and sphere/sphere are deliberately excluded. Native box/box reversal
actually changes canonical response fields in the negative corpus; passing a
Boolean fuzz comparison would not prove full-manifold equivalence.

The first passed prototype classified by primitive type only. Source review then
identified a necessary boundary: a noncompound Box/Sphere descriptor can carry an
asset pointer and redirect `ComputeContacts` to cooked geometry. The final guarded
prototype excludes that case. Compound simple children remain eligible because
`PrimitiveAt` reconstructs native Box/Sphere descriptors and ignores parent asset
metadata. The initial prototype and its passing receipts are retained separately;
no actual failure for an asset-bearing descriptor is invented.

The comparison checks every meaningful current `ContactManifold`/`Contact` field:
count, uncertainty, hit, world point, normal, precise normal, legacy penetration,
signed separation, local anchors and witnesses, local-anchor validity, world-frame
anchor flags and separation state. Each float/double component is compared by
represented bits, not a tolerance. The current contact type has no additional
`hasPrecise` or feature-tag fields. Compound identity is retained through primitive
indices; the original source snapshots make the exact type/dispatch contract
inspectable.

## Functional and adversarial evidence

The final native corpus passes **2,653 cases**, exercising 774 hits, 1,851 misses
and 22 uncertain results. It includes both argument orders, ULP-near tangency,
nonunit rotations, pivot offsets, rotated/offset compound children, huge/tiny
scales, subnormal/collapsed direct geometry and zero/NaN/infinite orientations.
Independent known overlap/depth/normal and known separation cases prevent the
comparison from passing solely because both paths return no contact.

Detecting test-only negatives:

- Omit normal/preciseNormal conversion: 774 failures, exit 1.
- Omit anchor/witness conversion: 774 failures, exit 1.
- Incorrectly permit box/box conversion: all 50 added box cases fail actual
  native canonical field comparison, exit 1.

Ten existing engine suites run on baseline and candidate using identical compiled
test objects: impact timing, contact cache, kinematic motion, continuation,
contact lifecycle, CharacterMotor, joints, particle/body contacts, rigid motion
and impact robustness. Every expected exit passes. Original assertions are not
modified.

An additional ordinary `PhysicsWorld` fixture uses actual compound impacts where
only higher-ID child 1 contacts lower-ID child 2, and the reciprocal 2/1 arrangement.
It covers sphere-moving/box-moving, rotated charts and translated charts. Actual
impact/contact rows, no-crossing and restitution are checked. Its **40 checks /
320 steps** pass, and all 44,117 bytes of body-state/contact trace are identical
between baseline and candidate. Disabling the real collider yields **24 failures**
in that same 40-check fixture. No scripted pose correction or solver retuning is
used.

The proposed cohesive `adoption/ReciprocalContactsTests.cpp` merely combines the
two tested standalone fixtures into namespaces and one entry point. That combined
**draft was not compiled** after the decision to defer; the separately compiled
source and binary hashes in `candidate/RESULTS.json` are the executed evidence.

## Exact source and artifact provenance

The baseline is the numerical implementation checkpoint
`be621491a6a1a578ac87e8c82e3b297ed8b18614`. Its frozen engine archive is:

`0e0dc7973b7d2b24510743d5297e9ae002cdb7b4dbdc943d55a4bdc4aab78514`

This is **not relabelled a clean be621 build**. Its actual older dirty build
identity, executable bytes and hashes are preserved in
`experiment/baseline/FROZEN.json` and the original build-info file. The later
liquid documentation checkpoint changes no physics source. Candidate builds
replace one copied `WorldImpactEvents` object and record exact helper/source,
archive, executable, commands and test-object provenance. Existing collision
implementations are unchanged; relevant native sources are byte-preserved here.
The cache copies are excluded from Git; evidence contains no executables,
archives or object files.

## Valid matched observation

Same original 270-step/1,024-piece scene, physics inputs, Release build and Linux
host as the first Pass 5 cache experiment. Root serialized heavy work and held the
Windows VM. Both valid observations were declared before execution. No timing run
was selected for its favourable maximum or repeated after seeing this result.

| Valid run | Median ms | p95 ms | Maximum ms | Wall s | Peak RSS KiB |
|---|---:|---:|---:|---:|---:|
| Cache A | 26.079 | 341.528 | 1740.532 | 50.589 | 410532 |
| Guarded reuse B | 28.068 | 328.003 | 1658.407 | 50.546 | 415588 |

The p95 is the existing runner's floor-index convention. Median is 7.6% worse;
p95 is 4.0% lower and maximum 4.7% lower; wall difference is negligible. RSS is an
observed process value, not proof of a universal memory increase or saving.

All 270 physical records / 15 fields and endpoint geometry are identical, ending
with digest `192f0f4bed728953`. The four original free, pile, active-machinery and
sleeping-machinery controls have identical physical CSV fields, full endpoint
arrays, acceptance fields and digests. Every summary difference is retained in
`experiment/ANALYSIS.json`, including diagnostic timing/allocation differences.
Actual geometry-scope calls fall from 626,049 to 598,539 in step 270; inclusive
geometry time is 857.620 to 809.004 ms. These inclusive categories overlap other
work and must not be added together.

## Preserved harness mistake and corrected follow-up

The first attempted A/B was invalid: `shutil.copyfile` preserved baseline binary
bytes but dropped executable mode. All five A commands exit **126**. The historical
as-run workload driver correctly records those exits but itself exits zero, so the
first wrapper proceeded to a candidate run. That candidate completes but remains
**unpaired and excluded from performance claims**, regardless of its timings.

Preserved locations:

- `experiment/first-permission-failed-A`: original command/exit/resource logs.
- `experiment/first-unpaired-B`: the completed unpaired candidate observation.
- `experiment/FIRST_ATTEMPT_COMMANDS.json`: original insufficient wrapper receipt.
- `experiment/PERMISSION_FOLLOWUP.json`: mode-only correction, unchanged hashes and
  evidence directory moves.

The corrected wrapper restores executable mode only and requires all five child
exits zero, 270 completed dense steps and successful control summaries. Its actual
A/B commands and receipts are retained. Expected hashes, fixtures, limits and
physics are unchanged. This harness mistake does not invalidate earlier committed
receipts whose child results were independently verified.

## Reproduction and inventory

`reproduction/build_reciprocal.py` and `run_matched_reciprocal.py` are the exact
isolated build/relink and strict matched-run drivers. They refer to the owned
`.cache/prealpha-pass5/dense/reciprocal` layout and existing Release test objects.
Use the frozen baseline indicated by hashes and copy executable modes as recorded;
run only one heavy workload at a time. Native tests and the full source variant
are under `experiment/`. `native-source/` preserves the relevant unchanged dispatch
implementation for independent review. `adoption/` files are proposals, not
installed engine source or executed final-combined-test receipts.

`SOURCE_INDEX.json` maps each preserved payload to its original path and SHA-256.
Historical receipts may name paths later used for the guarded variant; the
`initial-native-prototype` source snapshots retain the matching old bytes.
`COMPRESSED_INPUTS.json` records original hashes/sizes for lossless profile gzip.
`MANIFEST.json` enumerates evidence bytes, excluding itself to avoid self-reference.
The severe dense-contact tail remains an open production limitation.
