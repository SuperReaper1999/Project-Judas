# Vented reservoirs and a thin wet aperture

This bounded liquid-only control uses two vented, constant-section
reservoirs joined by one fully wet rectangular throat. The two reservoir
columns are quasi-hydrostatic. The throat retains its finite liquid inertia.
The atmospheric interface supplies a zero gauge pressure boundary at each
reservoir surface. There are no simulated gas volumes or air donor budgets.

The connected liquid carries volume between the reservoirs. For aperture
width `w`, gap height `b`, link length `L`, area `S=w*b`, reservoir surface
areas `A_i`, and reservoir volumes `V_i`, the inviscid reduction is

```
H_i = V_i / A_i
p_left - p_right = rho*g*(H_left-H_right)
q = S*u
dV_left/dt = -q, dV_right/dt = q
(rho*S*L)*du/dt = S*(p_left-p_right)
```

The port pressures are measured at the same elevation, so the port height
cancels in their difference. An implicit midpoint interval applies the
same `dt*q_mid` to both reservoirs and the same midpoint head to the
throat impulse. Independent potential and kinetic energy calculations
check `delta E_potential = -delta K = -(p_left-p_right)_mid * dt*q_mid`.
The analytical linear oscillator has frequency
`sqrt(g*S*(1/A_left+1/A_right)/L)` and supplies the trajectory oracle.

## Results

The common fixture has `w=2 m`, `L=.125 m`, the same initially `0.5 m`
head difference, and zero initial throat velocity for the aperture sweep.
Equal heads stay exactly stationary even with unequal reservoir cross
sections.

| Gap height (m) | First midpoint flow (m³/s) | Transit CFL |
| ---: | ---: | ---: |
| 0.001 | 6.539857431108e-4 | about 0.0436 |
| 0.0005 | 3.269964357389e-4 | 0.0435995247652 |
| 0.00025 | 1.634991089299e-4 | 0.0435997623813 |
| 2^-32 | 1.522712409489e-10 | 0.0435999999998 |
| 0, matched sealed control | exactly 0 | exactly 0 |

The area tends to zero and the fluid speed has a finite limit, so the
predicted volume flux vanishes linearly in this **inviscid fixed-length**
model. This is not a claim that real viscous slit flow obeys the same
asymptotics. The liquid transit ratio `abs(dt*q_mid)/(S*L)` tends to a
finite value instead of vanishing. A very narrow wet link at `10 m/s`
rejects the original interval and admits its half-interval at CFL
`0.67756666666580689`. An independent one-second trajectory at 15 steps
rejects at zero-based index 12 for the same reason. The raw first failed
attempt is preserved. The admitted convergence sequence retains the
same physical system and changes only the interval.

| Steps | Speed error (m/s) | Head error (m) | Maximum transit CFL |
| ---: | ---: | ---: | ---: |
| 30 | 5.68866e-5 | 2.71933e-7 | 0.6150 |
| 60 | 1.42222e-5 | 6.79866e-8 | 0.3097 |
| 120 | 3.55557e-6 | 1.69968e-8 | 0.1554 |
| 240 | 8.88895e-7 | 4.24922e-9 | 0.0778 |

Both variables converge at second order. The independent one-second
total-volume and mechanical energy checks use `2e-15 m³` and `2e-11 J`
tolerances, respectively. Unvented air and a partly exposed port are
explicit unsupported states and reject without publication.

This gate distinguishes a physically wet transit restriction from the
original dry-air donor failure. It does not qualify a grid pressure graph,
local liquid injection, arbitrary cut-cell wetting or dewetting, gas
entrapment, or the authored 3D pool. The unchanged mixed-pool failures remain
in `../static-wall-union/pressure-gate/`. No tracked source was changed.

## Reproduction

Run from the repository root:

```sh
d=.cache/fluid-moving-grid-20261103/atmospheric-reservoir-control
g++ -std=c++17 -O3 -Wall -Wextra -Wpedantic -Werror "$d/AtmosphericReservoir.cpp" -o "$d/reservoir"
"$d/reservoir"
g++ -std=c++17 -O1 -g -Wall -Wextra -Wpedantic -Werror \
  -fsanitize=address,undefined -fno-omit-frame-pointer -no-pie \
  "$d/AtmosphericReservoir.cpp" -o "$d/reservoir-asan"
ASAN_OPTIONS=detect_leaks=0 "$d/reservoir-asan"
```
