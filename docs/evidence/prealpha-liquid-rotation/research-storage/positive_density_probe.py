"""Independent exact-arithmetic algebra check; imports no research or engine code."""
from fractions import Fraction as F
from itertools import combinations_with_replacement
from pathlib import Path
import hashlib
import json
import math


def density_controls(h, volume=F(1)):
    a, b, c, d = h
    scale = 3 * volume / (d - a)
    spans = []
    if a < b:
        spans.append((a, b, (F(0), F(0), scale * (b-a)/(c-a))))
    if b < c:
        spans.append((b, c, (scale*(b-a)/(c-a), scale,
                               scale*(d-c)/(d-b))))
    if c < d:
        spans.append((c, d, (scale*(d-c)/(d-b), F(0), F(0))))
    return spans


def cdf_recurrence(q, h):
    f = [F(q >= x) for x in h]
    for degree in range(1, 4):
        for i in range(4-degree):
            lo, hi = h[i], h[i+degree]
            if q <= lo:
                f[i] = F(0)
            elif q >= hi:
                f[i] = F(1)
            else:
                f[i] = ((q-lo)*f[i]+(hi-q)*f[i+1])/(hi-lo)
    return f[0]


def integrated(d, t, width):
    # Integral of the Bernstein quadratic, evaluated as its exact cubic.
    return width * (d[0]*t + (d[1]-d[0])*t*t
                    + (d[0]-2*d[1]+d[2])*t*t*t/3)


def restricted(d, a, b):
    def blossom(x, y):
        return ((1-x)*(1-y)*d[0] + ((1-x)*y+x*(1-y))*d[1]
                + x*y*d[2])
    return blossom(a, a), blossom(a, b), blossom(b, b)


def bezier(controls, t):
    work = list(controls)
    while len(work) > 1:
        work = [(1-t)*a+t*b for a, b in zip(work, work[1:])]
    return work[0]


heights = [tuple(map(F, h)) for h in combinations_with_replacement(range(7), 4)
           if h[0] < h[-1]]
# Actual binary64 coordinates, including adjacent values with no interior double.
floating = [
    (0., 1., math.nextafter(1., math.inf), 2.),
    (0., math.nextafter(0., 1.), 1., 2.),
    (-1e6, -math.nextafter(1e6, 0), 1e6, math.nextafter(1e6, math.inf)),
    (1., math.nextafter(1., math.inf), math.nextafter(math.nextafter(1., math.inf), math.inf), 2.),
    (0., 0., 0., 1.), (0., 0., 1., 1.), (0., 1., 1., 1.),
]
heights += [tuple(F(x) for x in h) for h in floating]
counts = {"height_packets": len(heights), "positive_control_checks": 0,
          "exact_cdf_checks": 0, "exact_mass_checks": 0,
          "restriction_checks": 0, "rounded_prefix_checks": 0,
          "exact_derivative_checks": 0, "exact_energy_primitive_checks": 0}
for h in heights:
    cumulative = F(0)
    rounded_prefix = float(cumulative)
    for a, b, d in density_controls(h):
        assert all(x >= 0 for x in d)
        counts["positive_control_checks"] += len(d)
        width = b-a
        volume = (cumulative, cumulative+width*d[0]/3,
                  cumulative+width*(d[0]+d[1])/3,
                  cumulative+width*sum(d)/3)
        primitive = [F(0)]
        for control in volume:
            primitive.append(primitive[-1]+control/4)
        for k in range(1, 16):
            t = F(k, 16)
            assert cumulative + integrated(d, t, width) == cdf_recurrence(a+width*t, h)
            counts["exact_cdf_checks"] += 1
            assert (bezier(tuple(3*(y-x)/width for x, y in zip(volume, volume[1:])), t)
                    == bezier(d, t))
            counts["exact_derivative_checks"] += 1
            independent_integral = (cumulative*t + width*d[0]*t*t/2
                                    + width*(d[1]-d[0])*t*t*t/3
                                    + width*(d[0]-2*d[1]+d[2])*t*t*t*t/12)
            assert bezier(primitive, t) == independent_integral
            counts["exact_energy_primitive_checks"] += 1
        for x, y in ((F(0), F(1, 2)), (F(1, 2), F(1)), (F(1, 5), F(4, 5))):
            child = restricted(d, x, y)
            assert all(v >= 0 for v in child)
            assert (width*(y-x)*sum(child)/3
                    == integrated(d, y, width)-integrated(d, x, width))
            counts["restriction_checks"] += 1
        cumulative += width*sum(d)/3
        assert float(cumulative) >= rounded_prefix
        rounded_prefix = float(cumulative)
        counts["rounded_prefix_checks"] += 1
    assert cumulative == 1
    counts["exact_mass_checks"] += 1

result = {
    "status": "PASS", "classification": "independent exact-arithmetic algebra controls, not engine or continuum acceptance",
    "method": "Positive closed-form quadratic Bernstein density, exact primitive versus independent CDF recurrence, exact mass and de Casteljau restriction",
    "counts": counts,
    "research_executed_or_imported": False,
    "source_sha256": hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
}
Path(__file__).with_name("POSITIVE_DENSITY_RESULTS.json").write_text(json.dumps(result, indent=2)+"\n")
print(json.dumps(result, indent=2))
