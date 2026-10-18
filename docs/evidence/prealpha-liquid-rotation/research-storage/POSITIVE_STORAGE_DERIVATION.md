# Independent positive interval-storage derivation

This is new algebra for the Judas follow-up, not an imported Solbert implementation. The only executed artifact is the independent standard-library Fraction probe beside this note. No engine or research implementation was run by this subtask.

## 1. Projected uniform tetrahedron measure

Let the four projected vertex coordinates be sorted `h0 <= h1 <= h2 <= h3`, with `h0 < h3`, and tetrahedron volume `V > 0`. The uniform tetrahedron coordinate distribution is a degree-three simplex CDF. Its physical volume density is

`rho(q) = (3 V / (h3-h0)) N_0,2(q)`.

Here `N_0,2` is the ordinary nonnegative quadratic B-spline on knots `(h0,h1,h2,h3)`, with zero-denominator terms omitted. The B-spline integral is `(h3-h0)/3`, hence `integral rho = V` without normalization or a posteriori mass correction.

On a nonzero interval `[a,b]`, put `t=(q-a)/(b-a)`. Write

`rho(q) = d0 (1-t)^2 + 2 d1 t(1-t) + d2 t^2`.

With `G=3V/(h3-h0)`, the explicit controls are:

| Interval (skip if zero width) | `(d0,d1,d2)` |
| --- | --- |
| `[h0,h1]` | `G (0,0,(h1-h0)/(h2-h0))` |
| `[h1,h2]` | `G ((h1-h0)/(h2-h0),1,(h3-h2)/(h3-h1))` |
| `[h2,h3]` | `G ((h3-h2)/(h3-h1),0,0)` |

Every denominator used by a nonzero interval is strictly positive. All controls lie in `[0,G]`; none comes from subtracting almost equal CDFs. The formulas include repeated-height limits:

- `(a,a,a,b)`: one span, density `3V/(b-a) (1-t)^2`.
- `(a,a,b,b)`: one span, density `6V/(b-a) t(1-t)`.
- `(a,b,b,b)`: one span, density `3V/(b-a) t^2`.
- An internal double knot keeps two spans with continuous density.

No representable midpoint is needed. Adjacent binary64 heights can define an interval even when no binary64 point lies strictly inside it. Determine span membership by endpoint ordering, never by rounding a midpoint to an endpoint.

The same result follows from Cox–de Boor recurrence. On `[h1,h2]`, `N_0,2 = alpha(t)(1-t)+beta(t)t`, where `alpha=(q-h0)/(h2-h0)` and `beta=(h3-q)/(h3-h1)`. Its middle Bernstein control is `(alpha(1)+beta(0))/2=1`; the outer controls are those in the table. This supplies a compact independent check on an implementation using the general recurrence.

## 2. Positive aggregation over many tetrahedra

Collect all distinct projected vertex coordinates into a common sorted knot array. Restrict each tetrahedron's density pieces to the relevant global spans, sum densities, and integrate once in that global order.

Restriction of quadratic controls `(d0,d1,d2)` from parent parameter `[0,1]` to `[a,b]` can use the symmetric blossom

`P(x,y)=(1-x)(1-y)d0+[(1-x)y+x(1-y)]d1+xy d2`,

with child controls `(P(a,a),P(a,b),P(b,b))`. For `0<=a<=b<=1` all weights are nonnegative. De Casteljau subdivision is equivalent and preserves nonnegativity without switching to signed power coefficients.

A balanced range tree can retain the current `O(tetrahedra * log(knots))` assembly: store three nonnegative Bernstein density controls per node, restrict contributions into fully covered nodes, propagate by positive subdivision, then visit leaves in coordinate order. No global power-basis origin or subtractive endpoint extrapolation is required.

For interval width `w`, its exact positive volume is

`DeltaV = w (d0+d1+d2)/3 >= 0`.

