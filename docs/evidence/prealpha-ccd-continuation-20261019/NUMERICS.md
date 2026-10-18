# Native depth bound

For selected native triangle front n and actual points v, take H=max(dot(n,v))
and L=min(dot(n,p)) over query vertices p. D=max(0,H-L) bounds front penetration.
The allowed SAT axes include normalized n, so ordinary selected depth is bounded
by D/|n|. The rear-cone guard explicitly selects that front. Triangle face witnesses
use q=p-n*h and gap=dot(responseNormal,p-q), bounded by |n|*D provided clipping
retains the incident convex hull. All three triangle projections are enclosed,
including winding reversal and represented rotation.

The original epsilon-classification/zero-plane interpolation could extrapolate.
Retaining the incident endpoint when t lies outside [0,1] corrects this separate
numerical defect. Genuine crossings keep exactly the original expression.

Bounds do not assume exactly unit represented normals. They use max(|n|,1/|n|),
a normalization envelope and conservative 8*gamma(128)*M arithmetic padding,
gamma(k)=k*epsilon/(1-k*epsilon), M at least one and the largest mesh-relative
coordinate magnitude. This covers three bounded clipping stages, vertex/dot/
projection/subtraction operations. Padding only makes rejection harder; it never
changes actual contact tolerances. Nonfinite/unsupported cases fall back.

The interval certificate uses existing represented Cayley rotation intervals and
monotone binary32 centre endpoint bounds on one real trajectory segment. A
conservative swept enclosure selects every potential finite triangle. It checks
both fronts when native selection is uncertain, including the one-sided epsilon
strip. Only a certified whole interval is omitted. Hulls, compounds, pivots and
unsupported prescribed rotations retain full search. The point bound uses actual
native vertices after actual side selection and only affects the budget Boolean
predicate; response manifolds/ordinary contact queries retain their original path.

A direct adversary keeps unguarded SAT gap near .001 m while native accepted depth
grows from approximately .159 to .169 m. A SAT-gap-only shortcut would be wrong.
Dense-time controls falsify implementation errors; they do not replace the bound.
All 8,192 point-query rows and 120 corrected physical step digests match the
unoptimized corrected version. These claims do not make temporal sampling
universally continuous or resolve arbitrary deep initial overlap.
