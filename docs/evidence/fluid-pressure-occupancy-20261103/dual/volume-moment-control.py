"""Independent mass/height bound and replay of the nominal PBF density estimate."""
from collections import defaultdict
from itertools import product
import csv
import math
from pathlib import Path
import statistics

root=Path(__file__).resolve().parents[1]
samples=(
    ("baseline",root/"fluid-zero-settling-kernel/clean/player-baseline-strict"),
    ("rediscovery",root/"fluid-static-balanced-20261103/player-discover-0.20"),
)
A=2.2**2
rest=1000
h=.42
neighbors=tuple(product((-1,0,1),repeat=3))
for name,path in samples:
    with (path/"density-20-1000-warmup-particles.csv").open() as stream:
        particles=[tuple(float(row[k]) for k in ("x","y","z","mass")) for row in csv.DictReader(stream)]
    total=math.fsum(row[3] for row in particles)
    centroid=math.fsum(y*m for x,y,z,m in particles)/total
    height=total/(rest*A)
    assert all(abs(x)<=1.1 and abs(z)<=1.1 and 0<=y<6 for x,y,z,m in particles)
    # Fill the lowest available slabs at rest density. This is the minimum
    # center of mass over all free-surface shapes in the sealed cross-section.
    minimum_centroid=height/2
    implied_peak_lower_bound=total/(2*A*centroid)

    bins=defaultdict(list)
    for row in particles:
        bins[tuple(math.floor(v/h) for v in row[:3])].append(row)
    estimate=[]
    C=315/(64*math.pi*h**9)
    for x,y,z,m in particles:
        i,j,k=map(lambda a: math.floor(a/h),(x,y,z))
        value=0.
        for di,dj,dk in neighbors:
            for xx,yy,zz,mm in bins[i+di,j+dj,k+dk]:
                r2=(x-xx)**2+(y-yy)**2+(z-zz)**2
                if r2<h*h:
                    value+=C*(h*h-r2)**3*mm
        estimate.append(value)
    print(f"{name} samples={len(particles)} mass={total:.9f} kg volume={total/rest:.9f} m3")
    print(f"  centroid={centroid:.9f} m, minimum uniform-density centroid={minimum_centroid:.9f} m")
    print(f"  continuous peak density must be at least {implied_peak_lower_bound:.9f} kg/m3")
    print(f"  PBF point estimator mean {statistics.fmean(estimate):.9f} max {max(estimate):.9f} kg/m3")
    print(f"  highest particle {max(row[1] for row in particles):.9f} m, full-width uniform height {height:.9f} m")
