# Independent mathematical review of the proposed storage correction

Reviewed the semantic three-file diff in `src/LiquidGeometry.h`, `src/LiquidGeometry.cpp` and `src/LiquidSurface.cpp` on 2026-10-18. No production edits or production tests were made by this reviewer.

## Findings

No blocking issue found in the mathematical representation or consumers:

- The explicit projected-density controls match the independent derivation for every nonzero sorted vertex span, including repeated heights. Every used denominator is positive.
- Range restriction uses the quadratic Bernstein blossom. Positive controls remain positive under subdivision/addition.
- Each leaf derives all cubic volume controls from the same cumulative prefix and positive density integral. Its final control and the next leaf's first control use the same prefix value. Final capacity is the final prefix, rather than a separately ordered full-volume sum forced into the last endpoint.
- `Volume` evaluates the cubic with extended-precision de Casteljau and one output rounding.
- `Area` evaluates the independently retained quadratic physical density. Its units already are volume per coordinate height; removing the old power derivative's additional width division is correct.
- `LiquidInverse` uses the same `Volume` evaluator and retains its existing 48-step interval bisection policy.
- `Integral` builds the correct quartic primitive from cumulative cubic controls divided by four. `LiquidSurface::Energy` applies the physical interval width once. It retains the same physical potential definition.
- Empty polynomial metadata retains the existing linear M54 fallback. Arrays remain derived from tetrahedra on load; no resource format or gameplay API change is implied.

The independent Fraction probe checks 5,895 exact derivative identities and 5,895 energy antiderivatives as well as the earlier capacity, positivity, repeated-height and exact-mass controls.

## Finite-precision qualification

The stored cubic CDF controls can coincide after rounding when a real interval contains less than one binary64 capacity ULP. The physical density remains positive and should not be recovered by subtracting those rounded controls. `Area` is the derivative of the same pre-rounding positive integral, rather than a literal derivative of its quantized binary64 evaluations. As with any floating residual, the latter is a step function.

If rounded control error is bounded by `E`, differentiating rounded absolute controls can amplify the error by approximately `6E/width`; the separate density avoids this artificial cancellation. A finite-difference Jacobian check must use coordinate steps that resolve capacity changes, and forward capacity error must remain below unchanged geometric/solver tolerances. Adjacent capacity endpoints should be verified identical, without claiming bit-exact derivative equality at unresolvable heights.

This review does not certify native integration, geometric clipping, resources, saves, physics coupling or performance. Those require the parent and capacity investigator's actual final-source runs.
