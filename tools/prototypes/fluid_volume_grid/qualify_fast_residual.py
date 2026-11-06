#!/usr/bin/env python3
"""Independent exact dyadic oracle for the filtered audit (not runtime maths)."""
import fractions
import math
import random
import subprocess
import sys

rng = random.Random(761106)
fixtures = []
for sample in range(120):
    def value():
        return math.ldexp(.5 + rng.random() / 2, rng.randint(-500, 500))
    cells = []
    for _ in range(7):
        c, o, b, x = (value() for _ in range(4))
        cells.append([c, o, math.ldexp(o, -54), b, -math.ldexp(b, -54), x, math.ldexp(x, -54), 1e300 if sample % 2 else 1e-25])
    edges = [[rng.randrange(7), rng.randrange(7), value(), 0.] for _ in range(32)]
    fixtures.append((cells, edges))
# Product underflow and exact cancellation are explicit controls too.
fixtures += [([[1., 0., 0., 0., 0., 5e-324, 0., 1.]], []),
             ([[1., 0., 0., 1., 0., 1., 0., 1.]], [])]
text = [str(len(fixtures))]
for cells, edges in fixtures:
    text.append(f'{len(cells)} {len(edges)}')
    text.extend(' '.join(map(str, row)) for row in cells + edges)
proc = subprocess.run([sys.argv[1]], input='\n'.join(text)+'\n', text=True, capture_output=True, check=True)
lines = iter(proc.stdout.splitlines())
checks = 0
F = fractions.Fraction.from_float
for cells, edges in fixtures:
    assert next(lines) == '1', 'finite supported dyadic fixture rejected'
    residual = [F(bh)+F(bl)-F(c)*(F(xh)+F(xl))-(F(oh)+F(ol))*(F(xh)+F(xl)) for c,oh,ol,bh,bl,xh,xl,_ in cells]
    for a,b,qh,ql in edges:
        q=(F(qh)+F(ql))*(F(cells[a][5])+F(cells[a][6]))
        residual[a]-=q
        residual[b]+=q
    for exact in residual:
        lo,hi=map(float,next(lines).split())
        assert F(lo)<=exact<=F(hi), 'filtered interval excluded exact residual'
        checks+=1
assert next(lines, None) is None
print(f'{checks} exact rational enclosure checks, 0 failures')
