"""Interior sampled density on equal-volume cubic parcels, independent of engine."""
from itertools import product
import math

def density(radius_to_spacing, degree, spacing_scale=1.0):
    h = radius_to_spacing
    n = math.ceil(h / spacing_scale)
    coefficient = {3:315/64, 4:3465/512}[degree] / (math.pi * h**3)
    return coefficient * math.fsum(
        max(0, 1 - (i*i+j*j+k*k) * (spacing_scale/h)**2)**degree
        for i,j,k in product(range(-n,n+1), repeat=3)
    )

for h in (1.5,1.75,2,2.05,2.1,2.15,2.2,2.4,2.6,3,3.5,4):
    print(f"h/s={h:.2f}",
          f"poly6_bias={density(h,3)-1:+.9f}",
          f"quartic_bias={density(h,4)-1:+.9f}",
          f"poly6_5pct_compression={density(h,3,.95)-1:+.9f}",
          f"quartic_5pct_compression={density(h,4,.95)-1:+.9f}")
