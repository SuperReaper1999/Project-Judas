# Remaining-motion primitive bounds: conservative candidate rejection

Files in the shared candidate: `src/RigidMotionBounds.h` and `tests/RigidMotionBoundsTests.cpp`. No contact solver, event policy, sampling budget, or physics trajectory is changed.

## Contract

The helper is given one actual `RigidMotionSegment` covering `[start, finish]` and that segment's existing evaluated endpoint poses. It supports centered primitive boxes/spheres. Unsupported shape offsets, prescribed rotating trajectories, cross-segment intervals and arithmetic-range failures retain the old enclosing-sphere bound. The final result may be intersected with the old bound because intersection of two valid enclosing sets remains an enclosing set; an inconsistent empty intersection retains the old bound.

## Translation

For each axis, the actual represented center is `p0 + v * float(t - begin)`. Every operation is monotone on a segment, with order reversed when the fixed velocity is negative. Thus each represented center lies between the evaluated endpoint centers. With a fixed box orientation, union of the existing outward-rounded endpoint `ShapeAabb` values encloses the entire interval. Centered spheres are orientation-independent.

Dynamic zero angular drift is checked using the actual quaternion zero arithmetic and exact anchor bits. This deliberately rejects the signed-zero normalization corner case rather than asserting that numerical orientation is constant without evidence.

## Dynamic rotation

A dynamic segment has a fixed generator. Its pre-normalization quaternion is

`raw(t) = q0 + (omega * q0) * (0.5f * float(t - begin))`.

Each raw component is a rounded monotone affine function of represented elapsed time. Its endpoint values therefore bound every intermediate raw component exactly in the production arithmetic.

Let `lo_i, hi_i` be those component intervals. The minimum absolute component is zero if the interval spans zero, otherwise `min(abs(lo_i), abs(hi_i))`; its maximum is `max(abs(lo_i), abs(hi_i))`. The same GLM float length expression at those componentwise minima/maxima encloses every actual represented length: squared nonnegative magnitudes, the positive-addition tree and square root are all monotone. Applying actual float `1 / length` reverses the two positive bounds. Each normalized component is bounded by the four actual float products of its raw-component endpoints and reciprocal-length endpoints.

`RigidMotion` also skips normalization when the raw quaternion matches the anchor exactly. The anchor is unioned into the normalized-component intervals whenever every raw-component interval can include that anchor. Otherwise the branch is impossible.

The existing outward binary64 `contact_geometry::Iv` arithmetic then encloses the exact normalized rotation formula `R(q) = N(q) / (w*w+x*x+y*y+z*z)`. Nonnegative square intervals avoid an artificial zero-crossing denominator. Each box extent is bounded by `sum_j abs(R[j][axis]) * half[j]`. Adding the monotone center interval and rounding outward to float yields the new AABB. Invalid denominator/range falls back instead of asserting separation.

This covers full represented dynamic rotation, not endpoint geometry interpolation. Prescribed sine/cosine rotation deliberately remains on the old bound.

## Focused evidence

The isolated checks use actual production `RigidMotion`, `ShapeAabb`, `ShapeBoundingRadius`, and existing engine library. Randomized boxes and spheres cover dimensions 0.02–4 m, arbitrary orientations, center coordinates through 1e24, translation scales 0.01–1e4, angular speeds 1e-3–1e4, varying start times and interior samples. Each direct bound and its intersection with the original sphere must contain the independently evaluated outward `ShapeAabb` at every sample.

Negative controls reject rotating boxes from the translation-only helper, intervals crossing a prescribed stop, unsupported pivot offsets, zero quaternions and normalization overflow. A neighboring-box case verifies the new bound rejects an actual gap that the old sphere enclosure cannot reject.

The proof, rather than randomized sampling, establishes conservativeness. The tests exercise implementation errors and unsupported cases. Small direct compilation/test is appropriate; the parent owns final combined physics regressions/performance evidence.
