# Admission attempt costs

All rows reject; no speedup or usable-fluid-step claim. Three samples per case; p95 equals worst.
Geometry compilation is outside each attempt timer. The probe holds the body fixed and uses the preserved fixture, native float timestep/gravity and explicit top gas vent.

| Basin | Owners | Interval (s) | Mean ms | Median ms | p95 / worst ms | Admitted |
| --- | --- | ---: | ---: | ---: | ---: | ---: |
| Level | Original cuts | 0.01666666754 | 144.933 | 144.770 | 145.433 | 0/3 |
| Level | Original cuts | 0.0001666666754 | 40.931 | 40.902 | 40.992 | 0/3 |
| Level | Original cuts | 1.666666754e-06 | 17.015 | 16.971 | 17.117 | 0/3 |
| Level | Rectangular | 0.01666666754 | 135.573 | 135.539 | 135.685 | 0/3 |
| Level | Rectangular | 0.0001666666754 | 41.867 | 41.857 | 41.924 | 0/3 |
| Level | Rectangular | 1.666666754e-06 | 13.703 | 13.717 | 13.745 | 0/3 |
| Rotated | Original cuts | 0.01666666754 | 394.434 | 393.336 | 397.258 | 0/3 |
| Rotated | Original cuts | 0.0001666666754 | 83.597 | 83.598 | 83.753 | 0/3 |
| Rotated | Original cuts | 1.666666754e-06 | 34.371 | 34.189 | 34.878 | 0/3 |
| Rotated | Rectangular | 0.01666666754 | 7115.625 | 7111.615 | 7134.439 | 0/3 |
| Rotated | Rectangular | 0.0001666666754 | 103.398 | 103.593 | 103.602 | 0/3 |
| Rotated | Rectangular | 1.666666754e-06 | 31.746 | 31.714 | 31.931 | 0/3 |

Process peak RSS: **89,288 KiB** (87.20 MiB). This includes chart/state copies, not an engine renderer or GPU allocations.

The original hydroacoustic adapter rejects before solving, so its baseline timing is not a functionally equivalent performance comparison. Rectangular ownership is a representation experiment, not an identical-discretization speedup comparison.
