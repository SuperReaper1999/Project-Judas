# Deep player pool pressure and footprint replay

Both inputs are the original pre-release particle CSVs at fixed step 240
from the .20 m deep player fixture. Every 8 kg particle contributes
.008 m³ of material at rest density 1,000 kg/m³. The player's center
comes from its actual spawn in each original results.json. Engine
hydrostatic sampling uses a box of half-extents (.3, .9, .3) for this
non-displacing player. The standalone observer measures both that box
and the player's actual capsule. The pool contains 2,500 particles and
20,000 kg in both cases.

| At the own spawn | Baseline | Pressure and wall rediscovery |
| --- | ---: | ---: |
| First runtime immersion | 1.000 | .41047 |
| Local box particle-center mass | 664 kg | 728 kg |
| Uniform filled box expected mass | 648 kg | 648 kg |
| Mean Poly6 density across the box | 1005.605 | 1103.556 kg/m³ |
| Minimum Poly6 density across the box | 968.927 | 967.523 kg/m³ |
| Union of .2 m material cubes in the box | .85881 | .29381 |
| Union of equal-volume spheres in the box | .88194 | .29429 |
| Median particle nearest neighbor distance in the box | .198067 m | .000051413 m |
| Number of particles with another center within 2 mm | 0/83 | 91/91 |
| Poly6 density range at particle centers | 980.313–1027.047 | 999.866–1000.168 kg/m³ |

Geometric coverage uses midpoint quadrature at both 2 cm and 1 cm
resolution. Swapping spawn centers leaves the coverage difference.
Cube and sphere unions are independent geometric indicators for overlap
and holes. They are not a replacement wet solver or an estimate of exact
continuum wet volume. Particle-center mass and the broad Poly6 field
both rule out bulk mass evacuation. The 2 mm clustering is pervasive
at every height, although the candidate's sampled density is extremely
close to its numerical target. The runtime wet query is reacting to an
input with overlapping material footprints and gaps. Extending the
query to classify the whole smoothed field as wet would conflate those
gaps with genuine dry gaps.

The independent analytic counterexample arranges four coincident particles
at each simple cubic site. With exactly six neighboring sites in support,
`rho = 4 m [W(0) + 6 W(a)]`. At `a=.317029291310 m` this gives exactly
1,000 kg/m³ at every particle and 1,004.273 kg/m³ averaged over one
lattice cell. The represented cube union covers .251068 of the lattice
cell volume. Zero velocity solves the pressure inequality in zero gravity.
There is no intra-cluster density derivative to select a direction of
separation. This establishes a spatial representation ambiguity even for
a fully converged velocity projection.

Reproduce the standalone snapshot analysis from the repository root:

```sh
d=.cache/fluid-static-balanced-20261103/deep-geometry
g++ -O3 -std=c++17 -Wall -Wextra -Wpedantic "$d/measure.cpp" -o "$d/measure"
bash "$d/reproduce.sh"
python3 "$d/four-cluster.py"
```

Snapshot file hashes, two grid densities, both spawn positions and
component counts are preserved with the observer. The original input
snapshots remain in their own output directories.
