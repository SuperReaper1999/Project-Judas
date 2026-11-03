"""Connected components with a 2 mm diagnostic link radius."""
from collections import Counter, defaultdict
from itertools import product
from pathlib import Path
import csv
import math
import sys

link = .002
for csv_path in map(Path, sys.argv[1:]):
    with csv_path.open() as handle:
        points = [
            tuple(float(row[axis]) for axis in ('x', 'y', 'z'))
            for row in csv.DictReader(handle)
        ]
    buckets = defaultdict(list)
    parents = list(range(len(points)))

    def root(i):
        while parents[i] != i:
            parents[i] = parents[parents[i]]
            i = parents[i]
        return i

    for i, p in enumerate(points):
        bucket = tuple(math.floor(x / link) for x in p)
        for shift in product((-1, 0, 1), repeat=3):
            adjacent = tuple(a + b for a, b in zip(bucket, shift))
            for j in buckets.get(adjacent, ()):
                if math.dist(p, points[j]) < link:
                    parents[root(i)] = root(j)
        buckets[bucket].append(i)

    sizes = Counter(root(i) for i in range(len(points)))
    print(csv_path.parent.name, sorted(Counter(sizes.values()).items()))