Use one cumulative sum for both stored knot capacities and the final capacity. Converting a monotone cumulative sequence to binary64 is monotone, even if an increment is too small to change the rounded value. An independently summed final tetrahedron volume must remain an accuracy oracle, not be forced into the last knot after a different accumulation order; such forced endpoint replacement recreates the observed one-ULP decrease.

This is an algebraic re-expression of the same physical measure. It is not clipping negative capacities, sorting generated volumes, adding a minimum density, relaxing a decode tolerance, or transferring water. Numerical differences from direct geometric full volume must be measured and bounded under the existing acceptance criteria.

## 3. Coherent cubic cumulative storage

Given the left cumulative capacity `C`, integrate a leaf density to obtain cubic Bernstein controls

`B0=C`,

`B1=C+w d0/3`,

`B2=C+w(d0+d1)/3`,

`B3=C+w(d0+d1+d2)/3`.

They are nondecreasing. The interval CDF is the cubic Bezier function with these controls. Evaluate it through bounded interpolation/de Casteljau, rather than converting back to signed power coefficients and evaluating Horner. A positive-density construction alone does not prevent floating cancellation after power conversion; the capacity investigator has reproduced that remaining failure.

Retain the original density controls for physical one-sided slopes. Differencing binary64 CDF controls can erase a physically positive small derivative when all CDF controls round to the same large capacity. Storing density separately preserves that information without a synthetic floor. Rounded CDF and density represent the same pre-rounding primitive up to ordinary storage rounding; check their numerical consistency explicitly.

No file-format change is inherently required: these polynomial/density arrays are derived metadata rebuilt from decoded tetrahedra. The ordinary sampled curve remains an ordered table; M54's linear fallback remains unchanged. Every actual consumer must agree on the representation: forward capacity, inverse capacity, slope, and energy integral.

## 4. Energy primitive

For cubic Bezier capacity controls `(B0,B1,B2,B3)`, its unit-interval integral from zero to `t` is the quartic Bezier function with controls

`J0=0`,

`J1=B0/4`,

`J2=(B0+B1)/4`,

`J3=(B0+B1+B2)/4`,

`J4=(B0+B1+B2+B3)/4`.

Multiply by interval width `w` and accumulate completed intervals. This is the same `integral C(q) dq` used in `U=g[q C(q)-integral C]`; it does not alter the physical energy definition or solver equations. The outer potential-energy subtraction can still cancel for thin wet layers and deserves its existing tolerance/error accounting; this algebra does not promise arbitrary-scale relative accuracy there.

## 5. Conditioning and validation boundaries

- Cast projected coordinates to the accumulator precision before subtraction. Exact binary64 input coordinates should not be replaced by tolerance-merged heights.
- Positive normalized ratios avoid subtraction of nearly equal CDFs and large global power coefficients. Extremely small total height span still produces a physically large density `O(V/span)`; no universal uniform conditioning is claimed.
- If all four projected heights are equal, a nondegenerate 3D tetrahedron and valid nonzero affine coordinate cannot produce that geometry mathematically. Treat the degenerate numerical/field case through the existing explicit failure path, not an invented finite density.
- The density result is for the affine coordinate on each tetrahedron. Existing radial refinement error remains its own approximation contract.
- Check independent clipping/full volume, capacity inverses, repeated/adjacent heights, nextafter at span boundaries, wet-face areas and rotation-equivalent first moments. Storage positivity does not certify face geometry.
- Adding a new positive basis must not change M54 ownership, cadence, pressure budgets, damping, accepted test thresholds or research artifacts.

## Executed algebra checks

`positive_density_probe.py` independently checks exact Fraction identities over 210 packets, including all repeated integer-height patterns and adjacent binary64 heights. It compares 5,895 interior primitive values against an independent CDF recurrence, verifies 210 exact total-mass identities, 1,179 positive controls and 1,179 positive restriction/integral identities. It also checks 5,895 exact CDF-derivative identities and 5,895 quartic energy primitives against independent power antiderivatives. This is bounded exact-arithmetic algebra evidence, not an engine integration or continuum convergence claim. See `POSITIVE_DENSITY_RESULTS.json` for the executable-source hash and counts.
