"""Two-node wet pressure graph with exact stepped cut storage and a vented surface."""
import math
from dataclasses import dataclass

RHO_G = 9810.0
REF = 0.5


@dataclass(frozen=True)
class Storage:
    lower_area: float
    upper_area: float
    break_height: float = 0.5
    top: float = 1.0

    @property
    def below(self):
        return self.lower_area * self.break_height

    @property
    def capacity(self):
        return self.below + self.upper_area * (self.top - self.break_height)

    def volume(self, height):
        return self.lower_area * min(height, self.break_height) + (
            self.upper_area * max(height - self.break_height, 0.)
        )

    def height(self, volume):
        return volume / self.lower_area if volume <= self.below else (
            self.break_height + (volume - self.below) / self.upper_area
        )

    def mean_head(self, first, last):
        lo, hi = sorted((first, last))
        if lo == hi:
            return self.height(lo) - REF
        if lo < self.below < hi:
            lo_head, hi_head = self.height(lo), self.height(hi)
            lower = (self.below - lo) * ((lo_head + self.break_height) / 2 - REF)
            upper = (hi - self.below) * ((hi_head + self.break_height) / 2 - REF)
            return (lower + upper) / (hi - lo)
        return (self.height(lo) + self.height(hi)) / 2 - REF

    def independent_energy(self, volume):
        h = self.height(volume)
        low_h = min(h, self.break_height)
        low = self.lower_area * (low_h * low_h / 2 - REF * low_h)
        high = self.upper_area * max(h - self.break_height, 0.) * (
            (h + self.break_height) / 2 - REF
        )
        return RHO_G * (low + high)


def advance(store, fixed, displaced, k, k_surface, guess, vented=True):
    if not vented:
        return None
    # q leaves left. The interface sweeps physical volume s on each side.
    left, right = store
    q_min = max(displaced[0] + fixed[0] - left.capacity, -displaced[1] - fixed[1])
    q_max = min(displaced[0] + fixed[0], right.capacity - fixed[1] - displaced[1])
    if q_min > q_max:
        return None

    def evaluate(q):
        s = (displaced[0] - q, displaced[1] + q)
        after = (fixed[0] + s[0], fixed[1] + s[1])
        p_atm = tuple(RHO_G * a.mean_head(v, w) for a, v, w in zip(store, fixed, after))
        p = tuple(b + (v - u) / c for b, v, u, c in zip(p_atm, s, guess[1:], k_surface))
        return q - guess[0] - k * (p[0] - p[1]), s, after, p

    if evaluate(q_min)[0] > 0 or evaluate(q_max)[0] < 0:
        return None
    low, high = q_min, q_max
    for _ in range(100):
        q = (low + high) / 2
        value, _, _, _ = evaluate(q)
        if value == 0:
            low = high = q
            break
        if value > 0:
            high = q
        else:
            low = q
    q = (low + high) / 2
    error, s, after, p = evaluate(q)
    coefficients = (k, *k_surface)
    result = (q, *s)
    kinetic = math.fsum((v*v-u*u)/(2*c) for v, u, c in zip(result, guess, coefficients))
    dissipation = math.fsum((v-u)**2/(2*c) for v, u, c in zip(result, guess, coefficients))
    potential = math.fsum(a.independent_energy(v)-a.independent_energy(u)
                          for a, u, v in zip(store, fixed, after))
    wall_work = math.fsum(v*u for v, u in zip(p, displaced))
    return q, s, after, error, kinetic, dissipation, potential, wall_work


a = Storage(.83, 1.4)
b = Storage(1.19, .7)
store = (a, b)
surface = (2e-4, 3e-4)
rest = (a.volume(REF), b.volume(REF))
q, s, after, eq, d_k, loss, d_u, work = advance(store, rest, (0., 0.), 4e-5, surface, (0., 0., 0.))
assert abs(q) < 1e-18 and max(map(abs, s)) < 1e-18 and after == rest
print('equilibrium', q, s, eq)

# Lateral transfer crosses the stepped storage geometry without resetting the pressure
# or clipping away the atmospheric interface displacement.
initial = (a.volume(.507), b.volume(.493))
q, s, after, eq, d_k, loss, d_u, work = advance(store, initial, (0., 0.), 4e-5, surface, (.045, 0., 0.))
assert q > .01 and a.height(after[0]) < REF < b.height(after[1])
assert abs(math.fsum(after)-math.fsum(initial)) < 1e-15
assert abs(work - (d_k+loss+d_u)) < 1e-11 and abs(eq) < 1e-13
print('wet storage transition', q, s, 'head', a.height(after[0]), b.height(after[1]),
      'mass_residual', math.fsum(after)-math.fsum(initial), 'work_residual_J', work-(d_k+loss+d_u))

# A moving solid displaces 0.02 m³ of water with no modeled gas volume or gas pressure.
q, s, after, eq, d_k, loss, d_u, work = advance(store, rest, (.02, 0.), 4e-5, surface, (0., 0., 0.))
assert q > 0 and all(v > 0 for v in s)
assert abs(math.fsum(after) - (math.fsum(rest) + .02)) < 1e-15
assert abs(work - (d_k+loss+d_u)) < 1e-11 and abs(eq) < 1e-13
print('wall source', q, s, 'old/new water', math.fsum(rest)+.02, math.fsum(after),
      'work_J', work, 'work_residual_J', work-(d_k+loss+d_u))

# With finite heads the liquid velocity has a finite thin-aperture limit.
initial = (a.volume(.7), b.volume(.3))
for k in (4e-6, 4e-8, 4e-10):
    q, s, after, eq, d_k, loss, d_u, work = advance(store, initial, (0., 0.), k, surface, (0., 0., 0.))
    print('wet slit', k, q, 'ratio', q/k, 'work_residual_J', work-(d_k+loss+d_u))
    assert 3500 < q/k < 4000 and abs(work-(d_k+loss+d_u)) < 1e-11

assert advance(store, rest, (0., 0.), 4e-5, surface, (0., 0., 0.), vented=False) is None
assert advance(store, rest, (3., 0.), 4e-5, surface, (0., 0., 0.)) is None
print('unvented component and over-capacity geometric source rejected')
