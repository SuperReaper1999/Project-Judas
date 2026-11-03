# Reflected pressure neighbours at a finite stationary face

Isolated experiment at `103f4bf5` (documentation-only successor to engine source
`2fccc514bb7bd2e384c4bf726393c273d0b44beb`). Native Release GNU 15.2. The
existing PBF pressure projection is the only scratch replacement. The build
uses the pinned native engine archive and existing unchanged test objects.
The Alpha lock, published or tracked files, and independent research were
preserved.

## Operator and coverage

For a face with unit outward normal n, planar point w, and distance
d(x)=(x-w)·n, the virtual source position is x′=x-2 d(x)n. A receiver
and donor must both be outside the same stationary face and have their
tangential kernel support contained by that finite face. Ambiguous multiple
face support is excluded. The radius candidate bound follows from
|xᵢ−x′ⱼ|² = |xᵢ−xⱼ|² + 4dᵢdⱼ, so no otherwise distant donor is admitted.
The plane may be in an arbitrary orientation. The reflected samples exist
only during density evaluation and do not enter the physical particle array,
water reconstruction, marker ownership, or persistence.

For one pressure row, the image contribution to the receiver gradient is
g = mⱼ∇W(xᵢ−x′ⱼ)/ρref and to the donor gradient is −R g, where
R=I−2nnᵀ. The self-image contributes (I−R)g once. The denominator includes
the combined direct and reflected gradient for each donor. With symmetric
face admission, the cross-row correction uses λᵢ+λⱼ, and the two reflected
lines of action meet at their intersection with the physical face. The
equal-and-opposite normal impulse is recorded there. Finite feature, pose
and time are deliberately only supported in this stationary-plane experiment.
The broader test uses equal masses; general unequal-mass projected constraints,
pose Jacobians, consistent mixed active sets, curved geometry, and cap
consistency are out of scope.

The exact Poly6 derivative and the original Spiky projection directions are
separate variants. The nominal rest-lattice control uses analytical density
normalization at the declared h/s=2.1. No independent per-particle damping or
dynamic update gate is introduced.

## Known-answer controls

An independently authored two-particle overcompression applies one pressure
iteration without smoothing, gravity, physical contact, or an active clamp.
Both aligned and oblique cases record 284.235 N·s of wall impulse and 8.14653
N·m·s of wall angular impulse. Combined particle plus wall momentum residual
is at most 8.38e−5 N·s and angular residual is at most 2.34e−5 N·m·s. Moving
the finite face away removes all four wall impulse rows in both orientations.
The independent pair proof uses `trial-v2.cpp`; initial `trial-v1.cpp`
placed aggregate normal reactions at receiver projections, which does not
generally preserve torque.

A separate 150-particle, 1,200 kg rectangular containment at 30 Hz exposes
a 9.81 m/s² load along the normal of a stationary broad face. The calibrated
mirror responds immediately and the integrated mean wall reaction is close
to the analytical −11,772 N. However, 240-step mean particle speeds are
0.19552 aligned and 0.41563 m/s rotated, compared to exact native
0.03182/0.03022 m/s. Calibration without mirrors gives 0.01907/0.02347
in the scratch implementation. Floor-only mirror admission removes side
image/stencil interactions but remains poor in the rotated fixture
(0.03299/0.17553 m/s). The calibrated exact Poly6 gradient has even larger
speeds and some particles escape over the tank rim. No minimum height is
claimed as a solver penetration result, since the finite containment is open.

An uncalibrated mirror increases the aligned zero-gravity first-step kinetic
energy from 45.24688 to 50.32231 J. With the analytical normalization both
mirror and no-mirror variants stay near rest. Completing the density kernel
at the wall therefore does not eliminate interior lattice quadrature bias.

## Original engine cases

| Original case | Mirrored and calibrated result |
| --- | --- |
| Zero-gravity rigid | All 727 strict checks pass, mean 1.13e−6 m/s at release |
| Aligned 0.25 m half-density | 2/2,176 mandatory late failures, plus one strict warmup failure; warmup .372132 m/s, late peak .740216 m/s, independent immersion .606054, historical four-second comparison fails at −.515516 m/s |
| Deep neutral player .20 m smoke | 2/492 mandatory failures, plus one strict warmup failure; mean .873775 m/s, bulk .730235 m/s and last-second centroid travel .073988 m |

The direct v1/corrected reaction v2 physical corrections are the same; changes
only add proper pressure reaction point reporting. v3 also adds a floor-only
admission option used by its named control. Raw trajectories were generated
using unchanged original fixtures, initial conditions, limits and timesteps.

## Conclusion

Kernel completion can represent a stationary flat pressure boundary and
exchange reaction without inventing wet material. The experiment rules out
simple mirrored support inside the existing split PBF projection as a
settling fix. Static force closure and particle pressure linear/angular
balance do not imply a quiet loaded pool. In particular, at the solver's
preferred stand-off, mirrored centre spacing is 2×0.36=0.72 of nominal
along the normal. Its discrete pressure row sees geometric compression when
the hard-contact geometry and kernel support have not been solved together.
That is a model mismatch to test with joint pressure/contact ordering, not
a justification for damping, deleting checks, or widening wet regions.

## Reproduction

From the repository root, place this directory under the existing ignored
`.cache/fluid-mirrored-wall-20261103/`. The build helper in this packet
comes from the independently archived quartic study and uses the Release
compiler flags plus native library at `.cache/fluid-zero-settling-kernel/clean/`.
Run `python3 .cache/fluid-mirrored-wall-20261103/make.py`. The current driver
generates v3 executables in that clean directory. The v1 driver predates
the witness, and the v2 driver generates the names without the v3 suffix.

- Run `mirror-native-baseline`, linked via the provided build helper using
  `link('judas_production_fluid_tests','mirror-native-baseline',halfspaceObject)`
  and no replacement object, to get the actual engine result.
- Run `mirror-halfspace-v3` with no variables, with
  `MIRROR_BOUNDARY=1`, with `MIRROR_CALIBRATE=1`, with both, and with
  both plus `MIRROR_EXACT_GRADIENT=1`. Add `MIRROR_FIRST_BOX_ONLY=1`
  to the calibrated mirror variant for floor-only.
- Run `mirror-witness-v3` with the three mirror/calibration/exact variables.
- Run `mirror-production-v3 --case zero_gravity --strict-diagnostics --output <fresh>`
  and repeat for `half_025`, with mirror and calibration only.
- Run `mirror-player-v3 --smoke --smoke-spacing .2 --strict-diagnostics --output <fresh>`
  with mirror and calibration only.

Recorded logs and compressed trajectories contain the originally run v1/v2
or v3 controls. No candidate is authorized for engine integration.
